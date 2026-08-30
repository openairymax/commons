// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform.c
 * @brief Cross-platform basic tooling domain: network init, atomics,
 * socket communication, time & random, filesystem and string helpers.
 *
 * Provides a unified cross-platform abstraction layer:
 * - Socket network communication
 * - Atomic operations
 * - Time and random numbers
 * - Filesystem operations
 * - String and error helpers
 */

#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
/* WIN32_LEAN_AND_MEAN 先行定义：避免 windows.h 默认拉入 winsock.h 与
 * platform.h 引入的 winsock2.h 冲突（MSVC C2011 结构体重定义）。 */
#define WIN32_LEAN_AND_MEAN
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#ifndef EEXIST
#define EEXIST 17
#endif
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__) && defined(__MACH__)
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#endif

#include "error.h"
#include "platform.h"
#include "cancel_token.h"

#include "airy_memory.h"

int airy_network_init(void)
{
#if AIRY_PLATFORM_WINDOWS
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

const char *airy_arch_name(void)
{
    return AIRY_ARCH_NAME;
}

void airy_network_cleanup(void)
{
#if AIRY_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

void airy_ignore_sigpipe(void)
{
#ifndef AIRY_PLATFORM_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif
}

/* 原子标志位语义（0.1.6f 强化）：load=acquire / store=release /
 * fetch_add|sub=acq_rel。此前一律 seq_cst（全屏障）——cancel_token 的
 * is_canceled() 是零成本热路径（异步可中断轮询），在 ARM/RISC-V 上
 * seq_cst 需 dmb 全屏障、x86 上 seq_cst store 需 xchg/lock，
 * acquire/release 语义对标志位完全正确且消除全屏障开销。 */
int airy_atomic_load(airy_atomic_int_t *atomic)
{
    return atomic_load_explicit(atomic, memory_order_acquire);
}

void airy_atomic_store(airy_atomic_int_t *atomic, int value)
{
    atomic_store_explicit(atomic, value, memory_order_release);
}

int airy_atomic_fetch_add(airy_atomic_int_t *atomic, int value)
{
    return atomic_fetch_add_explicit(atomic, value, memory_order_acq_rel);
}

int airy_atomic_fetch_sub(airy_atomic_int_t *atomic, int value)
{
    return atomic_fetch_sub_explicit(atomic, value, memory_order_acq_rel);
}

uint64_t airy_thread_id(void)
{
#if AIRY_PLATFORM_WINDOWS
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

airy_sock_t airy_sock_tcp(void)
{
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

airy_sock_t airy_sock_unix(void)
{
#ifndef AIRY_PLATFORM_WINDOWS
    return socket(AF_UNIX, SOCK_STREAM, 0);
#else
    return AIRY_INVALID_SOCKET;
#endif
}

int airy_sock_set_nonblock(airy_sock_t sock, int nonblock)
{
#if AIRY_PLATFORM_WINDOWS
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
        return AIRY_EINVAL;
    if (nonblock) {
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    } else {
        return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
}

int airy_sock_set_reuseaddr(airy_sock_t sock, int reuse)
{
    int opt = reuse ? 1 : 0;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
}

void airy_sock_close(airy_sock_t sock)
{
#if AIRY_PLATFORM_WINDOWS
    closesocket(sock);
#else
    close(sock);
#endif
}

uint64_t airy_time_ns(void)
{
#if AIRY_PLATFORM_WINDOWS
    /* QPC 频率在系统生命周期内不变：缓存避免每次调用查询
     * QueryPerformanceFrequency（该 API 相对昂贵）。 */
    static LARGE_INTEGER frequency;
    static int frequency_inited = 0;
    if (!frequency_inited) {
        QueryPerformanceFrequency(&frequency);
        frequency_inited = 1;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t airy_time_ms(void)
{
    return airy_time_ns() / 1000000ULL;
}

void airy_sleep_ms(uint32_t ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

int airy_localtime_r(const time_t *timep, struct tm *result)
{
#ifdef _WIN32
    if (localtime_s(result, timep) != 0)
        return -1;
    return 0;
#else
    return localtime_r(timep, result) ? 0 : -1;
#endif
}

static AIRY_THREAD_LOCAL unsigned int g_random_seed = 0;
static AIRY_THREAD_LOCAL int g_random_initialized = 0;

void airy_random_init(void)
{
    if (!g_random_initialized) {
        g_random_seed = (unsigned int)airy_time_ns();
        g_random_initialized = 1;
    }
}

/* /dev/urandom 不可用（极罕见）时的回退：线程局部 xorshift，非密码学用途 */
static uint32_t airy_random_fallback(void)
{
    g_random_seed ^= g_random_seed << 13;
    g_random_seed ^= g_random_seed >> 17;
    g_random_seed ^= g_random_seed << 5;
    return g_random_seed;
}

/* 随机数线程局部缓冲池（0.1.6f 强化）：底层随机源（Windows
 * BCryptGenRandom / POSIX /dev/urandom）为系统级调用，单次开销可达
 * 数百 ns（实测 POSIX 833ns/op）。按线程批量预取 64 字节，耗尽再取，
 * 热路径（UUID/随机退避/采样）从系统调用降为内存拷贝（~5ns）。
 * 失败回退 xorshift。 */
static uint32_t airy_random_pool_take(void)
{
    static AIRY_THREAD_LOCAL uint8_t pool[64];
    static AIRY_THREAD_LOCAL size_t left = 0;
    if (left < sizeof(uint32_t)) {
#if AIRY_PLATFORM_WINDOWS
        if (BCryptGenRandom(NULL, pool, (ULONG)sizeof(pool),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
            return airy_random_fallback();
#else
        if (airy_random_bytes(pool, sizeof(pool)) != 0)
            return airy_random_fallback();
#endif
        left = sizeof(pool);
    }
    uint32_t v;
    __builtin_memcpy(&v, pool + sizeof(pool) - left, sizeof(v));
    left -= sizeof(v);
    return v;
}

uint32_t airy_random_uint32(uint32_t min, uint32_t max)
{
    if (!g_random_initialized) {
        airy_random_init();
    }

    uint32_t range = (max >= min) ? (max - min + 1) : 0;
    if (range == 0) {
        return min;
    }

    return min + airy_random_pool_take() % range;
}

float airy_random_float(void)
{
    if (!g_random_initialized) {
        airy_random_init();
    }

    return airy_random_pool_take() / 4294967296.0f;
}

int airy_random_bytes(void *buf, size_t len)
{
#if AIRY_PLATFORM_WINDOWS
    NTSTATUS status =
        BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return status == 0 ? 0 : -1;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return AIRY_EINVAL;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char *)buf + total, len - total);
        if (n <= 0) {
            close(fd);
            return AIRY_EINVAL;
        }
        total += (size_t)n;
    }

    close(fd);
    return 0;
#endif
}

int airy_file_exists(const char *path)
{
    if (!path)
        return 0;
#if AIRY_PLATFORM_WINDOWS
    struct _stat st;
    return _stat(path, &st) == 0 ? 1 : 0;
#else
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
#endif
}

int airy_mkdir_p(const char *path)
{
    if (!path)
        return AIRY_EINVAL;

    char *tmp = AIRY_STRDUP(path);
    if (!tmp)
        return AIRY_EINVAL;

    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';

#if AIRY_PLATFORM_WINDOWS
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif

            *p = saved;
        }
    }

#if AIRY_PLATFORM_WINDOWS
    int ret = _mkdir(tmp);
#else
    int ret = mkdir(tmp, 0755);
#endif

    AIRY_FREE(tmp);
    return (ret == 0 || errno == EEXIST) ? 0 : -1;
}

int64_t airy_file_size(const char *path)
{
    if (!path)
        return AIRY_EINVAL;
#if AIRY_PLATFORM_WINDOWS
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return AIRY_EINVAL;
    }
    return st.st_size;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return AIRY_EINVAL;
    }
    return st.st_size;
#endif
}

int airy_strlcpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || dest_size == 0 || !src) {
        return AIRY_EINVAL;
    }

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

    __builtin_memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return (int)copy_len;
}

int airy_strlcat(char *dest, const char *src, size_t dest_size)
{
    if (!dest || dest_size == 0 || !src) {
        return AIRY_EINVAL;
    }

    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) {
        return AIRY_EINVAL;
    }

    size_t src_len = strlen(src);
    size_t remaining = dest_size - dest_len - 1;
    size_t copy_len = (src_len < remaining) ? src_len : remaining;

    __builtin_memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';

    return (int)copy_len;
}

int airy_get_last_error(void)
{
#if AIRY_PLATFORM_WINDOWS
    return (int)GetLastError();
#else
    return errno;
#endif
}

const char *airy_strerror(int error)
{
#if AIRY_PLATFORM_WINDOWS
    static char msg[256];
    AIRY_MEMSET(msg, 0, sizeof(msg));
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, (DWORD)error,
                   0, msg, sizeof(msg), NULL);
    return msg;
#else
    return strerror(error);
#endif
}

int airy_file_lock(int fd, int exclusive, int block)
{
#if AIRY_PLATFORM_WINDOWS
    HANDLE h = (HANDLE)(intptr_t)fd;
    OVERLAPPED ovl;
    AIRY_MEMSET(&ovl, 0, sizeof(ovl));
    DWORD flags = exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    if (!block)
        flags |= LOCKFILE_FAIL_IMMEDIATELY;
    return LockFileEx(h, flags, 0, 1, 0, &ovl) ? 0 : (int)GetLastError();
#else
    struct flock fl;
    AIRY_MEMSET(&fl, 0, sizeof(fl));
    fl.l_type = exclusive ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    int cmd = block ? F_SETLKW : F_SETLK;
    if (fcntl(fd, cmd, &fl) == 0)
        return 0;
    if (errno == EACCES || errno == EAGAIN)
        return AIRY_EBUSY;
    return AIRY_EINVAL;
#endif
}

int airy_file_unlock(int fd)
{
#if AIRY_PLATFORM_WINDOWS
    HANDLE h = (HANDLE)(intptr_t)fd;
    OVERLAPPED ovl;
    AIRY_MEMSET(&ovl, 0, sizeof(ovl));
    return UnlockFileEx(h, 0, 1, 0, &ovl) ? 0 : (int)GetLastError();
#else
    struct flock fl;
    AIRY_MEMSET(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    return fcntl(fd, F_SETLK, &fl) == 0 ? 0 : AIRY_EINVAL;
#endif
}

int airy_get_sysinfo(airy_sysinfo_t *info)
{
    if (!info)
        return AIRY_EINVAL;
    AIRY_MEMSET(info, 0, sizeof(*info));

#if AIRY_PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->cpu_count = si.dwNumberOfProcessors;
    AIRY_STRNCPY_TERM(info->os_name, "Windows", sizeof(info->os_name));
    AIRY_STRNCPY_TERM(info->os_version, "10.0", sizeof(info->os_version));
    DWORD hn = (DWORD)sizeof(info->hostname);
    if (GetComputerNameA(info->hostname, &hn) == 0)
        AIRY_STRNCPY_TERM(info->hostname, "unknown", sizeof(info->hostname));
    MEMORYSTATUSEX ms;
    AIRY_MEMSET(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        info->memory_total = ms.ullTotalPhys;
        info->memory_free = ms.ullAvailPhys;
    }
    HKEY hk = NULL;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ,
                      &hk) == ERROR_SUCCESS) {
        DWORD type = REG_SZ;
        DWORD sz = (DWORD)sizeof(info->cpu_model);
        RegQueryValueExA(hk, "ProcessorNameString", NULL, &type, (LPBYTE)info->cpu_model, &sz);
        RegCloseKey(hk);
    }
#elif defined(__APPLE__) && defined(__MACH__)
    AIRY_STRNCPY_TERM(info->os_name, "Darwin", sizeof(info->os_name));
    char ver[64];
    size_t ver_len = sizeof(ver);
    if (sysctlbyname("kern.osrelease", ver, &ver_len, NULL, 0) == 0)
        AIRY_STRNCPY_TERM(info->os_version, ver, sizeof(info->os_version));
    if (gethostname(info->hostname, sizeof(info->hostname)) != 0)
        AIRY_STRNCPY_TERM(info->hostname, "unknown", sizeof(info->hostname));
    int ncpu = 0;
    size_t ncpu_len = sizeof(ncpu);
    if (sysctlbyname("hw.ncpu", &ncpu, &ncpu_len, NULL, 0) == 0)
        info->cpu_count = (uint32_t)ncpu;
    uint64_t mem = 0;
    size_t mem_len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &mem_len, NULL, 0) == 0)
        info->memory_total = mem;
    uint32_t pages = 0;
    size_t pages_len = sizeof(pages);
    if (sysctlbyname("vm.page_free_count", &pages, &pages_len, NULL, 0) == 0)
        info->memory_free = (uint64_t)pages * 4096;
    char brand[128];
    size_t brand_len = sizeof(brand);
    if (sysctlbyname("machdep.cpu.brand_string", brand, &brand_len, NULL, 0) == 0)
        AIRY_STRNCPY_TERM(info->cpu_model, brand, sizeof(info->cpu_model));
#else
    /* Linux/POSIX：uname + sysconf + /proc 单源 */
    struct utsname un;
    if (uname(&un) == 0) {
        AIRY_STRNCPY_TERM(info->os_name, un.sysname, sizeof(info->os_name));
        AIRY_STRNCPY_TERM(info->os_version, un.release, sizeof(info->os_version));
    }
    if (gethostname(info->hostname, sizeof(info->hostname)) != 0)
        AIRY_STRNCPY_TERM(info->hostname, "unknown", sizeof(info->hostname));
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu > 0)
        info->cpu_count = (uint32_t)ncpu;
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0)
        info->memory_total = (uint64_t)pages * (uint64_t)page_size;
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemAvailable:", 13) == 0) {
                unsigned long kb = strtoul(line + 13, NULL, 10);
                info->memory_free = (uint64_t)kb * 1024;
                break;
            }
        }
        fclose(f);
    }
    FILE *cf = fopen("/proc/cpuinfo", "r");
    if (cf) {
        char line[256];
        while (fgets(line, sizeof(line), cf)) {
            if (strncmp(line, "model name", 10) == 0) {
                const char *colon = strchr(line, ':');
                if (colon) {
                    const char *p = colon + 1;
                    while (*p == ' ' || *p == '\t')
                        p++;
                    size_t n = strlen(p);
                    while (n > 0 && (p[n - 1] == '\n' || p[n - 1] == '\r'))
                        n--;
                    size_t cap = sizeof(info->cpu_model) - 1;
                    if (n > cap)
                        n = cap;
                    __builtin_memcpy(info->cpu_model, p, n);
                    info->cpu_model[n] = '\0';
                }
                break;
            }
        }
        fclose(cf);
    }
#endif
    return AIRY_SUCCESS;
}

/* GPU 探测（q8f）：F2 硬件面板与硬件自动裁剪共用。best-effort——
 * 未探测到 GPU 写空串返回 SUCCESS（"无 GPU"是合法结果，非错误）。
 * 探测顺序：nvidia-smi → /proc/driver/nvidia/version → lspci。 */
int airy_get_gpu_info(char *out, size_t cap)
{
    if (!out || cap < 2)
        return AIRY_EINVAL;
    out[0] = '\0';

#if defined(_WIN32)
    (void)cap;
    /* Windows：nvidia-smi 常驻 NVIDIA 驱动目录；用 `where` 探测不可靠，
     * 保持空串（后续可接入 WMI 视频控制器查询）。 */
    return AIRY_SUCCESS;
#elif defined(__APPLE__) && defined(__MACH__)
    (void)cap;
    /* macOS：无 nvidia-smi；system_profiler 慢（数秒），不在此路径执行，
     * 保持空串（F2 面板显示"未报告 GPU"）。 */
    return AIRY_SUCCESS;
#else
    char line[256];
    /* 1) nvidia-smi -L：输出形如 "GPU 0: NVIDIA GeForce RTX 4090 (UUID: ...)"，
     * 截取 "GPU 0: " 之后、" (UUID" 之前为型号。 */
    FILE *fp = popen("nvidia-smi -L 2>/dev/null | head -1", "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            const char *p = strstr(line, "GPU 0: ");
            const char *start = p ? p + 7 : line;
            const char *uuid = strstr(start, " (UUID");
            size_t len = uuid ? (size_t)(uuid - start) : strlen(start);
            while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r'))
                len--;
            if (len > 0) {
                if (len >= cap)
                    len = cap - 1;
                __builtin_memcpy(out, start, len);
                out[len] = '\0';
            }
        }
        pclose(fp);
        if (out[0])
            return AIRY_SUCCESS;
    }
    /* 2) NVIDIA 驱动存在但无 nvidia-smi：/proc/driver/nvidia/version 首行
     * 含驱动版本，型号不可得，仅标注 NVIDIA 驱动。 */
    fp = fopen("/proc/driver/nvidia/version", "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            const char *p = strstr(line, "NVIDIA");
            if (p)
                AIRY_STRNCPY_TERM(out, p, cap);
        }
        fclose(fp);
        if (out[0])
            return AIRY_SUCCESS;
    }
    /* 3) 通用 PCI 探测：VGA/3D/Display 控制器（AMD/Intel/虚拟 GPU）。
     * lspci 输出如 "01:00.0 VGA compatible controller: NVIDIA ..."。 */
    fp = popen("lspci 2>/dev/null | grep -iE 'vga|3d|display' | head -1", "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            const char *colon = strchr(line, ':');
            if (colon) {
                const char *start = colon + 1;
                if (strncmp(start, " ", 1) == 0)
                    start++;
                size_t len = strlen(start);
                while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r'))
                    len--;
                if (len > 0) {
                    if (len >= cap)
                        len = cap - 1;
                    __builtin_memcpy(out, start, len);
                    out[len] = '\0';
                }
            }
        }
        pclose(fp);
    }
    return AIRY_SUCCESS;
#endif
}

/* 硬件画像（0.1.6 P1-3/d）：CPU/内存/加速器聚合 + minimal/full 分类。
 * 与 install.sh/airymaxrt assess_hardware() 同口径（SSoT 单一判据）。
 * 供 info_d 服务与上层自动裁剪/监控复用：外设增强（插卡/扩容）后调用方
 * 可据此恢复被裁剪的能力。 */
int airy_get_hw_profile(airy_hw_profile_t *out)
{
    if (!out)
        return AIRY_EINVAL;
    AIRY_MEMSET(out, 0, sizeof(*out));

    airy_sysinfo_t si;
    if (airy_get_sysinfo(&si) == AIRY_SUCCESS) {
        out->cpu_count = si.cpu_count;
        /* airy_get_sysinfo 的内存单位为字节，画像判据使用 KiB */
        out->mem_total_kib = si.memory_total / 1024;
        out->mem_avail_kib = si.memory_free / 1024;
    }

    out->profile = (out->mem_total_kib >= AIRY_HW_MIN_MEM_TOTAL_KIB &&
                    out->mem_avail_kib >= AIRY_HW_MIN_MEM_AVAIL_KIB &&
                    out->cpu_count >= AIRY_HW_MIN_CPU_COUNT)
                       ? AIRY_HW_PROFILE_FULL
                       : AIRY_HW_PROFILE_MINIMAL;

    char gpu[128];
    if (airy_get_gpu_info(gpu, sizeof(gpu)) == AIRY_SUCCESS && gpu[0]) {
        out->accel_present = 1;
        out->accel_count = 1;
        AIRY_STRNCPY_TERM(out->accel_model, gpu, sizeof(out->accel_model));
    }
    return AIRY_SUCCESS;
}


