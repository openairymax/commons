// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform.c
 * @brief 跨平台基础工具域：网络初始化、原子操作、Socket 通信、时间与随机数、文件系统与字符串辅助
 *
 * 提供统一的跨平台抽象层：
 * - Socket 网络通信
 * - 原子操作
 * - 时间与随机数
 * - 文件系统操作
 * - 字符串与错误辅助
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
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#define strdup _strdup
#define access _access /* flawfinder: ignore */
#ifndef EEXIST
#define EEXIST 17
#endif
#pragma comment(lib, "bcrypt.lib")
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
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

int airy_atomic_load(airy_atomic_int_t *atomic)
{
    return atomic_load_explicit(atomic, memory_order_seq_cst);
}

void airy_atomic_store(airy_atomic_int_t *atomic, int value)
{
    atomic_store_explicit(atomic, value, memory_order_seq_cst);
}

int airy_atomic_fetch_add(airy_atomic_int_t *atomic, int value)
{
    return atomic_fetch_add_explicit(atomic, value, memory_order_seq_cst);
}

int airy_atomic_fetch_sub(airy_atomic_int_t *atomic, int value)
{
    return atomic_fetch_sub_explicit(atomic, value, memory_order_seq_cst);
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
#if AIRY_PLATFORM_WINDOWS
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
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
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
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

static AIRY_THREAD_LOCAL unsigned int g_random_seed = 0;
static AIRY_THREAD_LOCAL int g_random_initialized = 0;

void airy_random_init(void)
{
    if (!g_random_initialized) {
        g_random_seed = (unsigned int)airy_time_ns();
        g_random_initialized = 1;
    }
}

uint32_t airy_random_uint32(uint32_t min, uint32_t max)
{
    if (!g_random_initialized) {
        airy_random_init();
    }

#if AIRY_PLATFORM_WINDOWS
    uint32_t rnd;
    BCryptGenRandom(NULL, (PUCHAR)&rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return min + (uint32_t)((rnd / 4294967296.0) * (max - min + 1));
#else
    return min + (uint32_t)((double)rand_r(&g_random_seed) / (RAND_MAX + 1.0) * (max - min + 1));
#endif
}

float airy_random_float(void)
{
    if (!g_random_initialized) {
        airy_random_init();
    }

#if AIRY_PLATFORM_WINDOWS
    uint32_t rnd;
    BCryptGenRandom(NULL, (PUCHAR)&rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return rnd / 4294967296.0f;
#else
    return (float)rand_r(&g_random_seed) / (float)RAND_MAX;
#endif
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
