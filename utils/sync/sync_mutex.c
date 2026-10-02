// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file sync_mutex.c
 * @brief Mutex implementation.
 */

#include "sync_internal.h"

sync_result_t sync_mutex_create(sync_mutex_t *mutex, const sync_attr_t *attr)
{
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *m = (struct sync_mutex *)AIRY_CALLOC(1, sizeof(struct sync_mutex));
    if (m == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    m->type = SYNC_TYPE_MUTEX;
    if (attr != NULL && attr->name != NULL) {
        m->name = sync_internal_strdup(attr->name);
    }
    sync_stats_reset(&m->stats);

#ifdef _WIN32
    InitializeCriticalSection(&m->mutex);
#else
    pthread_mutexattr_t attr_mutex;
    pthread_mutexattr_init(&attr_mutex);
    if (attr != NULL && (attr->flags & SYNC_FLAG_RECURSIVE)) {
        pthread_mutexattr_settype(&attr_mutex, PTHREAD_MUTEX_RECURSIVE);
    }
    int result = pthread_mutex_init(&m->mutex, &attr_mutex);
    pthread_mutexattr_destroy(&attr_mutex);
    if (result != 0) {
        AIRY_FREE(m->name);
        AIRY_FREE(m);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    m->initialized = true;
    *mutex = m;
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_free(sync_mutex_t mutex)
{
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!mutex->initialized) {
        AIRY_FREE(mutex->name);
        AIRY_FREE(mutex);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    DeleteCriticalSection(&mutex->mutex);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif

    AIRY_FREE(mutex->name);
    AIRY_FREE(mutex);
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_lock_ex(sync_mutex_t mutex, const sync_timeout_t *timeout)
{
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }

    return sync_mtx_lock(&mutex->mutex, timeout, &mutex->stats);
}

sync_result_t sync_mutex_try_lock(sync_mutex_t mutex)
{
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    BOOL result = TryEnterCriticalSection(&mutex->mutex);
    if (!result) {
        return SYNC_ERROR_BUSY;
    }
#else
    int rc = pthread_mutex_trylock(&mutex->mutex);
    if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    sync_internal_update_stats_lock(&mutex->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_unlock_ex(sync_mutex_t mutex)
{
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    LeaveCriticalSection(&mutex->mutex);
#else
    int rc = pthread_mutex_unlock(&mutex->mutex);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    atomic_fetch_add(&mutex->stats.unlock_count, (size_t)1);
    return SYNC_SUCCESS;
}
