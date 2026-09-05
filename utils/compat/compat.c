// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file compat.c
 * @brief Cross-platform compatibility implementation.
 */

#include "compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "logging.h"

typedef void (*assert_handler_fn_t)(const char *cond, const char *file, int line, const char *func,
                                    const char *msg);

static assert_handler_fn_t g_assert_handler = NULL;

void airy_set_assert_handler(void (*handler)(const char *, const char *, int, const char *,
                                             const char *))
{
    g_assert_handler = (assert_handler_fn_t)handler;
}

void (*airy_get_assert_handler(void))(const char *, const char *, int, const char *, const char *)
{
    return (void (*)(const char *, const char *, int, const char *, const char *))g_assert_handler;
}

#ifdef AIRY_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <intrin.h>
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

char *airy_strncpy_safe(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0) {
        return dest;
    }

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

    __builtin_memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return dest;
}

int airy_memset_s(void *dest, int c, size_t dest_size, size_t count)
{
    if (!dest) {
        return AIRY_EINVAL;
    }

    if (count > dest_size) {
        return AIRY_EINVAL;
    }

    __builtin_memset(dest, (int)c, count);
    return 0;
}

int airy_memcpy_s(void *dest, size_t dest_size, const void *src, size_t count)
{
    if (!dest || !src) {
        return AIRY_EINVAL;
    }

    if (count > dest_size) {
        return AIRY_EINVAL;
    }

    if ((char *)dest < (const char *)src + count && (const char *)src < (char *)dest + count) {

        __builtin_memmove(dest, src, count);
    } else {
        __builtin_memcpy(dest, src, count);
    }

    return 0;
}

int airy_memmove_s(void *dest, size_t dest_size, const void *src, size_t count)
{
    if (!dest || !src) {
        return AIRY_EINVAL;
    }

    if (count > dest_size) {
        return AIRY_EINVAL;
    }

    __builtin_memmove(dest, src, count);
    return 0;
}

void airy_assert_fail(const char *cond, const char *file, int line, const char *func)
{
    AIRY_LOG_ERROR("Assertion failed: %s", cond);
    AIRY_LOG_ERROR("  at %s:%d in %s()", file, line, func);

    if (g_assert_handler) {
        g_assert_handler(cond, file, line, func, NULL);
        return;
    }

#ifdef AIRY_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
    raise(SIGABRT);
#endif

    abort();
}

void airy_assert_fail_msg(const char *cond, const char *file, int line, const char *func,
                          const char *msg)
{
    AIRY_LOG_ERROR("Assertion failed: %s", cond);
    AIRY_LOG_ERROR("  Message: %s", msg);
    AIRY_LOG_ERROR("  at %s:%d in %s()", file, line, func);

    if (g_assert_handler) {
        g_assert_handler(cond, file, line, func, msg);
        return;
    }

#ifdef AIRY_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
    raise(SIGABRT);
#endif

    abort();
}

void airy_debug_break(void)
{
#ifdef AIRY_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
#ifdef SIGTRAP
    raise(SIGTRAP);
#else
    raise(SIGABRT);
#endif
#endif
}

#ifndef AIRY_VERSION_STRING
#define AIRY_VERSION_STRING "0.1.1"
#endif
#ifndef AIRY_VERSION_MAJOR
#define AIRY_VERSION_MAJOR 0
#endif
#ifndef AIRY_VERSION_MINOR
#define AIRY_VERSION_MINOR 0
#endif
#ifndef AIRY_VERSION_PATCH
#define AIRY_VERSION_PATCH 5
#endif

static const char *g_version_string = AIRY_VERSION_STRING;

const char *airy_version_string(void)
{
    return g_version_string;
}

const char *airy_build_info(void)
{
    static char build_info[256] = {0};

    if (build_info[0] == '\0') {
        snprintf(build_info, sizeof(build_info),
                 "AgentRT v%s | Compiler: %s | Platform: %s | Build: %s %s", AIRY_VERSION_STRING,
                 AIRY_COMPILER_NAME, AIRY_PLATFORM_NAME, __DATE__, __TIME__);
    }

    return build_info;
}

#ifdef _WIN32
#include <windows.h>
#include "airy_memory.h"

int gethostname(char *name, int len)
{
    DWORD size = (DWORD)len;
    if (!GetComputerNameA(name, &size)) {
        return AIRY_EINVAL;
    }
    return 0;
}

long sysconf(int name)
{
    switch (name) {
    case _SC_PAGESIZE: {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (long)si.dwPageSize;
    }
    case _SC_NPROCESSORS_ONLN: {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (long)si.dwNumberOfProcessors;
    }
    case _SC_OPEN_MAX:
        return 512;
    case _SC_CLK_TCK:
        return 1000;
    default:
        return AIRY_EINVAL;
    }
}

int nanosleep(const struct timespec *ts, struct timespec *rem)
{
    (void)rem;
    DWORD ms = (DWORD)(ts->tv_sec * 1000 + ts->tv_nsec / 1000000);
    Sleep(ms);
    return 0;
}

int clock_gettime(int clk_id, struct timespec *ts)
{
    if (!ts)
        return -1;
    if (clk_id == CLOCK_MONOTONIC) {
        LARGE_INTEGER freq, cnt;
        if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&cnt))
            return -1;
        ts->tv_sec = (time_t)(cnt.QuadPart / freq.QuadPart);
        ts->tv_nsec = (long)((cnt.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
        return 0;
    }
    /* CLOCK_REALTIME（默认分支）：GetSystemTimeAsFileTime 100ns 自
     * 1601-01-01，减 Unix 纪元偏移（11644473600s）后换算。用不带
     * _WIN32_WINNT>=0x0602 要求的版本，兼容任意 SDK 目标。 */
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    const uint64_t epoch_100ns = UINT64_C(116444736000000000);
    uint64_t t100 = u.QuadPart - epoch_100ns;
    ts->tv_sec = (time_t)(t100 / UINT64_C(10000000));
    ts->tv_nsec = (long)((t100 % UINT64_C(10000000)) * 100);
    return 0;
}

int setenv(const char *name, const char *value, int overwrite)
{
    if (!name || !name[0])
        return -1;
    if (!overwrite) {
        size_t needed = 0;
        if (getenv_s(&needed, NULL, 0, name) == 0 && needed > 0)
            return 0;
    }
    return _putenv_s(name, value ? value : "") == 0 ? 0 : -1;
}

char *strndup(const char *s, size_t n)
{
    size_t len = 0;
    const char *p = s;
    while (len < n && *p) {
        len++;
        p++;
    }
    char *dup = (char *)AIRY_MALLOC(len + 1);
    if (dup) {
        __builtin_memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

struct tm *localtime_r(const time_t *timer, struct tm *buf)
{
    if (localtime_s(buf, timer) == 0)
        return buf;
    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
}

/* strtok_r: MSVC's strtok_s takes char **context as the third argument,
 * which matches POSIX strtok_r's char **saveptr semantics, so map directly.
 * MSVC 下 windows_preinclude.h 已把 strtok_r 宏映射为 strtok_s，此处再
 * 定义同名函数会被宏展开成 strtok_s 定义 → 与 ucrt.lib 冲突（LNK2005）
 * 且无限自递归；UCRT 已提供 strtok_s，MSVC 构建无需此 shim。 */
#if !defined(_MSC_VER)
char *strtok_r(char *str, const char *delim, char **saveptr)
{
    return strtok_s(str, delim, saveptr);
}
#endif
#endif
