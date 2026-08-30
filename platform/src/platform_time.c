// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_time.c
 * @brief 时间服务实现：逻辑墙钟 / 时区偏移 / SNTP 校对 / 周期同步。
 *
 * 设计要点：
 * - 逻辑墙钟 = 校正点(base, mono) + monotonic 增量：单调递增，不受
 *   系统时间跳变影响，保证系统时序稳定性；
 * - SNTP（RFC 4330 简化模式 3 客户端）：UDP 123 端口，48 字节请求/
 *   响应，解析 transmit timestamp（1900 epoch → Unix epoch）；
 * - 时区偏移：Hinnant days_from_civil 算法计算 localtime 与 UTC 之差，
 *   跨平台（不依赖 timegm/_mkgmtime）；
 * - 周期线程失败退避：2x 指数退避，上限 24h，成功后恢复标准间隔；
 * - 不依赖高层模块（无 logger / 无 cJSON），纯平台原语实现。
 */

#include <time.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/time.h>
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "platform.h"
#include "platform_time.h"
#include "airy_memory.h"

/* ==================== 内部常量 ==================== */

#define SNTP_PORT 123u
#define SNTP_PKT_LEN 48u
#define SNTP_LI_VN_MODE 0x23u /* LI=0, VN=4, Mode=3(client) */
#define NTP_UNIX_DELTA 2208988800ULL /* 1900-01-01 → 1970-01-01 秒 */
#define SYNC_DEFAULT_INTERVAL 3600u
#define SYNC_MIN_INTERVAL 30u
#define SYNC_MAX_BACKOFF 86400u
#define SYNC_ONCE_TIMEOUT 2000u

/* ==================== 内部状态 ==================== */

static _Atomic int64_t g_wall_base_ms = 0;   /* 校正点逻辑墙钟（epoch ms） */
static _Atomic uint64_t g_wall_base_mono = 0; /* 校正点单调时钟（ns） */
static _Atomic int g_wall_inited = 0;        /* 首次使用初始化标记 */

static _Atomic int64_t g_sync_delta_ms = 0;  /* 网络-系统偏移（ms） */
static _Atomic int g_sync_ready = 0;         /* 已联网同步标记 */
static _Atomic int g_sync_stop = 0;          /* 周期线程停止标记 */
static _Atomic int g_sync_running = 0;       /* 周期线程运行标记 */

/* ==================== 内部工具 ==================== */

/* 系统实时墙钟（epoch ms）。POSIX CLOCK_REALTIME；Windows
 * GetSystemTimeAsFileTime（1601 epoch → 1970 epoch）。 */
static int64_t sys_wall_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (int64_t)((t - 116444736000000000ULL) / 10000ULL);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* Hinnant days_from_civil：y/m/d → 1970-01-01 起的天数。 */
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2 ? 1 : 0;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

/* struct tm → Unix epoch 秒（按 tm 字面字段，不含时区二次换算）。 */
static int64_t tm_epoch_sec(const struct tm *t)
{
    int64_t day = days_from_civil((int64_t)t->tm_year + 1900, (unsigned)t->tm_mon + 1,
                                  (unsigned)t->tm_mday);
    return day * 86400 + (int64_t)t->tm_hour * 3600 + (int64_t)t->tm_min * 60 + t->tm_sec;
}

/* ==================== 公开 API ==================== */

int airy_time_tz_offset(void)
{
    time_t now = time(NULL);
    struct tm local_tm;
    if (airy_localtime_r(&now, &local_tm) != 0)
        return 0;
    /* localtime 的字段在本地时区语义下解释；其 epoch 与 UTC epoch 之差
     * 即"本地时区相对 UTC 的偏移"（含夏令时）。 */
    return (int)(tm_epoch_sec(&local_tm) - (int64_t)now);
}

uint64_t airy_time_wall_ms(void)
{
    if (atomic_load_explicit(&g_wall_inited, memory_order_acquire) == 0) {
        /* 首次使用：以系统时间为基准建立校正点（离线回退语义）。 */
        int64_t sys = sys_wall_ms();
        int64_t base = 0;
        if (atomic_compare_exchange_strong_explicit(&g_wall_base_ms, &base, sys,
                                                    memory_order_release,
                                                    memory_order_relaxed)) {
            atomic_store_explicit(&g_wall_base_mono, airy_time_ns(), memory_order_release);
            atomic_store_explicit(&g_wall_inited, 1, memory_order_release);
        }
    }
    uint64_t mono = airy_time_ns();
    uint64_t base_mono = atomic_load_explicit(&g_wall_base_mono, memory_order_acquire);
    int64_t base = atomic_load_explicit(&g_wall_base_ms, memory_order_acquire);
    return (uint64_t)(base + (int64_t)((mono - base_mono) / 1000000ULL));
}

uint64_t airy_time_wall_sec(void)
{
    return airy_time_wall_ms() / 1000;
}

int airy_time_sync_ready(void)
{
    return atomic_load_explicit(&g_sync_ready, memory_order_acquire);
}

int64_t airy_time_sync_delta(void)
{
    return atomic_load_explicit(&g_sync_delta_ms, memory_order_acquire);
}

/* ==================== SNTP 内部实现 ==================== */

static int sock_set_recv_timeout(airy_sock_t fd, uint32_t ms)
{
#if defined(_WIN32) || defined(_WIN64)
    DWORD tv = (DWORD)ms;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

static uint32_t be32_load(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

/* 向单个 NTP 服务器发起 SNTP 查询，成功返回 0 并输出 UTC epoch ms。 */
static int sntp_query(const char *host, uint32_t timeout_ms, int64_t *out_utc_ms)
{
    if (!host || !*host)
        return AIRY_EINVAL;

    struct addrinfo hints;
    AIRY_MEMSET(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", SNTP_PORT);

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return AIRY_ETIMEDOUT;

    int rc = AIRY_ETIMEDOUT;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        airy_sock_t fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == AIRY_INVALID_SOCKET)
            continue;

        uint8_t pkt[SNTP_PKT_LEN] = {0};
        pkt[0] = SNTP_LI_VN_MODE;
#if defined(_WIN32) || defined(_WIN64)
        int alen = (int)ai->ai_addrlen;
#else
        socklen_t alen = (socklen_t)ai->ai_addrlen;
#endif
        if ((int)sendto(fd, (const char *)pkt, sizeof(pkt), 0, ai->ai_addr, alen) < 0) {
            airy_sock_close(fd);
            continue;
        }

        if (sock_set_recv_timeout(fd, timeout_ms) != 0) {
            airy_sock_close(fd);
            continue;
        }

        struct sockaddr_storage from;
        uint8_t buf[SNTP_PKT_LEN];
#if defined(_WIN32) || defined(_WIN64)
        int from_len = (int)sizeof(from);
        int n = recvfrom(fd, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&from, &from_len);
#else
        socklen_t from_len = (socklen_t)sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &from_len);
#endif
        if (n >= (int)SNTP_PKT_LEN) {
            /* LI(VN Mode 字节高 2 位) ≤ 2 且 stratum 1~15 为可用响应 */
            uint8_t li = (buf[0] >> 6) & 0x03;
            uint8_t stratum = buf[1];
            if (li <= 2 && stratum >= 1 && stratum <= 15) {
                uint64_t ntp_sec = (uint64_t)be32_load(buf + 40);
                uint64_t ntp_frac = (uint64_t)be32_load(buf + 44);
                uint64_t unix_ms = (ntp_sec >= NTP_UNIX_DELTA ? ntp_sec - NTP_UNIX_DELTA : 0);
                *out_utc_ms = (int64_t)(unix_ms * 1000ULL +
                                        (ntp_frac * 1000ULL) / 0x100000000ULL);
                rc = AIRY_SUCCESS;
                airy_sock_close(fd);
                break;
            }
        }
        airy_sock_close(fd);
    }
    freeaddrinfo(res);
    return rc;
}

/* 逗号分隔列表拆分（跨平台，替换 strtok_r）：把 src 按 ',' 拆入
 * out（最多 cap-1 项 + NULL 结尾），返回项数。 */
static size_t split_csv(const char *src, char *out[], size_t cap)
{
    size_t n = 0;
    const char *p = src;
    while (*p && n + 1 < cap) {
        while (*p == ' ' || *p == ',')
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && *p != ',')
            p++;
        const char *end = p;
        while (end > start && end[-1] == ' ')
            end--;
        size_t len = (size_t)(end - start);
        if (len > 0) {
            char *slot = out[n];
            size_t copy = len < 127 ? len : 127;
            __builtin_memcpy(slot, start, copy);
            slot[copy] = '\0';
            n++;
        }
    }
    out[n] = NULL;
    return n;
}

/* 解析服务器列表：优先 AIRY_NTP_SERVERS 环境变量（逗号分隔），
 * 否则内置默认列表。 */
static const char *const *sntp_servers(void)
{
    static const char *const defaults[] = {"pool.ntp.org", "ntp.aliyun.com",
                                           "time.google.com", NULL};
    const char *env = getenv("AIRY_NTP_SERVERS");
    if (env && *env) {
        static char env_pool[7][128];
        static char *env_argv[8];
        static const char *env_ret[8];
        char *ptrs[8];
        size_t i;
        for (i = 0; i < 7; i++)
            ptrs[i] = env_pool[i];
        size_t n = split_csv(env, ptrs, 8);
        if (n > 0) {
            for (i = 0; i < n; i++)
                env_argv[i] = env_pool[i];
            env_argv[n] = NULL;
            for (i = 0; i <= n; i++)
                env_ret[i] = env_argv[i];
            return env_ret;
        }
    }
    return defaults;
}

int airy_time_sync_once(uint32_t timeout_ms)
{
    if (timeout_ms == 0)
        return AIRY_EINVAL;
    if (timeout_ms < 200)
        timeout_ms = 200;

    const char *const *servers = sntp_servers();
    int64_t utc_ms = 0;
    int rc = AIRY_ETIMEDOUT;
    for (size_t i = 0; servers[i]; i++) {
        if (sntp_query(servers[i], timeout_ms, &utc_ms) == AIRY_SUCCESS) {
            rc = AIRY_SUCCESS;
            break;
        }
    }
    if (rc != AIRY_SUCCESS)
        return AIRY_ETIMEDOUT;

    /* 校正点：标准本地时间 = UTC + 时区偏移；偏移 = 标准本地 - 系统时间 */
    int64_t std_local_ms = utc_ms + (int64_t)airy_time_tz_offset() * 1000;
    int64_t sys = sys_wall_ms();
    int64_t delta = std_local_ms - sys;

    atomic_store_explicit(&g_sync_delta_ms, delta, memory_order_release);
    atomic_store_explicit(&g_sync_ready, 1, memory_order_release);
    atomic_store_explicit(&g_wall_base_ms, std_local_ms, memory_order_release);
    atomic_store_explicit(&g_wall_base_mono, airy_time_ns(), memory_order_release);
    atomic_store_explicit(&g_wall_inited, 1, memory_order_release);
    return AIRY_SUCCESS;
}

/* ==================== 周期校对线程 ==================== */

static void *sync_loop(void *arg)
{
    uint32_t interval = *(const uint32_t *)arg;
    uint32_t cur = interval;
    while (atomic_load_explicit(&g_sync_stop, memory_order_acquire) == 0) {
        /* 分段睡眠 1s，可被 stop 及时打断 */
        uint32_t slept = 0;
        while (slept < cur && atomic_load_explicit(&g_sync_stop, memory_order_acquire) == 0) {
            airy_sleep_ms(1000);
            slept++;
        }
        if (atomic_load_explicit(&g_sync_stop, memory_order_acquire) != 0)
            break;
        if (airy_time_sync_once(SYNC_ONCE_TIMEOUT) == AIRY_SUCCESS) {
            cur = interval;
        } else {
            if (cur < SYNC_MAX_BACKOFF)
                cur *= 2;
            if (cur > SYNC_MAX_BACKOFF)
                cur = SYNC_MAX_BACKOFF;
        }
    }
    atomic_store_explicit(&g_sync_running, 0, memory_order_release);
    return NULL;
}

int airy_time_sync_start(uint32_t interval_sec)
{
    if (atomic_load_explicit(&g_sync_running, memory_order_acquire) != 0)
        return AIRY_SUCCESS; /* 幂等 */
    if (interval_sec == 0 || interval_sec < SYNC_MIN_INTERVAL)
        interval_sec = SYNC_DEFAULT_INTERVAL;

    static uint32_t s_interval = SYNC_DEFAULT_INTERVAL;
    s_interval = interval_sec;

    atomic_store_explicit(&g_sync_stop, 0, memory_order_release);
    airy_thread_t th;
    if (airy_platform_thread_create(&th, sync_loop, &s_interval) != 0)
        return AIRY_ENOMEM;
    airy_platform_thread_detach(th);
    atomic_store_explicit(&g_sync_running, 1, memory_order_release);
    return AIRY_SUCCESS;
}

void airy_time_sync_stop(void)
{
    if (atomic_load_explicit(&g_sync_running, memory_order_acquire) == 0)
        return;
    atomic_store_explicit(&g_sync_stop, 1, memory_order_release);
    /* 等待线程退出（分段睡眠最多 1s 即检查停止标记） */
    for (int i = 0; i < 500; i++) {
        if (atomic_load_explicit(&g_sync_running, memory_order_acquire) == 0)
            return;
        airy_sleep_ms(10);
    }
}
