/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file compat.h
 * @brief Cross-platform compatibility definitions.
 *
 * Provides compiler compatibility, platform abstraction macros, and
 * bit-operation utilities.
 *
 * @see docs/Capital_Specifications/coding_standard/C_coding_style_standard.md
 */

#ifndef AIRY_RT_UTILS_COMPAT_H
#define AIRY_RT_UTILS_COMPAT_H


#ifdef _WIN32
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#endif

#include "atomic_compat.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef AIRY_API
#ifdef _WIN32
#ifdef AIRY_BUILD_SHARED
#define AIRY_API __declspec(dllexport)
#elif defined(AIRY_USE_SHARED)
#define AIRY_API __declspec(dllimport)
#else
#define AIRY_API
#endif
#else
#define AIRY_API __attribute__((visibility("default")))
#endif
#endif /* AIRY_API */


#if defined(__GNUC__)
#define AIRY_COMPILER_GCC 1
#define AIRY_COMPILER_NAME "GCC"
#define AIRY_COMPILER_VERSION __GNUC__
#elif defined(__clang__)
#define AIRY_COMPILER_CLANG 1
#define AIRY_COMPILER_NAME "Clang"
#define AIRY_COMPILER_VERSION __clang_major__
#elif defined(_MSC_VER)
#define AIRY_COMPILER_MSVC 1
#define AIRY_COMPILER_NAME "MSVC"
#define AIRY_COMPILER_VERSION _MSC_VER
#else
#define AIRY_COMPILER_UNKNOWN 1
#define AIRY_COMPILER_NAME "Unknown"
#define AIRY_COMPILER_VERSION 0
#endif


#if defined(_WIN32) || defined(_WIN64)
#define AIRY_PLATFORM_WINDOWS 1
#define AIRY_PLATFORM_NAME "Windows"
#elif defined(__linux__)
#define AIRY_PLATFORM_LINUX 1
#define AIRY_PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
#define AIRY_PLATFORM_MACOS 1
#define AIRY_PLATFORM_NAME "macOS"
#else
#define AIRY_PLATFORM_UNKNOWN 1
#define AIRY_PLATFORM_NAME "Unknown"
#endif


#if defined(AIRY_COMPILER_GCC) || defined(AIRY_COMPILER_CLANG)
#ifndef AIRY_INLINE
#define AIRY_INLINE static inline __attribute__((always_inline))
#endif
#define AIRY_NOINLINE __attribute__((noinline))
#ifndef AIRY_UNUSED
#define AIRY_UNUSED __attribute__((unused))
#endif
#define AIRY_USED __attribute__((used))
#define AIRY_WEAK __attribute__((weak))
#define AIRY_PACKED __attribute__((packed))
#ifndef AIRY_ALIGNED
#define AIRY_ALIGNED(x) __attribute__((aligned(x)))
#endif
#define AIRY_DEPRECATED __attribute__((deprecated))
#define AIRY_FALLTHROUGH __attribute__((fallthrough))
#define AIRY_PRINTF_FORMAT(fmt, args) __attribute__((format(__printf__, fmt, args)))
#define AIRY_SCANF_FORMAT(fmt, args) __attribute__((format(__scanf__, fmt, args)))
#define AIRY_LIKELY(x) __builtin_expect(!!(x), 1)
#define AIRY_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define AIRY_PREFETCH(x) __builtin_prefetch(x)
#define AIRY_UNREACHABLE() __builtin_unreachable()
#define AIRY_ASSUME(x)               \
    do {                             \
        if (!(x))                    \
            __builtin_unreachable(); \
    } while (0)
#elif defined(AIRY_COMPILER_MSVC)
#ifndef AIRY_INLINE
#define AIRY_INLINE static __forceinline
#endif
#define AIRY_NOINLINE __declspec(noinline)
#ifndef AIRY_UNUSED
#define AIRY_UNUSED
#endif
#define AIRY_USED
#define AIRY_WEAK
#define AIRY_PACKED
/* 与 include/airymax/uapi_compat.h 同名宏（其 MSVC 分支为 no-op）互斥：
 * 谁先被包含谁生效，避免 C4005 重定义（/WX 下即错误）。MSVC 下
 * __declspec(align(n)) 需置于 struct 关键字之前（见 AIRY_ALIGNED_PREFIX）。 */
#ifndef AIRY_ALIGNED
#define AIRY_ALIGNED(x) __declspec(align(x))
#endif
#define AIRY_DEPRECATED __declspec(deprecated)
#define AIRY_FALLTHROUGH
#define AIRY_PRINTF_FORMAT(fmt, args)
#define AIRY_SCANF_FORMAT(fmt, args)
#define AIRY_LIKELY(x) (x)
#define AIRY_UNLIKELY(x) (x)
#define AIRY_PREFETCH(x)
#define AIRY_UNREACHABLE() __assume(0)
#define AIRY_ASSUME(x) __assume(x)
#else
#ifndef AIRY_INLINE
#define AIRY_INLINE static inline
#endif
#define AIRY_NOINLINE
#ifndef AIRY_UNUSED
#define AIRY_UNUSED
#endif
#define AIRY_USED
#define AIRY_WEAK
#define AIRY_PACKED
#define AIRY_ALIGNED(x)
#define AIRY_DEPRECATED
#define AIRY_FALLTHROUGH
#define AIRY_PRINTF_FORMAT(fmt, args)
#define AIRY_SCANF_FORMAT(fmt, args)
#define AIRY_LIKELY(x) (x)
#define AIRY_UNLIKELY(x) (x)
#define AIRY_PREFETCH(x)
#define AIRY_UNREACHABLE()
#define AIRY_ASSUME(x) ((void)0)
#endif


#if defined(AIRY_PLATFORM_WINDOWS)
#define AIRY_THREAD_LOCAL __declspec(thread)
#else
#define AIRY_THREAD_LOCAL __thread
#endif


#if defined(AIRY_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup

#include <time.h>
#include <windows.h>

AIRY_API int nanosleep(const struct timespec *ts, struct timespec *rem);
AIRY_API char *strndup(const char *s, size_t n);
AIRY_API struct tm *localtime_r(const time_t *timer, struct tm *buf);
/* C11 时钟/环境垫片（UCRT 不提供；windows_preinclude.h 与 <time.h> 均可能
 * 定义 CLOCK_*，用 #ifndef 守卫防重定义冲突）。 */
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
AIRY_API int clock_gettime(int clk_id, struct timespec *ts);
AIRY_API int setenv(const char *name, const char *value, int overwrite);

/* 采样/统计计数器（0.1.6f 强化）：relaxed——fetch_add 无同步需求，
 * 仅作近似计数（logging_control 采样率），避免 seq_cst 全屏障。 */
#define AIRY_ATOMIC_FETCH_ADD(ptr, val) atomic_fetch_add_explicit(ptr, val, memory_order_relaxed)
#define AIRY_ATOMIC_FETCH_ADD64(ptr, val) atomic_fetch_add_explicit(ptr, val, memory_order_relaxed)

#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 1
#endif
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 2
#endif
#ifndef _SC_OPEN_MAX
#define _SC_OPEN_MAX 3
#endif
#ifndef _SC_CLK_TCK
#define _SC_CLK_TCK 4
#endif
#else
/* 采样/统计计数器（0.1.6f 强化）：relaxed，理由同 Windows 分支。 */
#define AIRY_ATOMIC_FETCH_ADD(ptr, val) atomic_fetch_add_explicit(ptr, val, memory_order_relaxed)
#define AIRY_ATOMIC_FETCH_ADD64(ptr, val) atomic_fetch_add_explicit(ptr, val, memory_order_relaxed)
#endif


#if defined(AIRY_PLATFORM_WINDOWS)
#define AIRY_PATH_SEP '\\'
#define AIRY_PATH_SEP_STR "\\"
#else
#define AIRY_PATH_SEP '/'
#define AIRY_PATH_SEP_STR "/"
#endif

#ifndef AIRY_PATH_MAX
#if defined(AIRY_PLATFORM_WINDOWS)
#define AIRY_PATH_MAX 260
#else
#define AIRY_PATH_MAX 4096
#endif
#endif


/**
 * @brief Check whether a pointer is aligned
 * @param ptr Pointer
 * @param align Alignment value (must be a power of 2)
 * @return 1 if aligned, 0 if not
 */
AIRY_INLINE int airy_is_aligned(const void *ptr, size_t align)
{
    return ((uintptr_t)ptr & (align - 1)) == 0;
}

/**
 * @brief Align a value up
 * @param value Original value
 * @param align Alignment value (must be a power of 2)
 * @return Aligned value
 */
AIRY_INLINE size_t airy_align_up(size_t value, size_t align)
{
    return (value + align - 1) & ~(align - 1);
}

/**
 * @brief Align a value down
 * @param value Original value
 * @param align Alignment value (must be a power of 2)
 * @return Aligned value
 */
AIRY_INLINE size_t airy_align_down(size_t value, size_t align)
{
    return value & ~(align - 1);
}


/**
 * @brief Get the number of array elements
 * @param arr Array
 * @return Number of elements
 */
#define AIRY_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief Get the offset of a struct member
 * @param type Struct type
 * @param member Member name
 * @return Offset
 */
#if defined(AIRY_COMPILER_GCC) || defined(AIRY_COMPILER_CLANG)
#define AIRY_OFFSETOF(type, member) __builtin_offsetof(type, member)
#else
#define AIRY_OFFSETOF(type, member) ((size_t)&((type *)0)->member)
#endif

/**
 * @brief Get the struct pointer from a member pointer
 * @param ptr Member pointer
 * @param type Struct type
 * @param member Member name
 * @return Struct pointer
 */
#define AIRY_CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - AIRY_OFFSETOF(type, member)))


/**
 * @brief Test whether a bit is set
 * @param x Value
 * @param bit Bit index (starting from 0)
 * @return 1 if set, 0 if not
 */
AIRY_INLINE int airy_bit_test(unsigned int x, unsigned int bit)
{
    return (int)((x >> bit) & 1U);
}

/**
 * @brief Set a bit
 * @param x Value pointer
 * @param bit Bit index (starting from 0)
 */
AIRY_INLINE void airy_bit_set(unsigned int *x, unsigned int bit)
{
    if (x)
        *x |= (1U << bit);
}

/**
 * @brief Clear a bit
 * @param x Value pointer
 * @param bit Bit index (starting from 0)
 */
AIRY_INLINE void airy_bit_clear(unsigned int *x, unsigned int bit)
{
    if (x)
        *x &= ~(1U << bit);
}

/**
 * @brief Flip a bit
 * @param x Value pointer
 * @param bit Bit index (starting from 0)
 */
AIRY_INLINE void airy_bit_flip(unsigned int *x, unsigned int bit)
{
    if (x)
        *x ^= (1U << bit);
}

/**
 * @brief Count the number of set bits
 * @param x Value
 * @return Number of set bits
 */
AIRY_INLINE unsigned int airy_popcount(unsigned int x)
{
#if defined(AIRY_COMPILER_GCC) || defined(AIRY_COMPILER_CLANG)
    return (unsigned int)__builtin_popcount(x);
#elif defined(AIRY_COMPILER_MSVC)
    return (unsigned int)__popcnt(x);
#else
    unsigned int count = 0;
    while (x) {
        count += x & 1U;
        x >>= 1;
    }
    return count;
#endif
}

/**
 * @brief Count the number of leading zeros
 * @param x Value
 * @return Number of leading zeros
 */
AIRY_INLINE unsigned int airy_clz(unsigned int x)
{
#if defined(AIRY_COMPILER_GCC) || defined(AIRY_COMPILER_CLANG)
    return (unsigned int)__builtin_clz(x);
#elif defined(AIRY_COMPILER_MSVC)
    unsigned long index;
    if (_BitScanReverse(&index, x)) {
        return 31U - (unsigned int)index;
    }
    return 32U;
#else
    unsigned int n = 0;
    if (x == 0)
        return 32U;
    while ((x & 0x80000000U) == 0) {
        n++;
        x <<= 1;
    }
    return n;
#endif
}

/**
 * @brief Count the number of trailing zeros
 * @param x Value
 * @return Number of trailing zeros
 */
AIRY_INLINE unsigned int airy_ctz(unsigned int x)
{
#if defined(AIRY_COMPILER_GCC) || defined(AIRY_COMPILER_CLANG)
    return (unsigned int)__builtin_ctz(x);
#elif defined(AIRY_COMPILER_MSVC)
    unsigned long index;
    if (_BitScanForward(&index, x)) {
        return (unsigned int)index;
    }
    return 32U;
#else
    unsigned int n = 0;
    if (x == 0)
        return 32U;
    while ((x & 1U) == 0) {
        n++;
        x >>= 1;
    }
    return n;
#endif
}


/**
 * @brief Safe string copy
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Destination buffer size
 * @return 0 on success, non-zero on failure
 */
AIRY_API int airy_strlcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief Safe string concatenation
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Destination buffer size
 * @return 0 on success, non-zero on failure
 */
AIRY_API int airy_strlcat(char *dest, const char *src, size_t dest_size);

/**
 * @brief Safe string copy (returns the destination)
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Destination buffer size
 * @return Destination buffer pointer
 */
AIRY_API char *airy_strncpy_safe(char *dest, const char *src, size_t dest_size);


/**
 * @brief Safe memory set
 * @param dest Destination buffer
 * @param c Fill value
 * @param dest_size Destination buffer size
 * @param count Number of bytes to set
 * @return 0 on success, non-zero on failure
 */
AIRY_API int airy_memset_s(void *dest, int c, size_t dest_size, size_t count);

/**
 * @brief Safe memory copy
 * @param dest Destination buffer
 * @param dest_size Destination buffer size
 * @param src Source buffer
 * @param count Number of bytes to copy
 * @return 0 on success, non-zero on failure
 */
AIRY_API int airy_memcpy_s(void *dest, size_t dest_size, const void *src, size_t count);

/**
 * @brief Safe memory move
 * @param dest Destination buffer
 * @param dest_size Destination buffer size
 * @param src Source buffer
 * @param count Number of bytes to move
 * @return 0 on success, non-zero on failure
 */
AIRY_API int airy_memmove_s(void *dest, size_t dest_size, const void *src, size_t count);


#ifdef NDEBUG
#define AIRY_ASSERT(cond) ((void)0)
#define AIRY_ASSERT_MSG(cond, msg) ((void)0)
#else
#define AIRY_ASSERT(cond)                                          \
    do {                                                           \
        if (!(cond)) {                                             \
            airy_assert_fail(#cond, __FILE__, __LINE__, __func__); \
        }                                                          \
    } while (0)

#define AIRY_ASSERT_MSG(cond, msg)                                          \
    do {                                                                    \
        if (!(cond)) {                                                      \
            airy_assert_fail_msg(#cond, __FILE__, __LINE__, __func__, msg); \
        }                                                                   \
    } while (0)
#endif

/**
 * @brief Assertion failure handler
 */
AIRY_API void airy_assert_fail(const char *cond, const char *file, int line, const char *func);

/**
 * @brief Assertion failure handler (with message)
 */
AIRY_API void airy_assert_fail_msg(const char *cond, const char *file, int line, const char *func,
                                   const char *msg);

/**
 * @brief Custom assertion handler callback type
 *
 * When set, this callback is invoked on assertion failure instead of
 * abort(). In production it can be set to log and degrade gracefully.
 */
typedef void (*airy_assert_handler_t)(const char *cond, const char *file, int line,
                                      const char *func, const char *msg);

/**
 * @brief Set a custom assertion handler
 */
AIRY_API void airy_set_assert_handler(airy_assert_handler_t handler);

/**
 * @brief Get the current assertion handler
 */
AIRY_API airy_assert_handler_t airy_get_assert_handler(void);


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AIRY_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(AIRY_COMPILER_GCC) || defined(AIRY_COMPILER_CLANG)
#define AIRY_STATIC_ASSERT(cond, msg) \
    typedef char airy_static_assert_##__LINE__[(cond) ? 1 : -1] __attribute__((unused))
#else
#define AIRY_STATIC_ASSERT(cond, msg) typedef char airy_static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

/**
 * @brief Compile-time check
 */
#define AIRY_COMPILE_TIME_ASSERT(cond) AIRY_STATIC_ASSERT(cond, "Compile-time assertion failed")

/**
 * @brief Check a type's size
 */
#define AIRY_CHECK_SIZE(type, size) \
    AIRY_STATIC_ASSERT(sizeof(type) == size, "Size mismatch for " #type)


#ifdef DEBUG
#define AIRY_DEBUG_BREAK() airy_debug_break()
#else
#define AIRY_DEBUG_BREAK() ((void)0)
#endif

/**
 * @brief Debug breakpoint
 */
AIRY_API void airy_debug_break(void);


#ifndef AIRY_VERSION_MAJOR
#define AIRY_VERSION_MAJOR 0
#endif
#ifndef AIRY_VERSION_MINOR
#define AIRY_VERSION_MINOR 0
#endif
#ifndef AIRY_VERSION_PATCH
#define AIRY_VERSION_PATCH 5
#endif
#ifndef AIRY_VERSION_STRING
#define AIRY_VERSION_STRING "0.1.1"
#endif

/**
 * @brief Get the version string
 */
AIRY_API const char *airy_version_string(void);

/**
 * @brief Get build information
 */
AIRY_API const char *airy_build_info(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_COMPAT_H */
