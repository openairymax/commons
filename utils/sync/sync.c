// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file sync.c
 * @brief Unified thread synchronization primitives - core layer.
 *
 * Provides cross-platform, safe, efficient thread synchronization
 * primitives for Windows and POSIX: mutex, rwlock and condition variable.
 *
 * @note This file is the module entry point; implementations are split
 *       across:
 *       - sync_mutex.c: mutex
 *       - sync_rwlock.c: rwlock
 *       - sync_condition.c: condition variable
 */

#include "sync.h"

#include "platform_process.h"

#ifdef _WIN32
#include <windows.h>
#endif

uint64_t sync_get_thread_id(void)
{
    return airy_thread_id();
}

bool sync_atomic_cas(volatile void *ptr, uintptr_t expected, uintptr_t desired)
{
#ifdef _WIN32
    /* 按指针位宽选择 Interlocked 变体：32 位 Windows 下 uintptr_t 为 32 位，
     * 硬编码 _Interlocked*64 会按 8 字节读写造成越界与语义错误 */
#if defined(_WIN64)
    return _InterlockedCompareExchange64((volatile LONG64 *)ptr, (LONG64)desired,
                                         (LONG64)expected) == (LONG64)expected;
#else
    return _InterlockedCompareExchange((volatile LONG *)ptr, (LONG)desired,
                                       (LONG)expected) == (LONG)expected;
#endif
#else
    return __sync_bool_compare_and_swap((volatile uintptr_t *)ptr, expected, desired);
#endif
}

uintptr_t sync_atomic_add(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
#if defined(_WIN64)
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, (LONG64)value);
#else
    return (uintptr_t)_InterlockedExchangeAdd((volatile LONG *)ptr, (LONG)value);
#endif
#else
    return __sync_fetch_and_add((volatile uintptr_t *)ptr, value);
#endif
}

uintptr_t sync_atomic_sub(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
#if defined(_WIN64)
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, -(LONG64)value);
#else
    return (uintptr_t)_InterlockedExchangeAdd((volatile LONG *)ptr, -(LONG)value);
#endif
#else
    return __sync_fetch_and_sub((volatile uintptr_t *)ptr, value);
#endif
}

uintptr_t sync_atomic_load(volatile void *ptr)
{
#ifdef _WIN32
#if defined(_WIN64)
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, 0);
#else
    return (uintptr_t)_InterlockedExchangeAdd((volatile LONG *)ptr, 0);
#endif
#else
    return __sync_fetch_and_add((volatile uintptr_t *)ptr, 0);
#endif
}

void sync_atomic_store(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
#if defined(_WIN64)
    _InterlockedExchange64((volatile LONG64 *)ptr, (LONG64)value);
#else
    _InterlockedExchange((volatile LONG *)ptr, (LONG)value);
#endif
#else
    __sync_lock_test_and_set((volatile uintptr_t *)ptr, value);
#endif
}
