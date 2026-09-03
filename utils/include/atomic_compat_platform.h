/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * Platform selection & low-level atomic primitives:
 *   - C11 stdatomic (Linux/macOS)
 *   - Interlocked API (Windows)
 *   - __atomic builtins (POSIX fallback)
 * Split from atomic_compat.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_ATOMIC_COMPAT_PLATFORM_H
#define AIRY_RT_ATOMIC_COMPAT_PLATFORM_H

#pragma GCC system_header

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(_WIN32) && \
    !defined(AIRY_NO_STDATOMIC) && !defined(_MSC_VER)


#ifndef AIRY_COREKERN_STDATOMIC_SHIM
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include_next <stdatomic.h>
#pragma GCC diagnostic pop
#endif

#define AIRY_USE_STDATOMIC 1

static inline int atomic_load_32(volatile _Atomic int *ptr, memory_order order)
{
    return (int)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_32(volatile _Atomic int *ptr, int val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline int atomic_fetch_add_32(volatile _Atomic int *ptr, int val, memory_order order)
{
    return (int)atomic_fetch_add_explicit(ptr, val, order);
}
static inline int atomic_fetch_sub_32(volatile _Atomic int *ptr, int val, memory_order order)
{
    return (int)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline int atomic_exchange_32(volatile _Atomic int *ptr, int val, memory_order order)
{
    return (int)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_32(volatile _Atomic int *ptr, int *expected,
                                                      int desired, memory_order succ,
                                                      memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}
static inline int64_t atomic_load_64(volatile _Atomic int64_t *ptr, memory_order order)
{
    return (int64_t)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_64(volatile _Atomic int64_t *ptr, int64_t val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline int64_t atomic_fetch_add_64(volatile _Atomic int64_t *ptr, int64_t val,
                                          memory_order order)
{
    return (int64_t)atomic_fetch_add_explicit(ptr, val, order);
}
static inline int64_t atomic_fetch_sub_64(volatile _Atomic int64_t *ptr, int64_t val,
                                          memory_order order)
{
    return (int64_t)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline int64_t atomic_exchange_64(volatile _Atomic int64_t *ptr, int64_t val,
                                         memory_order order)
{
    return (int64_t)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_64(volatile _Atomic int64_t *ptr,
                                                      int64_t *expected, int64_t desired,
                                                      memory_order succ, memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}
static inline char atomic_load_8(volatile _Atomic char *ptr, memory_order order)
{
    return (char)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_8(volatile _Atomic char *ptr, char val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline char atomic_fetch_add_8(volatile _Atomic char *ptr, char val, memory_order order)
{
    return (char)atomic_fetch_add_explicit(ptr, val, order);
}
static inline char atomic_fetch_sub_8(volatile _Atomic char *ptr, char val, memory_order order)
{
    return (char)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline char atomic_exchange_8(volatile _Atomic char *ptr, char val, memory_order order)
{
    return (char)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_8(volatile _Atomic char *ptr, char *expected,
                                                     char desired, memory_order succ,
                                                     memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}
static inline short atomic_load_16(volatile _Atomic short *ptr, memory_order order)
{
    return (short)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_16(volatile _Atomic short *ptr, short val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline short atomic_fetch_add_16(volatile _Atomic short *ptr, short val, memory_order order)
{
    return (short)atomic_fetch_add_explicit(ptr, val, order);
}
static inline short atomic_fetch_sub_16(volatile _Atomic short *ptr, short val, memory_order order)
{
    return (short)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline short atomic_exchange_16(volatile _Atomic short *ptr, short val, memory_order order)
{
    return (short)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_16(volatile _Atomic short *ptr, short *expected,
                                                      short desired, memory_order succ,
                                                      memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}

static inline void *atomic_load_ptr(_Atomic(void *) *ptr, memory_order order)
{
    return (void *)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_ptr(_Atomic(void *) *ptr, void *val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline void *atomic_exchange_ptr(_Atomic(void *) *ptr, void *val, memory_order order)
{
    return (void *)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_ptr(_Atomic(void *) *ptr, void **expected,
                                                       void *desired, memory_order succ,
                                                       memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}

/* Portable name for "atomic void* object" so call sites can cast to
 * (atomic_void_ptr_t *) without spelling _Atomic themselves.  Clang rejects
 * "_Atomic void **" as "pointer to _Atomic void" (ill-formed), and the
 * _Atomic->volatile macro below would corrupt "_Atomic(void *)" when the
 * Windows/fallback branches are active, so each branch defines its own
 * typedef instead. */
typedef _Atomic(void *) atomic_void_ptr_t;

#else


typedef enum {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

#define AIRY_USE_STDATOMIC 0

#endif

/* ====================================================================
 * Windows implementation: Interlocked API
 * ==================================================================== */
#if defined(_WIN32)

#include <intrin.h>
#include <windows.h>

#define _Atomic volatile

/* See C11 branch: call sites cast to (atomic_void_ptr_t *) only. */
typedef void *volatile atomic_void_ptr_t;

static inline char atomic_load_8(volatile char *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_8(volatile char *ptr, char value, memory_order order)
{
    (void)order;
    *ptr = value;
}

static inline char atomic_exchange_8(volatile char *ptr, char desired, memory_order order)
{
    (void)order;
#ifdef _WIN64
    volatile short *p = (volatile short *)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchange16(p, (short)desired);
#else
    return (char)InterlockedExchange8(ptr, (BYTE)desired);
#endif
}

static inline int atomic_compare_exchange_strong_8(volatile char *ptr, char *expected, char desired,
                                                   memory_order success, memory_order failure)
{
    (void)success;
    (void)failure;
#ifdef _WIN64
    char old = *ptr;
    if (old == *expected) {
        *ptr = desired;
        return 1;
    }
    *expected = old;
    return 0;
#else
    char old = (char)InterlockedCompareExchange8(ptr, (BYTE)desired, (BYTE)*expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
#endif
}

static inline char atomic_fetch_add_8(volatile char *ptr, char value, memory_order order)
{
    (void)order;
#ifdef _WIN64
    volatile short *p = (volatile short *)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchangeAdd16(p, (short)value);
#else
    return (char)InterlockedExchangeAdd8(ptr, (char)value);
#endif
}

static inline char atomic_fetch_sub_8(volatile char *ptr, char value, memory_order order)
{
    (void)order;
#ifdef _WIN64
    volatile short *p = (volatile short *)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchangeAdd16(p, -(short)value);
#else
    return (char)InterlockedExchangeAdd8(ptr, -(char)value);
#endif
}


static inline short atomic_load_16(volatile short *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_16(volatile short *ptr, short value, memory_order order)
{
    (void)order;
    *ptr = value;
}

static inline short atomic_exchange_16(volatile short *ptr, short desired, memory_order order)
{
    (void)order;
    return (short)InterlockedExchange16((volatile SHORT *)ptr, desired);
}

static inline int atomic_compare_exchange_strong_16(volatile short *ptr, short *expected,
                                                    short desired, memory_order success,
                                                    memory_order failure)
{
    (void)success;
    (void)failure;
    short old = (short)InterlockedCompareExchange16((volatile SHORT *)ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}

static inline short atomic_fetch_add_16(volatile short *ptr, short value, memory_order order)
{
    (void)order;
    return (short)InterlockedExchangeAdd16((volatile SHORT *)ptr, value);
}

static inline short atomic_fetch_sub_16(volatile short *ptr, short value, memory_order order)
{
    (void)order;
    return (short)InterlockedExchangeAdd16((volatile SHORT *)ptr, -value);
}


static inline long atomic_load_32(volatile long *ptr, memory_order order)
{
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_32(volatile long *ptr, long value, memory_order order)
{
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline long atomic_exchange_32(volatile long *ptr, long desired, memory_order order)
{
    (void)order;
    return (long)InterlockedExchange((volatile LONG *)ptr, desired);
}

static inline int atomic_compare_exchange_strong_32(volatile long *ptr, long *expected,
                                                    long desired, memory_order success,
                                                    memory_order failure)
{
    (void)success;
    (void)failure;
    long old = (long)InterlockedCompareExchange((volatile LONG *)ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}

static inline long atomic_fetch_add_32(volatile long *ptr, long value, memory_order order)
{
    (void)order;
    return (long)InterlockedExchangeAdd((volatile LONG *)ptr, value);
}

static inline long atomic_fetch_sub_32(volatile long *ptr, long value, memory_order order)
{
    (void)order;
    return (long)InterlockedExchangeAdd((volatile LONG *)ptr, -value);
}


static inline __int64 atomic_load_64(volatile __int64 *ptr, memory_order order)
{
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_64(volatile __int64 *ptr, __int64 value, memory_order order)
{
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline __int64 atomic_exchange_64(volatile __int64 *ptr, __int64 desired, memory_order order)
{
    (void)order;
    return (__int64)InterlockedExchange64((volatile LONGLONG *)ptr, desired);
}

static inline int atomic_compare_exchange_strong_64(volatile __int64 *ptr, __int64 *expected,
                                                    __int64 desired, memory_order success,
                                                    memory_order failure)
{
    (void)success;
    (void)failure;
    __int64 old =
        (__int64)InterlockedCompareExchange64((volatile LONGLONG *)ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}

static inline __int64 atomic_fetch_add_64(volatile __int64 *ptr, __int64 value, memory_order order)
{
    (void)order;
    return (__int64)InterlockedExchangeAdd64((volatile LONGLONG *)ptr, value);
}

static inline __int64 atomic_fetch_sub_64(volatile __int64 *ptr, __int64 value, memory_order order)
{
    (void)order;
    return (__int64)InterlockedExchangeAdd64((volatile LONGLONG *)ptr, -value);
}


static inline void *atomic_load_ptr(void *volatile *ptr, memory_order order)
{
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_ptr(void *volatile *ptr, void *value, memory_order order)
{
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline void *atomic_exchange_ptr(void *volatile *ptr, void *desired, memory_order order)
{
    (void)order;
    return InterlockedExchangePointer(ptr, desired);
}

static inline int atomic_compare_exchange_strong_ptr(void *volatile *ptr, void **expected,
                                                     void *desired, memory_order success,
                                                     memory_order failure)
{
    (void)success;
    (void)failure;
    void *old = InterlockedCompareExchangePointer(ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}


#ifdef _WIN64
#define atomic_fetch_add_size(p, v, o) (__int64)atomic_fetch_add_64((__int64 *)(p), (__int64)(v), o)
#define atomic_fetch_sub_size(p, v, o) (__int64)atomic_fetch_sub_64((__int64 *)(p), (__int64)(v), o)
#define atomic_load_size(p, o) (size_t)atomic_load_64((__int64 *)(p), o)
#define atomic_store_size(p, v, o) atomic_store_64((__int64 *)(p), (__int64)(v), o)
#else
#define atomic_fetch_add_size(p, v, o) (long)atomic_fetch_add_32((long *)(p), (long)(v), o)
#define atomic_fetch_sub_size(p, v, o) (long)atomic_fetch_sub_32((long *)(p), (long)(v), o)
#define atomic_load_size(p, o) (size_t)atomic_load_32((long *)(p), o)
#define atomic_store_size(p, v, o) atomic_store_32((long *)(p), (long)(v), o)
#endif


static inline double atomic_load_double(volatile double *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_double(volatile double *ptr, double value, memory_order order)
{
    (void)order;
    *ptr = value;
}

static inline double atomic_fetch_add_double(volatile double *ptr, double value, memory_order order)
{
    (void)order;
    double old = *ptr;
    *ptr += value;
    return old;
}

/* ====================================================================
 * POSIX fallback implementation: GCC/Clang __atomic builtins
 * Used when AIRY_USE_STDATOMIC=0 and not Windows
 * ==================================================================== */
#elif !AIRY_USE_STDATOMIC

#define _Atomic volatile

/* See C11 branch: call sites cast to (atomic_void_ptr_t *) only. */
typedef void *volatile atomic_void_ptr_t;

static inline char atomic_load_8(volatile char *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_8(volatile char *ptr, char value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline char atomic_exchange_8(volatile char *ptr, char desired, memory_order order)
{
    return (char)__atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_8(volatile char *ptr, char *expected, char desired,
                                                   memory_order success, memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline char atomic_fetch_add_8(volatile char *ptr, char value, memory_order order)
{
    return (char)__atomic_fetch_add(ptr, value, (int)order);
}
static inline char atomic_fetch_sub_8(volatile char *ptr, char value, memory_order order)
{
    return (char)__atomic_fetch_sub(ptr, value, (int)order);
}

static inline short atomic_load_16(volatile short *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_16(volatile short *ptr, short value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline short atomic_exchange_16(volatile short *ptr, short desired, memory_order order)
{
    return (short)__atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_16(volatile short *ptr, short *expected,
                                                    short desired, memory_order success,
                                                    memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline short atomic_fetch_add_16(volatile short *ptr, short value, memory_order order)
{
    return (short)__atomic_fetch_add(ptr, value, (int)order);
}
static inline short atomic_fetch_sub_16(volatile short *ptr, short value, memory_order order)
{
    return (short)__atomic_fetch_sub(ptr, value, (int)order);
}

static inline long atomic_load_32(volatile long *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_32(volatile long *ptr, long value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline long atomic_exchange_32(volatile long *ptr, long desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_32(volatile long *ptr, long *expected,
                                                    long desired, memory_order success,
                                                    memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline long atomic_fetch_add_32(volatile long *ptr, long value, memory_order order)
{
    return __atomic_fetch_add(ptr, value, (int)order);
}
static inline long atomic_fetch_sub_32(volatile long *ptr, long value, memory_order order)
{
    return __atomic_fetch_sub(ptr, value, (int)order);
}

static inline int64_t atomic_load_64(volatile int64_t *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_64(volatile int64_t *ptr, int64_t value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline int64_t atomic_exchange_64(volatile int64_t *ptr, int64_t desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_64(volatile int64_t *ptr, int64_t *expected,
                                                    int64_t desired, memory_order success,
                                                    memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline int64_t atomic_fetch_add_64(volatile int64_t *ptr, int64_t value, memory_order order)
{
    return __atomic_fetch_add(ptr, value, (int)order);
}
static inline int64_t atomic_fetch_sub_64(volatile int64_t *ptr, int64_t value, memory_order order)
{
    return __atomic_fetch_sub(ptr, value, (int)order);
}

static inline void *atomic_load_ptr(void *volatile *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_ptr(void *volatile *ptr, void *value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline void *atomic_exchange_ptr(void *volatile *ptr, void *desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_ptr(void *volatile *ptr, void **expected,
                                                     void *desired, memory_order success,
                                                     memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}

#define atomic_fetch_add_size(p, v, o) __atomic_fetch_add(p, v, (int)o)
#define atomic_fetch_sub_size(p, v, o) __atomic_fetch_sub(p, v, (int)o)
#define atomic_load_size(p, o) __atomic_load_n(p, (int)o)
#define atomic_store_size(p, v, o) __atomic_store_n(p, v, (int)o)

static inline double atomic_load_double(volatile double *ptr, memory_order order)
{
    double val;
    __atomic_load((const double *)ptr, &val, (int)order);
    return val;
}
static inline void atomic_store_double(volatile double *ptr, double value, memory_order order)
{
    __atomic_store(ptr, &value, (int)order);
}
static inline double atomic_fetch_add_double(volatile double *ptr, double value, memory_order order)
{
    double old;
    __atomic_load((const double *)ptr, &old, (int)memory_order_relaxed);
    double new_val;
    do {
        new_val = old + value;
    } while (
        !__atomic_compare_exchange_n(ptr, &old, new_val, 0, (int)order, (int)memory_order_relaxed));
    return old;
}

#endif /* Platform selection */

#endif /* AIRY_RT_ATOMIC_COMPAT_PLATFORM_H */
