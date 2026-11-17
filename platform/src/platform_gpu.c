/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file platform_gpu.c
 * @brief 加速器能力探测实现（0.1.21 B-0）
 *
 * 三层探测按「硬件 → 端点可达 → /v1/models 应答」逐层叠加，任一层失败
 * 即止步于上一层并把原因写进 reason。所有失败路径都以 R3 + 原因表达，
 * 不返回错误码——「没有可用加速器」是合法结论，不是异常（§12.3 三值）。
 */

#include "platform_gpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <dirent.h>
#include <unistd.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

#define GPU_EP_DEFAULT "127.0.0.1"
#define GPU_EP_PORT    8000
#define GPU_EP_MS      200

/* 虚拟显示/渲染设备：会出现在 /dev/dri 与 sysfs，但不提供算力。 */
static int gpu_is_virt(const char *drv)
{
    if (!drv || !drv[0])
        return 0;
    return strcmp(drv, "vgem") == 0 || strcmp(drv, "vkms") == 0 ||
           strcmp(drv, "virtio_gpu") == 0 || strcmp(drv, "virtio-gpu") == 0;
}

airy_accel_kind_t airy_gpu_class(uint16_t vendor, const char *driver)
{
    if (gpu_is_virt(driver))
        return AIRY_ACCEL_VIRTUAL;
    switch (vendor) {
    case 0x10de:
        return AIRY_ACCEL_NVIDIA;
    case 0x1002:
        return AIRY_ACCEL_AMD;
    case 0x8086:
        return AIRY_ACCEL_INTEL;
    case 0x0000:
        return AIRY_ACCEL_NONE;
    default:
        return AIRY_ACCEL_OTHER;
    }
}

static const char *gpu_kind_name(airy_accel_kind_t k)
{
    switch (k) {
    case AIRY_ACCEL_NVIDIA:
        return "nvidia";
    case AIRY_ACCEL_AMD:
        return "amd";
    case AIRY_ACCEL_INTEL:
        return "intel";
    case AIRY_ACCEL_OTHER:
        return "other";
    case AIRY_ACCEL_VIRTUAL:
        return "virtual";
    default:
        return "none";
    }
}

#if !defined(_WIN32) && !defined(__APPLE__)

/* sysfs 十六进制属性（vendor/device）读取；缺失按 0 处理。 */
static uint16_t gpu_id16(const char *path)
{
    char line[16];
    FILE *fp = fopen(path, "r");
    unsigned long v;

    if (!fp)
        return 0;
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    v = strtoul(line, NULL, 16);
    return (uint16_t)(v & 0xffffu);
}

/* device/driver 是符号链接，末段即内核驱动名（i915/xe/amdgpu/nvidia/vgem）。 */
static void gpu_drv_name(const char *devdir, char *out, size_t cap)
{
    char path[256];
    char buf[256];
    ssize_t n;

    out[0] = '\0';
    snprintf(path, sizeof(path), "%s/driver", devdir);
    n = readlink(path, buf, sizeof(buf) - 1);
    if (n <= 0)
        return;
    buf[n] = '\0';
    const char *base = strrchr(buf, '/');
    base = base ? base + 1 : buf;
    snprintf(out, cap, "%s", base);
}

/* 内核模块版本（/sys/module/<drv>/version）；不提供该属性的驱动留空。 */
static void gpu_drv_ver(const char *drv, char *out, size_t cap)
{
    char path[128];
    char line[64];
    FILE *fp;

    out[0] = '\0';
    if (!drv || !drv[0])
        return;
    snprintf(path, sizeof(path), "/sys/module/%s/version", drv);
    fp = fopen(path, "r");
    if (!fp)
        return;
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        snprintf(out, cap, "%s", line);
    }
    fclose(fp);
}

/* 专用显存总量（amdgpu/xe 暴露 mem_info_vram_total）；不可得返回 0，
 * 表示「未知」而非「没有」——iGPU 共享内存与 NVIDIA 专有驱动均属此类。 */
static uint64_t gpu_vram(const char *devdir)
{
    char path[384];
    char line[32];
    unsigned long long v = 0;
    FILE *fp;

    snprintf(path, sizeof(path), "%s/mem_info_vram_total", devdir);
    fp = fopen(path, "r");
    if (!fp)
        return 0;
    if (fgets(line, sizeof(line), fp))
        v = strtoull(line, NULL, 10);
    fclose(fp);
    return (uint64_t)v;
}

/* 只接受 cardN（排除 card0-DP-1 这类连接器节点）。 */
static int gpu_is_card(const char *name)
{
    if (strncmp(name, "card", 4) != 0)
        return 0;
    for (const char *p = name + 4; *p; p++)
        if (*p < '0' || *p > '9')
            return 0;
    return name[4] != '\0';
}

static uint32_t gpu_scan_drm(airy_gpu_cap_t *cap, uint32_t *virt)
{
    DIR *d = opendir("/sys/class/drm");
    struct dirent *e;

    *virt = 0;
    if (!d)
        return 0;

    while ((e = readdir(d)) != NULL) {
        char devdir[320];
        char vpath[360];
        char dpath[360];
        char drv[32];
        uint16_t vendor;
        uint16_t device;

        if (!gpu_is_card(e->d_name))
            continue;
        snprintf(devdir, sizeof(devdir), "/sys/class/drm/%s/device", e->d_name);
        snprintf(vpath, sizeof(vpath), "%s/vendor", devdir);
        snprintf(dpath, sizeof(dpath), "%s/device", devdir);
        vendor = gpu_id16(vpath);
        if (vendor == 0)
            continue;
        device = gpu_id16(dpath);
        gpu_drv_name(devdir, drv, sizeof(drv));

        if (gpu_is_virt(drv)) {
            (*virt)++;
            continue;
        }
        if (cap->dev_count >= AIRY_GPU_MAX_DEVS)
            continue;
        {
            airy_gpu_dev_t *dev = &cap->devs[cap->dev_count];
            __builtin_memset(dev, 0, sizeof(*dev));
            dev->kind = airy_gpu_class(vendor, drv);
            dev->vendor_id = vendor;
            dev->device_id = device;
            snprintf(dev->driver, sizeof(dev->driver), "%s", drv);
            gpu_drv_ver(drv, dev->driver_ver, sizeof(dev->driver_ver));
            dev->vram_bytes = gpu_vram(devdir);
            snprintf(dev->model, sizeof(dev->model), "%s %s %04x:%04x",
                     gpu_kind_name(dev->kind), drv, vendor, device);
            cap->dev_count++;
        }
    }
    closedir(d);
    return cap->dev_count;
}

/* 非阻塞 connect + select：探测不得让 CLI/守护卡住（上限 GPU_EP_MS）。 */
static int gpu_ep_conn(const char *host, int port)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char svc[8];
    int fd = -1;

    __builtin_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(svc, sizeof(svc), "%d", port);
    if (getaddrinfo(host, svc, &hints, &res) != 0)
        return -1;

    for (struct addrinfo *ai = res; ai && fd < 0; ai = ai->ai_next) {
        struct timeval tv;
        fd_set wr;
        int rc;

        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc != 0 && errno != EINPROGRESS) {
            close(fd);
            fd = -1;
            continue;
        }
        FD_ZERO(&wr);
        FD_SET(fd, &wr);
        tv.tv_sec = 0;
        tv.tv_usec = GPU_EP_MS * 1000;
        if (select(fd + 1, NULL, &wr, NULL, &tv) <= 0) {
            close(fd);
            fd = -1;
            continue;
        }
        {
            int err = 0;
            socklen_t len = sizeof(err);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0 || err != 0) {
                close(fd);
                fd = -1;
            }
        }
    }
    freeaddrinfo(res);
    return fd;
}

/* L3：GET /v1/models，状态行 200 且响应体含 data 字段才算通过。 */
static int gpu_ep_models(const char *host, int port)
{
    static const char req[] = "GET /v1/models HTTP/1.0\r\nHost: x\r\n\r\n";
    char buf[512];
    int fd = gpu_ep_conn(host, port);
    ssize_t n;

    if (fd < 0)
        return 0;
    {
        struct timeval tv = { 0, GPU_EP_MS * 1000 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
    if (send(fd, req, sizeof(req) - 1, 0) <= 0) {
        close(fd);
        return 0;
    }
    n = recv(fd, buf, sizeof(buf) - 1, 0);
    close(fd);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    return strstr(buf, " 200") != NULL && strstr(buf, "\"data\"") != NULL;
}

static void gpu_detect(airy_gpu_cap_t *cap)
{
    const char *ep = "127.0.0.1";
    int port = GPU_EP_PORT;
    uint32_t virt = 0;
    int l3;

    if (gpu_scan_drm(cap, &virt) == 0) {
        if (virt > 0)
            snprintf(cap->reason, sizeof(cap->reason),
                     "仅检出 %u 个虚拟显示设备（vgem/vkms/virtio），不计入算力", virt);
        else
            snprintf(cap->reason, sizeof(cap->reason),
                     "sysfs/drm 未检出 PCI 加速器（平台不适用或容器未透传）");
        cap->verdict = AIRY_GPU_R3;
        return;
    }
    cap->layers |= AIRY_GPU_L1_HW;

    l3 = gpu_ep_models(ep, port);
    if (l3) {
        cap->layers |= AIRY_GPU_L2_RUNTIME | AIRY_GPU_L3_OPENAI;
        snprintf(cap->backend, sizeof(cap->backend), "%s:%d", ep, port);
        cap->verdict = (cap->devs[0].kind == AIRY_ACCEL_NVIDIA) ? AIRY_GPU_R1 : AIRY_GPU_R2;
        return;
    }
    cap->verdict = AIRY_GPU_R3;
    snprintf(cap->reason, sizeof(cap->reason),
             "加速器在，但 %s:%d 无 OpenAI 兼容应答；本机不得宣称 GPU 服务", ep, port);
}

#else /* _WIN32 || __APPLE__ */

static void gpu_detect(airy_gpu_cap_t *cap)
{
    snprintf(cap->reason, sizeof(cap->reason),
             "当前平台未接入加速器探测后端（不适用，显式 R3，非静默降级）");
    cap->verdict = AIRY_GPU_R3;
}

#endif

int airy_gpu_probe(airy_gpu_cap_t *cap)
{
    if (!cap)
        return AIRY_EINVAL;
    __builtin_memset(cap, 0, sizeof(*cap));
    gpu_detect(cap);
    return AIRY_SUCCESS;
}
