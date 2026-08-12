/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file sync_common.h
 * @brief Common synchronization definitions.
 *
 * Provides shared synchronization functionality -- mutexes, condition
 * variables, semaphores, etc. -- reducing duplication of
 * synchronization-related code.
 */

#ifndef SYNC_COMMON_H
#define SYNC_COMMON_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mutex structure
 */
typedef struct {
    void *mutex;
    bool initialized;
} sync_mutex_t;

/**
 * @brief Condition variable structure
 */
typedef struct {
    void *cond;
    bool initialized;
} sync_cond_t;

/**
 * @brief Semaphore structure
 */
typedef struct {
    void *sem;
    bool initialized;
    uint32_t value;
} sync_sem_t;

/**
 * @brief Read-write lock structure
 */
typedef struct {
    void *rwlock;
    bool initialized;
} sync_rwlock_t;

/**
 * @brief Initialize a mutex
 * @param mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int sync_mutex_init(sync_mutex_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex Mutex pointer
 */
void sync_mutex_destroy(sync_mutex_t *mutex);

/**
 * @brief Lock a mutex
 * @param mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int sync_mutex_lock(sync_mutex_t *mutex);

/**
 * @brief Unlock a mutex
 * @param mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int sync_mutex_unlock(sync_mutex_t *mutex);

/**
 * @brief Try to lock a mutex
 * @param mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int sync_mutex_trylock(sync_mutex_t *mutex);

/**
 * @brief Initialize a condition variable
 * @param cond Condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int sync_cond_init(sync_cond_t *cond);

/**
 * @brief Destroy a condition variable
 * @param cond Condition variable pointer
 */
void sync_cond_destroy(sync_cond_t *cond);

/**
 * @brief Wait on a condition variable
 * @param cond Condition variable pointer
 * @param mutex Mutex pointer
 * @return 0 on success, non-zero on failure
 */
int sync_cond_wait(sync_cond_t *cond, sync_mutex_t *mutex);

/**
 * @brief Timed wait on a condition variable
 * @param cond Condition variable pointer
 * @param mutex Mutex pointer
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, non-zero on failure
 */
int sync_cond_timedwait(sync_cond_t *cond, sync_mutex_t *mutex, uint32_t timeout_ms);

/**
 * @brief Wake one thread waiting on the condition variable
 * @param cond Condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int sync_cond_signal(sync_cond_t *cond);

/**
 * @brief Wake all threads waiting on the condition variable
 * @param cond Condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int sync_cond_broadcast(sync_cond_t *cond);

/**
 * @brief Initialize a semaphore
 * @param sem Semaphore pointer
 * @param value Initial value
 * @return 0 on success, non-zero on failure
 */
int sync_sem_init(sync_sem_t *sem, uint32_t value);

/**
 * @brief Destroy a semaphore
 * @param sem Semaphore pointer
 */
void sync_sem_destroy(sync_sem_t *sem);

/**
 * @brief Wait on a semaphore
 * @param sem Semaphore pointer
 * @return 0 on success, non-zero on failure
 */
int sync_sem_wait(sync_sem_t *sem);

/**
 * @brief Timed wait on a semaphore
 * @param sem Semaphore pointer
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, non-zero on failure
 */
int sync_sem_timedwait(sync_sem_t *sem, uint32_t timeout_ms);

/**
 * @brief Try to wait on a semaphore
 * @param sem Semaphore pointer
 * @return 0 on success, non-zero on failure
 */
int sync_sem_trywait(sync_sem_t *sem);

/**
 * @brief Post a semaphore
 * @param sem Semaphore pointer
 * @return 0 on success, non-zero on failure
 */
int sync_sem_post(sync_sem_t *sem);

/**
 * @brief Get a semaphore's current value
 * @param sem Semaphore pointer
 * @param value Output parameter for the current value
 * @return 0 on success, non-zero on failure
 */
int sync_sem_getvalue(sync_sem_t *sem, uint32_t *value);

/**
 * @brief Initialize a read-write lock
 * @param rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int sync_rwlock_init(sync_rwlock_t *rwlock);

/**
 * @brief Destroy a read-write lock
 * @param rwlock Read-write lock pointer
 */
void sync_rwlock_destroy(sync_rwlock_t *rwlock);

/**
 * @brief Acquire the read lock
 * @param rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int sync_rwlock_rdlock(sync_rwlock_t *rwlock);

/**
 * @brief Acquire the write lock
 * @param rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int sync_rwlock_wrlock(sync_rwlock_t *rwlock);

/**
 * @brief Try to acquire the read lock
 * @param rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int sync_rwlock_tryrdlock(sync_rwlock_t *rwlock);

/**
 * @brief Try to acquire the write lock
 * @param rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int sync_rwlock_trywrlock(sync_rwlock_t *rwlock);

/**
 * @brief Unlock a read-write lock
 * @param rwlock Read-write lock pointer
 * @return 0 on success, non-zero on failure
 */
int sync_rwlock_unlock(sync_rwlock_t *rwlock);

#ifdef __cplusplus
}
#endif

#endif /* SYNC_COMMON_H */
