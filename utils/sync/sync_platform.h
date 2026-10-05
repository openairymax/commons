/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file sync_platform.h
 * @brief Synchronization module platform abstraction layer (internal use).
 *
 * Provides the underlying cross-platform implementation abstraction for
 * the synchronization primitives. Supports Windows and POSIX systems.
 */

#ifndef AIRY_RT_SYNC_PLATFORM_H
#define AIRY_RT_SYNC_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup sync_platform
 * @{
 */


#ifdef _WIN32
#include "atomic_compat.h"

#include <synchapi.h>
#include <windows.h>


typedef CRITICAL_SECTION platform_mutex_t;


typedef SRWLOCK platform_rwlock_t;


typedef atomic_int platform_spinlock_t;


typedef HANDLE platform_semaphore_t;


typedef CONDITION_VARIABLE platform_condition_t;


#else
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(__MACH__)
#include "atomic_compat.h"
#endif


typedef pthread_mutex_t platform_mutex_t;


typedef pthread_rwlock_t platform_rwlock_t;


/* macOS 无 pthread_spinlock_t：使用 C11 atomic_int CAS 自旋，
 * 与 Windows 分支实现策略对齐。 */
#if defined(__APPLE__) && defined(__MACH__)
typedef atomic_int platform_spinlock_t;
#else
typedef pthread_spinlock_t platform_spinlock_t;
#endif


typedef sem_t platform_semaphore_t;


typedef pthread_cond_t platform_condition_t;


#endif /* _WIN32 */
/**
 * @brief Initialize a platform mutex, optionally recursive
 * @param[in] mutex Mutex pointer
 * @param[in] recursive true to initialize a recursive mutex
 * @return 0 on success, non-zero on failure
 */
int platform_mtx_init(platform_mutex_t *mutex, bool recursive);

/**
 * @brief Initialize a platform mutex
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_init(platform_mutex_t *mutex);

/**
 * @brief Destroy a platform mutex
 * @param[in] mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_destroy(platform_mutex_t *mutex);

/**
 * @brief Lock a platform mutex
 * @param[in] mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_lock(platform_mutex_t *mutex);

/**
 * @brief Unlock a platform mutex
 * @param[in] mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_unlock(platform_mutex_t *mutex);

/**
 * @brief Try to lock a platform mutex
 * @param[in] mutex Mutex pointer
 * @return 0 on success, non-zero on failure/busy
 */
int platform_mutex_trylock(platform_mutex_t *mutex);

/**
 * @brief Lock a platform mutex with timeout
 * @param[in] mutex Mutex pointer
 * @param[in] timeout_ms Timeout in milliseconds
 * @return 0 on success, ETIMEDOUT on timeout, other non-zero on failure
 */
int platform_mtx_timed(platform_mutex_t *mutex, uint32_t timeout_ms);

/**
 * @brief Initialize a platform read-write lock
 * @return 0 on success, non-zero on failure
 */
int platform_rwlock_init(platform_rwlock_t *rwlock);

/**
 * @brief Destroy a platform read-write lock
 * @param[in] rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int platform_rwlock_destroy(platform_rwlock_t *rwlock);

/**
 * @brief Acquire a platform read lock
 * @param[in] rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int platform_rwlock_rdlock(platform_rwlock_t *rwlock);

/**
 * @brief Acquire a platform write lock
 * @param[in] rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int platform_rwlock_wrlock(platform_rwlock_t *rwlock);

/**
 * @brief Try to acquire a platform read lock
 * @param[in] rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure/busy
 */
int platform_rwlock_tryrdlock(platform_rwlock_t *rwlock);

/**
 * @brief Try to acquire a platform write lock
 * @param[in] rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure/busy
 */
int platform_rwlock_trywrlock(platform_rwlock_t *rwlock);

/**
 * @brief Acquire a platform read or write lock with timeout
 * @param[in] rwlock Read-write lock pointer
 * @param[in] write true for write lock, false for read lock
 * @param[in] timeout_ms Timeout in milliseconds
 * @return 0 on success, ETIMEDOUT on timeout, other non-zero on failure
 */
int platform_rw_timed(platform_rwlock_t *rwlock, bool write, uint32_t timeout_ms);

/**
 * @brief Unlock a platform read-write lock
 * @param[in] rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int platform_rwlock_unlock(platform_rwlock_t *rwlock);

/**
 * @brief Initialize a platform spinlock
 * @return 0 on success, non-zero on failure
 */
int platform_spinlock_init(platform_spinlock_t *spinlock);

/**
 * @brief Destroy a platform spinlock
 * @param[in] spinlock Spinlock pointer
 * @return 0 on success, non-zero on failure
 */
int platform_spinlock_destroy(platform_spinlock_t *spinlock);

/**
 * @brief Lock a platform spinlock
 * @param[in] spinlock Spinlock pointer
 * @return 0 on success, non-zero on failure
 */
int platform_spinlock_lock(platform_spinlock_t *spinlock);

/**
 * @brief Unlock a platform spinlock
 * @param[in] spinlock Spinlock pointer
 * @return 0 on success, non-zero on failure
 */
int platform_spinlock_unlock(platform_spinlock_t *spinlock);

/**
 * @brief Initialize a platform semaphore
 * @param[in] semaphore Semaphore pointer
 * @param[in] value Initial value
 * @return 0 on success, non-zero on failure
 */
int platform_semaphore_init(platform_semaphore_t *semaphore, unsigned int value);

/**
 * @brief Destroy a platform semaphore
 * @param[in] semaphore Semaphore pointer
 * @return 0 on success, non-zero on failure
 */
int platform_semaphore_destroy(platform_semaphore_t *semaphore);

/**
 * @brief Wait on a platform semaphore (P operation)
 * @param[in] semaphore Semaphore pointer
 * @return 0 on success, non-zero on failure
 */
int platform_semaphore_wait(platform_semaphore_t *semaphore);

/**
 * @brief Post a platform semaphore (V operation)
 * @param[in] semaphore Semaphore pointer
 * @return 0 on success, non-zero on failure
 */
int platform_semaphore_post(platform_semaphore_t *semaphore);

/**
 * @brief Timed wait on a platform semaphore
 * @param[in] semaphore Semaphore pointer
 * @param[in] timeout_ms Timeout in milliseconds
 * @return 0 on success, non-zero on failure/timeout
 */
int platform_semaphore_timedwait(platform_semaphore_t *semaphore, uint32_t timeout_ms);

/**
 * @brief Try to wait on a platform semaphore
 * @param[in] semaphore Semaphore pointer
 * @return 0 on success, non-zero on failure/busy
 */
int platform_semaphore_trywait(platform_semaphore_t *semaphore);

/**
 * @brief Initialize a platform condition variable
 * @return 0 on success, non-zero on failure
 */
int platform_condition_init(platform_condition_t *cond);

/**
 * @brief Destroy a platform condition variable
 * @param[in] cond Condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int platform_condition_destroy(platform_condition_t *cond);

/**
 * @brief Wait on a platform condition variable
 * @param[in] cond Condition variable pointer
 * @param[in] mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int platform_condition_wait(platform_condition_t *cond, platform_mutex_t *mutex);

/**
 * @brief Timed wait on a platform condition variable
 * @param[in] cond Condition variable pointer
 * @param[in] mutex Mutex pointer
 * @param[in] timeout_ms Timeout in milliseconds
 * @return 0 on success, non-zero on failure/timeout
 */
int platform_condition_timedwait(platform_condition_t *cond, platform_mutex_t *mutex,
                                 uint32_t timeout_ms);

/**
 * @brief Wake one thread waiting on the condition variable
 * @param[in] cond Condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int platform_condition_signal(platform_condition_t *cond);

/**
 * @brief Wake all threads waiting on the condition variable
 * @param[in] cond Condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int platform_condition_broadcast(platform_condition_t *cond);

/**
 * @brief Get the current thread ID
 * @return Thread ID
 */
uint64_t platform_get_thread_id(void);

/** @} */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SYNC_PLATFORM_H */
