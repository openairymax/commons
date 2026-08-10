// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file compat.c
 * @brief 跨平台兼容性实现
 */

#include "compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "logging.h"  /* d9: 改用 LOG_ERROR（logging.h 完整日志 API） */



typedef void (*assert_handler_fn_t)(const char *cond, const char *file, int line, const char *func,
                                    const char *msg);

static assert_handler_fn_t g_assert_handler = NULL;

void airy_set_assert_handler(void (*handler)(const char *, const char *, int, const char *,
                                                const char *))
{
    g_assert_handler = (assert_handler_fn_t)handler;
}

void (*airy_get_assert_handler(void))(const char *, const char *, int, const char *,
                                         const char *)
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

/* ==================== 安全字符串函数实现 ==================== */

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

/* ==================== 安全内存函数实现 ==================== */

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
        /* 重叠区域，使用 memmove */
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

/* ==================== 断言函数实现 ==================== */

void airy_assert_fail(const char *cond, const char *file, int line, const char *func)
{
    LOG_ERROR("Assertion failed: %s", cond);
    LOG_ERROR("  at %s:%d in %s()", file, line, func);

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
    LOG_ERROR("Assertion failed: %s", cond);
    LOG_ERROR("  Message: %s", msg);
    LOG_ERROR("  at %s:%d in %s()", file, line, func);

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

/* ==================== 调试函数实现 ==================== */

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

/* ==================== 版本信息实现 ==================== */

static const char *g_version_string = "0.1.1";

const char *airy_version_string(void)
{
    return g_version_string;
}

const char *airy_build_info(void)
{
    static char build_info[256] = {0};

    if (build_info[0] == '\0') {
        snprintf(build_info, sizeof(build_info),
                 "AgentRT v%s | Compiler: %s | Platform: %s | Build: %s %s", "0.1.1", "gcc",
                 "linux", __DATE__, __TIME__);
    }

    return build_info;
}

#ifdef _WIN32
#include <windows.h>
#include "airy_memory.h"

int gethostname(char *name, size_t len)
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

char *AIRY_STRDUP(const char *s, size_t n)
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
#endif
