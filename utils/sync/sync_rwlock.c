// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file sync_rwlock.c
 * @brief Read-write lock implementation.
 */

#include "sync_internal.h"

sync_result_t sync_rwlock_create(sync_rwlock_t *rwlock, const sync_attr_t *attr)
{
    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_rwlock *r = (struct sync_rwlock *)AIRY_CALLOC(1, sizeof(struct sync_rwlock));
    if (r == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    r->type = SYNC_TYPE_RWLOCK;
#ifdef _WIN32
    atomic_init(&r->writer_owner, (unsigned)0);
#endif
    if (attr != NULL && attr->name != NULL) {
        r->name = sync_internal_strdup(attr->name);
    }
    sync_stats_reset(&r->stats);

#ifdef _WIN32
    InitializeSRWLock(&r->rwlock);
#else
    pthread_rwlockattr_t attr_rwlock;
    pthread_rwlockattr_init(&attr_rwlock);
    if (attr != NULL && (attr->flags & SYNC_FLAG_SHARED)) {
        pthread_rwlockattr_setpshared(&attr_rwlock, PTHREAD_PROCESS_SHARED);
    }
    int result = pthread_rwlock_init(&r->rwlock, &attr_rwlock);
    pthread_rwlockattr_destroy(&attr_rwlock);
    if (result != 0) {
        AIRY_FREE(r->name);
        AIRY_FREE(r);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    r->initialized = true;
    *rwlock = r;
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_free(sync_rwlock_t rwlock)
{
    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!rwlock->initialized) {
        AIRY_FREE(rwlock->name);
        AIRY_FREE(rwlock);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    (void)rwlock->rwlock;
#else
    pthread_rwlock_destroy(&rwlock->rwlock);
#endif

    AIRY_FREE(rwlock->name);
    AIRY_FREE(rwlock);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_read_lock_ex(sync_rwlock_t rwlock, const sync_timeout_t *timeout)
{
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

    return sync_rw_lock(&rwlock->rwlock, false, timeout, &rwlock->stats);
}

sync_result_t sync_rwlock_try_read_lock(sync_rwlock_t rwlock)
{
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    BOOL result = TryAcquireSRWLockShared(&rwlock->rwlock);
    if (!result) {
        return SYNC_ERROR_BUSY;
    }
#else
    int rc = pthread_rwlock_tryrdlock(&rwlock->rwlock);
    if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    sync_internal_update_stats_lock(&rwlock->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_write_lock_ex(sync_rwlock_t rwlock, const sync_timeout_t *timeout)
{
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

    sync_result_t result = sync_rw_lock(&rwlock->rwlock, true, timeout, &rwlock->stats);
#ifdef _WIN32
    if (result == SYNC_SUCCESS) {
        atomic_store(&rwlock->writer_owner, (unsigned)GetCurrentThreadId());
    }
#endif
    return result;
}

sync_result_t sync_rwlock_try_write_lock(sync_rwlock_t rwlock)
{
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    BOOL result = TryAcquireSRWLockExclusive(&rwlock->rwlock);
    if (!result) {
        return SYNC_ERROR_BUSY;
    }
#else
    int rc = pthread_rwlock_trywrlock(&rwlock->rwlock);
    if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

#ifdef _WIN32
    atomic_store(&rwlock->writer_owner, (unsigned)GetCurrentThreadId());
#endif
    sync_internal_update_stats_lock(&rwlock->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_unlock_ex(sync_rwlock_t rwlock)
{
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    /* SRWLock 释放方向必须与获取方向一致：仅当本线程是当前写者时以独占
     * 模式释放，否则按共享模式释放。 */
    unsigned tid = (unsigned)GetCurrentThreadId();
    if (rwlock->writer_owner == tid) {
        ReleaseSRWLockExclusive(&rwlock->rwlock);
        atomic_store(&rwlock->writer_owner, (unsigned)0);
    } else {
        ReleaseSRWLockShared(&rwlock->rwlock);
    }
#else
    int rc = pthread_rwlock_unlock(&rwlock->rwlock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    atomic_fetch_add(&rwlock->stats.unlock_count, (size_t)1);
    return SYNC_SUCCESS;
}
