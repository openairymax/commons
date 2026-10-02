// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file sync_recursive_mutex.c
 * @brief Recursive mutex implementation.
 */

#include "sync_internal.h"

sync_result_t sync_recursive_mutex_create(sync_recursive_mutex_t *mutex, const sync_attr_t *attr)
{
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_recursive_mutex *m =
        (struct sync_recursive_mutex *)AIRY_CALLOC(1, sizeof(struct sync_recursive_mutex));
    if (m == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    m->type = SYNC_TYPE_RECURSIVE_MUTEX;
    m->recursive_count = 0;
    if (attr != NULL && attr->name != NULL) {
        m->name = sync_internal_strdup(attr->name);
    }
    sync_stats_reset(&m->stats);

#ifdef _WIN32
    InitializeCriticalSection(&m->mutex);
#else
    pthread_mutexattr_t attr_mutex;
    pthread_mutexattr_init(&attr_mutex);
    pthread_mutexattr_settype(&attr_mutex, PTHREAD_MUTEX_RECURSIVE);
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

sync_result_t sync_recursive_mutex_free(sync_recursive_mutex_t mutex)
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

sync_result_t sync_recursive_mutex_lock_ex(sync_recursive_mutex_t mutex,
                                           const sync_timeout_t *timeout)
{
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }

    sync_result_t result = sync_mtx_lock(&mutex->mutex, timeout, &mutex->stats);
    if (result == SYNC_SUCCESS) {
        mutex->recursive_count++;
    }
    return result;
}

sync_result_t sync_recursive_mutex_unlock_ex(sync_recursive_mutex_t mutex)
{
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }

    if (mutex->recursive_count > 0) {
        mutex->recursive_count--;
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

sync_result_t sync_recursive_mutex_get_count(sync_recursive_mutex_t mutex, size_t *count)
{
    if (mutex == NULL || count == NULL) {
        return SYNC_ERROR_INVALID;
    }
    *count = mutex->recursive_count;
    return SYNC_SUCCESS;
}
