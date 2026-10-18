/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file sync.h
 * @brief Unified thread-synchronization primitives module: core-layer API.
 *
 * Provides cross-platform, safe, efficient thread-synchronization
 * primitives: mutexes, condition variables and read-write locks.
 * Supports Windows and POSIX systems.
 *
 * @note Thread safety: all public interfaces are thread-safe
 * @see ARCHITECTURAL_PRINCIPLES.md E-3 resource-determinism principle
 */

#ifndef AIRY_RT_SYNC_H
#define AIRY_RT_SYNC_H

#include "airy_abi.h"

AIRY_ABI_BEGIN

/**
 * @defgroup sync_api Thread synchronization API
 * @{
 */

/**
 * @brief Lock operation results
 */
typedef enum {
    SYNC_SUCCESS = 0,
    SYNC_ERROR_TIMEOUT,
    SYNC_ERROR_DEADLOCK,
    SYNC_ERROR_INVALID,
    SYNC_ERROR_MEMORY,
    SYNC_ERROR_PERMISSION,
    SYNC_ERROR_BUSY,
    SYNC_ERROR_UNSUPPORTED,
    SYNC_ERROR_UNKNOWN
} sync_result_t;

/**
 * @brief Lock option flags
 */
typedef enum {
    SYNC_FLAG_NONE = 0,
    SYNC_FLAG_RECURSIVE = 1 << 4
} sync_flag_t;

/**
 * @brief Timeout options
 */
typedef struct {
    uint64_t timeout_ms;
    bool absolute;
} sync_timeout_t;

/**
 * @brief Mutex handle (opaque type)
 */
typedef struct sync_mutex *sync_mutex_t;

/**
 * @brief Read-write lock handle
 */
typedef struct sync_rwlock *sync_rwlock_t;

/**
 * @brief Condition variable handle
 */
typedef struct sync_condition *sync_condition_t;

/**
 * @brief Lock attributes
 */
typedef struct {
    uint32_t flags;
} sync_attr_t;

/**
 * @brief Create a mutex
 *
 * @param[out] mutex Mutex handle
 * @param[in] attr Lock attributes (optional)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_mutex_create(sync_mutex_t *mutex, const sync_attr_t *attr);

/**
 * @brief Destroy a mutex
 *
 * @param[in] mutex Mutex handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_mutex_free(sync_mutex_t mutex);

/**
 * @brief Lock a mutex
 *
 * @param[in] mutex Mutex handle
 * @param[in] timeout Timeout setting (optional, NULL waits indefinitely)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_mutex_lock_ex(sync_mutex_t mutex, const sync_timeout_t *timeout);

/**
 * @brief Try to lock a mutex
 *
 * @param[in] mutex Mutex handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_mutex_try_lock(sync_mutex_t mutex);

/**
 * @brief Unlock a mutex
 *
 * @param[in] mutex Mutex handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_mutex_unlock_ex(sync_mutex_t mutex);

/**
 * @brief Create a read-write lock
 *
 * @param[out] rwlock Read-write lock handle
 * @param[in] attr Lock attributes (optional)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_rwlock_create(sync_rwlock_t *rwlock, const sync_attr_t *attr);

/**
 * @brief Destroy a read-write lock
 *
 * @param[in] rwlock Read-write lock handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_rwlock_free(sync_rwlock_t rwlock);

/**
 * @brief Acquire the read lock (shared)
 *
 * @param[in] rwlock Read-write lock handle
 * @param[in] timeout Timeout setting (optional)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_rwlock_read_lock_ex(sync_rwlock_t rwlock, const sync_timeout_t *timeout);

/**
 * @brief Try to acquire the read lock
 *
 * @param[in] rwlock Read-write lock handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_rwlock_try_read_lock(sync_rwlock_t rwlock);

/**
 * @brief Acquire the write lock (exclusive)
 *
 * @param[in] rwlock Read-write lock handle
 * @param[in] timeout Timeout setting (optional)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_rwlock_write_lock_ex(sync_rwlock_t rwlock, const sync_timeout_t *timeout);

/**
 * @brief Try to acquire the write lock
 *
 * @param[in] rwlock Read-write lock handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_rwlock_try_write_lock(sync_rwlock_t rwlock);

/**
 * @brief Unlock a read-write lock
 *
 * @param[in] rwlock Read-write lock handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_rwlock_unlock_ex(sync_rwlock_t rwlock);

/**
 * @brief Create a condition variable
 *
 * @param[out] condition Condition variable handle
 * @param[in] attr Condition variable attributes (optional)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_condition_create(sync_condition_t *condition, const sync_attr_t *attr);

/**
 * @brief Destroy a condition variable
 *
 * @param[in] condition Condition variable handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_condition_free(sync_condition_t condition);

/**
 * @brief Wait on a condition variable
 *
 * @param[in] condition Condition variable handle
 * @param[in] mutex Associated mutex
 * @param[in] timeout Timeout setting (optional)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_condition_wait_ex(sync_condition_t condition, sync_mutex_t mutex,
                                     const sync_timeout_t *timeout);

/**
 * @brief Wake one thread waiting on the condition variable
 *
 * @param[in] condition Condition variable handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_condition_signal_ex(sync_condition_t condition);

/**
 * @brief Wake all threads waiting on the condition variable
 *
 * @param[in] condition Condition variable handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_condition_broadcast_ex(sync_condition_t condition);

/**
 * @brief Get the current thread ID
 *
 * @return Thread ID
 */
uint64_t sync_get_thread_id(void);

/**
 * @brief Atomic operation: compare-and-swap
 *
 * @param[inout] ptr Pointer
 * @param[in] expected Expected value
 * @param[in] desired Desired value
 * @return true on success, false on failure
 */
bool sync_atomic_cas(volatile void *ptr, uintptr_t expected, uintptr_t desired);

/**
 * @brief Atomic operation: add
 *
 * @param[inout] ptr Pointer
 * @param[in] value Value to add
 * @return Value before the addition
 */
uintptr_t sync_atomic_add(volatile void *ptr, uintptr_t value);

/**
 * @brief Atomic operation: subtract
 *
 * @param[inout] ptr Pointer
 * @param[in] value Value to subtract
 * @return Value before the subtraction
 */
uintptr_t sync_atomic_sub(volatile void *ptr, uintptr_t value);

/**
 * @brief Atomic operation: load
 *
 * @param[in] ptr Pointer
 * @return Current value
 */
uintptr_t sync_atomic_load(volatile void *ptr);

/**
 * @brief Atomic operation: store
 *
 * @param[inout] ptr Pointer
 * @param[in] value Value to store
 */
void sync_atomic_store(volatile void *ptr, uintptr_t value);

/** @} */ /* end of sync_api */
AIRY_ABI_END

#endif /* AIRY_RT_SYNC_H */
