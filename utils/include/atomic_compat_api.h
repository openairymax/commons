/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * Unified atomic API: type aliases, double/bool/pointer operations, fences.
 * Split from atomic_compat.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_ATOMIC_COMPAT_API_H
#define AIRY_RT_ATOMIC_COMPAT_API_H

#pragma GCC system_header

#if AIRY_USE_STDATOMIC

typedef _Atomic double atomic_double;
typedef _Atomic uint64_t atomic_uint64_t;
typedef _Atomic int64_t atomic_int64_t;
typedef _Atomic size_t atomic_size_t;
typedef _Atomic uint_fast64_t atomic_uint_fast64_t;
typedef _Atomic unsigned long atomic_uint_fast32_t;


static inline double atomic_load_double_fn(_Atomic double *ptr, memory_order order)
{
    return (double)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_double_fn(_Atomic double *ptr, double value, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, value, order);
#pragma GCC diagnostic pop
}
static inline double atomic_fetch_add_double_fn(_Atomic double *ptr, double value,
                                                memory_order order)
{
    double old = (double)atomic_load_explicit(ptr, memory_order_relaxed);
    double new_val;
    do {
        new_val = old + value;
    } while (
        !atomic_compare_exchange_weak_explicit(ptr, &old, new_val, order, memory_order_relaxed));
    return old;
}
static inline double atomic_exchange_double_fn(_Atomic double *ptr, double desired,
                                               memory_order order)
{
    return (double)atomic_exchange_explicit(ptr, desired, order);
}

#define atomic_fetch_add_double(ptr, val, ord) atomic_fetch_add_double_fn(ptr, val, ord)
#define atomic_load_double(ptr, ord) atomic_load_double_fn(ptr, ord)
#define atomic_store_double(ptr, val, ord) atomic_store_double_fn(ptr, val, ord)
#define atomic_exchange_double(ptr, val, ord) atomic_exchange_double_fn(ptr, val, ord)

#else

typedef volatile int atomic_int;
typedef volatile unsigned int atomic_uint;
typedef volatile long atomic_long;
typedef volatile unsigned long atomic_ulong;
typedef volatile int64_t atomic_int64_t;
typedef volatile uint64_t atomic_uint64_t;
typedef volatile size_t atomic_size_t;
typedef volatile double atomic_double;

#ifdef _WIN32
typedef volatile uint64_t atomic_uint_fast64_t;
#else
typedef volatile uint_fast64_t atomic_uint_fast64_t;
#endif
typedef volatile unsigned long atomic_uint_fast32_t;
typedef volatile int atomic_bool;

#endif


#if AIRY_USE_STDATOMIC

static inline _Bool atomic_load_bool(_Atomic _Bool *ptr, memory_order order)
{
    return (_Bool)atomic_load_explicit(ptr, order);
}

static inline void atomic_store_bool(_Atomic _Bool *ptr, _Bool value, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, value, order);
#pragma GCC diagnostic pop
}

static inline _Bool atomic_exchange_bool(_Atomic _Bool *ptr, _Bool desired, memory_order order)
{
    return (_Bool)atomic_exchange_explicit(ptr, desired, order);
}

#elif defined(_WIN32)

static inline int atomic_load_bool(volatile int *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_bool(volatile int *ptr, int value, memory_order order)
{
    (void)order;
    *ptr = value ? 1 : 0;
}

static inline int atomic_exchange_bool(volatile int *ptr, int desired, memory_order order)
{
    (void)order;
    return InterlockedExchange((volatile LONG *)ptr, desired ? 1 : 0);
}

#else

static inline int atomic_load_bool(volatile int *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}

static inline void atomic_store_bool(volatile int *ptr, int value, memory_order order)
{
    __atomic_store_n(ptr, value ? 1 : 0, (int)order);
}

static inline int atomic_exchange_bool(volatile int *ptr, int desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired ? 1 : 0, (int)order);
}

#endif


#if !AIRY_USE_STDATOMIC
#define atomic_init(ptr, val) (*(ptr) = (val))

#define atomic_load_explicit(ptr, order)                                           \
    (sizeof(*(ptr)) == 1 ? (int)atomic_load_8((volatile char *)(ptr), order) :     \
     sizeof(*(ptr)) == 2 ? (int)atomic_load_16((volatile short *)(ptr), order) :   \
     sizeof(*(ptr)) == 4 ? (int)atomic_load_32((volatile long *)(ptr), order) :    \
     sizeof(*(ptr)) == 8 ? (int)atomic_load_64((volatile int64_t *)(ptr), order) : \
                           *(ptr))

#define atomic_store_explicit(ptr, val, order)                                                 \
    (sizeof(*(ptr)) == 1 ? atomic_store_8((volatile char *)(ptr), (char)(val), order) :        \
     sizeof(*(ptr)) == 2 ? atomic_store_16((volatile short *)(ptr), (short)(val), order) :     \
     sizeof(*(ptr)) == 4 ? atomic_store_32((volatile long *)(ptr), (long)(val), order) :       \
     sizeof(*(ptr)) == 8 ? atomic_store_64((volatile int64_t *)(ptr), (int64_t)(val), order) : \
                           (*(ptr) = (val)))

#define atomic_load(ptr) atomic_load_explicit(ptr, memory_order_seq_cst)
#define atomic_store(ptr, val) atomic_store_explicit(ptr, val, memory_order_seq_cst)

#define atomic_exchange(ptr, val)                                                          \
    (sizeof(*(ptr)) == 1 ?                                                                 \
         (int)atomic_exchange_8((char *)(ptr), (char)(val), memory_order_seq_cst) :        \
     sizeof(*(ptr)) == 2 ?                                                                 \
         (int)atomic_exchange_16((short *)(ptr), (short)(val), memory_order_seq_cst) :     \
     sizeof(*(ptr)) == 4 ?                                                                 \
         (int)atomic_exchange_32((long *)(ptr), (long)(val), memory_order_seq_cst) :       \
     sizeof(*(ptr)) == 8 ?                                                                 \
         (int)atomic_exchange_64((int64_t *)(ptr), (int64_t)(val), memory_order_seq_cst) : \
         *(ptr))

#define atomic_compare_exchange_strong(ptr, expected, desired)                                    \
    (sizeof(*(ptr)) == 1 ?                                                                        \
         atomic_compare_exchange_strong_8((char *)(ptr), (char *)(expected), (char)(desired),     \
                                          memory_order_seq_cst, memory_order_seq_cst) :           \
     sizeof(*(ptr)) == 2 ?                                                                        \
         atomic_compare_exchange_strong_16((short *)(ptr), (short *)(expected), (short)(desired), \
                                           memory_order_seq_cst, memory_order_seq_cst) :          \
     sizeof(*(ptr)) == 4 ?                                                                        \
         atomic_compare_exchange_strong_32((long *)(ptr), (long *)(expected), (long)(desired),    \
                                           memory_order_seq_cst, memory_order_seq_cst) :          \
     sizeof(*(ptr)) == 8 ?                                                                        \
         atomic_compare_exchange_strong_64((int64_t *)(ptr), (int64_t *)(expected),               \
                                           (int64_t)(desired), memory_order_seq_cst,              \
                                           memory_order_seq_cst) :                                \
         0)

#define atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail)          \
    (sizeof(*(ptr)) == 1 ?                                                                   \
         atomic_compare_exchange_strong_8((volatile char *)(ptr), (char *)(expected),        \
                                          (char)(desired), succ, fail) :                     \
     sizeof(*(ptr)) == 2 ?                                                                   \
         atomic_compare_exchange_strong_16((volatile short *)(ptr), (short *)(expected),     \
                                           (short *)(desired), succ, fail) :                 \
     sizeof(*(ptr)) == 4 ?                                                                   \
         atomic_compare_exchange_strong_32((volatile long *)(ptr), (long *)(expected),       \
                                           (long)(desired), succ, fail) :                    \
     sizeof(*(ptr)) == 8 ?                                                                   \
         atomic_compare_exchange_strong_64((volatile int64_t *)(ptr), (int64_t *)(expected), \
                                           (int64_t *)(desired), succ, fail) :               \
         0)

#define atomic_fetch_add(ptr, val)                                                          \
    (sizeof(*(ptr)) == 1 ?                                                                  \
         (int)atomic_fetch_add_8((char *)(ptr), (char)(val), memory_order_seq_cst) :        \
     sizeof(*(ptr)) == 2 ?                                                                  \
         (int)atomic_fetch_add_16((short *)(ptr), (short)(val), memory_order_seq_cst) :     \
     sizeof(*(ptr)) == 4 ?                                                                  \
         (int)atomic_fetch_add_32((long *)(ptr), (long)(val), memory_order_seq_cst) :       \
     sizeof(*(ptr)) == 8 ?                                                                  \
         (int)atomic_fetch_add_64((int64_t *)(ptr), (int64_t)(val), memory_order_seq_cst) : \
         0)

#define atomic_fetch_sub(ptr, val)                                                          \
    (sizeof(*(ptr)) == 1 ?                                                                  \
         (int)atomic_fetch_sub_8((char *)(ptr), (char)(val), memory_order_seq_cst) :        \
     sizeof(*(ptr)) == 2 ?                                                                  \
         (int)atomic_fetch_sub_16((short *)(ptr), (short)(val), memory_order_seq_cst) :     \
     sizeof(*(ptr)) == 4 ?                                                                  \
         (int)atomic_fetch_sub_32((long *)(ptr), (long)(val), memory_order_seq_cst) :       \
     sizeof(*(ptr)) == 8 ?                                                                  \
         (int)atomic_fetch_sub_64((int64_t *)(ptr), (int64_t)(val), memory_order_seq_cst) : \
         0)

#define atomic_fetch_add_explicit(ptr, val, order)                                                \
    (sizeof(*(ptr)) == 1 ? (int)atomic_fetch_add_8((volatile char *)(ptr), (char)(val), order) :  \
     sizeof(*(ptr)) == 2 ? (int)atomic_fetch_add_16((volatile short *)(ptr), (short)(val),        \
                                                    order) :                                      \
     sizeof(*(ptr)) == 4 ? (int)atomic_fetch_add_32((volatile long *)(ptr), (long)(val), order) : \
     sizeof(*(ptr)) == 8 ? (int)atomic_fetch_add_64((volatile int64_t *)(ptr), (int64_t)(val),    \
                                                    order) :                                      \
                           0)

#define atomic_fetch_sub_explicit(ptr, val, order)                                                \
    (sizeof(*(ptr)) == 1 ? (int)atomic_fetch_sub_8((volatile char *)(ptr), (char)(val), order) :  \
     sizeof(*(ptr)) == 2 ? (int)atomic_fetch_sub_16((volatile short *)(ptr), (short)(val),        \
                                                    order) :                                      \
     sizeof(*(ptr)) == 4 ? (int)atomic_fetch_sub_32((volatile long *)(ptr), (long)(val), order) : \
     sizeof(*(ptr)) == 8 ? (int)atomic_fetch_sub_64((volatile int64_t *)(ptr), (int64_t)(val),    \
                                                    order) :                                      \
                           0)
#endif /* !AIRY_USE_STDATOMIC */


#if AIRY_USE_STDATOMIC

/* When using system <stdatomic.h>, atomic_thread_fence is already provided
 * as a macro: #define atomic_thread_fence(MO) __atomic_thread_fence(MO)
 * Do NOT redefine it as a static inline function here, because the macro
 * would expand the function name, creating infinite recursion:
 *   static inline void __atomic_thread_fence(memory_order order) {
 *       __atomic_thread_fence((int)order);  // calls itself!
 *   }
 */

#elif defined(_WIN32)

static inline void atomic_thread_fence(memory_order order)
{
    (void)order;
    MemoryBarrier();
}

#else

static inline void atomic_thread_fence(memory_order order)
{
    __atomic_thread_fence((int)order);
}

#endif

#endif /* AIRY_RT_ATOMIC_COMPAT_API_H */
