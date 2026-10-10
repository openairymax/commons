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
 * @brief Synchronization primitive types
 */
typedef enum {
    SYNC_TYPE_UNKNOWN = 0,
    SYNC_TYPE_MUTEX,
    SYNC_TYPE_RWLOCK,
    SYNC_TYPE_CONDITION
} sync_type_t;

/**
 * @brief Lock object types (for type-safe calls to sync_get_type)
 */
typedef enum {
    SYNC_LOCK_MUTEX,
    SYNC_LOCK_RWLOCK,
    SYNC_LOCK_CONDITION
} sync_lock_type_t;

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
    SYNC_FLAG_SHARED = 1 << 0,
    SYNC_FLAG_EXCLUSIVE = 1 << 1,
    SYNC_FLAG_TRY = 1 << 2,
    SYNC_FLAG_TIMEOUT = 1 << 3,
    SYNC_FLAG_RECURSIVE = 1 << 4,
    SYNC_FLAG_ERROR_CHECK = 1 << 5,
    SYNC_FLAG_PRIORITY_INHERIT = 1 << 6,
    SYNC_FLAG_ROBUST = 1 << 7
} sync_flag_t;

/**
 * @brief Lock option identifiers (for sync_set_option / sync_get_option)
 */
typedef enum {
    SYNC_OPTION_NAME = 1,
    SYNC_OPTION_TIMEOUT = 2,
    SYNC_OPTION_PRIORITY_INHERIT = 3,
    SYNC_OPTION_ROBUST = 4
} sync_option_t;

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
    sync_type_t type;
    uint32_t flags;
    const char *name;
    void *context;
} sync_attr_t;

/**
 * @brief Lock statistics
 */
typedef struct {
    size_t lock_count;
    size_t unlock_count;
    size_t wait_count;
    size_t timeout_count;
    size_t deadlock_count;
    uint64_t total_wait_time_ms;
    uint64_t max_wait_time_ms;
} sync_stats_t;

/**
 * @brief Deadlock detection information
 */
typedef struct {
    size_t thread_count;
    size_t lock_count;
    uint64_t detection_time;
    char **thread_names;
    char **lock_names;
} sync_deadlock_info_t;

/**
 * @brief Error callback function type
 *
 * @param[in] result Error result
 * @param[in] lock_name Lock name
 * @param[in] context User context
 */
typedef void (*sync_error_callback_t)(sync_result_t result, const char *lock_name, void *context);

/**
 * @brief Initialize the synchronization module
 *
 * @param[in] error_callback Error callback (optional)
 * @param[in] context User context (optional)
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_init(sync_error_callback_t error_callback, void *context);

/**
 * @brief Clean up the synchronization module
 */
void sync_cleanup(void);

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
 * @brief Get lock statistics
 *
 * @param[in] lock Lock handle (any type)
 * @param[out] stats Statistics
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_get_stats(void *lock, sync_stats_t *stats);

/**
 * @brief Reset lock statistics
 *
 * @param[in] lock Lock handle
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_reset_stats(void *lock);

/**
 * @brief Check for deadlocks
 *
 * @param[out] info Deadlock information (if detected)
 * @param[in] max_info_size Maximum information size
 * @return SYNC_ERROR_DEADLOCK if a deadlock is detected, otherwise SYNC_SUCCESS
 */
sync_result_t sync_check_deadlock(sync_deadlock_info_t *info, size_t max_info_size);

/**
 * @brief Set a lock name
 *
 * @param[in] lock Lock handle
 * @param[in] name Lock name
 * @return SYNC_SUCCESS on success, error code on failure
 */
sync_result_t sync_set_name(void *lock, const char *name);

/**
 * @brief Get a lock name
 *
 * @param[in] lock Lock handle
 * @return Lock name (NULL if not set)
 */
const char *sync_get_name(void *lock);

/**
 * @brief Get the current thread ID
 *
 * @return Thread ID
 */
uint64_t sync_get_thread_id(void);

/**
 * @brief Get the lock type
 *
 * @param[in] lock Lock handle
 * @param[in] lock_type Actual lock type (for safe conversion)
 * @return Lock type identifier
 * @note The caller must ensure lock and lock_type match, otherwise the
 *       behavior is undefined
 */
sync_type_t sync_get_type(void *lock, sync_lock_type_t lock_type);

/**
 * @brief Sleep the current thread
 *
 * @param[in] ms Sleep time in milliseconds
 */
void sync_sleep(unsigned int ms);

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

/**
 * @brief Set a lock option
 *
 * Supported options:
 * - SYNC_OPTION_NAME: set the lock name (value is const char*)
 * - SYNC_OPTION_TIMEOUT: set the default timeout (value is uint64_t*, ms)
 * - SYNC_OPTION_PRIORITY_INHERIT: set priority inheritance (value is bool*)
 * - SYNC_OPTION_ROBUST: set the robust-lock configuration (value is bool*)
 *
 * @param[in] lock Lock handle (any type)
 * @param[in] option Option identifier
 * @param[in] value Option value pointer
 * @return SYNC_SUCCESS on success, error code on failure
 *
 * @threadsafe Yes
 */
sync_result_t sync_set_option(void *lock, int option, void *value);

/**
 * @brief Get a lock option
 *
 * Supported options:
 * - SYNC_OPTION_NAME: get the lock name (value is const char**)
 * - SYNC_OPTION_TIMEOUT: get the default timeout (value is uint64_t*, ms)
 * - SYNC_OPTION_PRIORITY_INHERIT: get the priority inheritance setting (value is bool*)
 * - SYNC_OPTION_ROBUST: get the robust-lock configuration (value is bool*)
 *
 * @param[in] lock Lock handle (any type)
 * @param[in] option Option identifier
 * @param[out] value Buffer for the option value
 * @return SYNC_SUCCESS on success, error code on failure
 *
 * @threadsafe Yes
 */
sync_result_t sync_get_option(void *lock, int option, void *value);

/** @} */ /* end of sync_api */
AIRY_ABI_END

#endif /* AIRY_RT_SYNC_H */
