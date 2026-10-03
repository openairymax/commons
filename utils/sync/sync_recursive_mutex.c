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

    struct sync_recursive_mutex *m = (struct sync_recursive_mutex *)sync_lock_new(
        sizeof(struct sync_recursive_mutex), SYNC_TYPE_RECURSIVE_MUTEX, attr);
    if (m == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    int result = platform_mtx_init(&m->mutex, true);
    if (result != 0) {
        sync_lock_free((sync_lock_hdr_t *)m, &m->mutex);
        return sync_internal_posix_error_to_result(result);
    }

    m->hdr.initialized = true;
    *mutex = m;
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_free(sync_recursive_mutex_t mutex)
{
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    return sync_lock_free((sync_lock_hdr_t *)mutex, &mutex->mutex);
}

sync_result_t sync_recursive_mutex_lock_ex(sync_recursive_mutex_t mutex,
                                           const sync_timeout_t *timeout)
{
    if (mutex == NULL || !mutex->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    sync_result_t result = sync_mtx_lock(&mutex->mutex, timeout, &mutex->hdr.stats);
    if (result == SYNC_SUCCESS) {
        mutex->recursive_count++;
    }
    return result;
}

sync_result_t sync_recursive_mutex_unlock_ex(sync_recursive_mutex_t mutex)
{
    if (mutex == NULL || !mutex->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    if (mutex->recursive_count > 0) {
        mutex->recursive_count--;
    }

    return sync_lock_unlock((sync_lock_hdr_t *)mutex, &mutex->mutex);
}

sync_result_t sync_recursive_mutex_get_count(sync_recursive_mutex_t mutex, size_t *count)
{
    if (mutex == NULL || count == NULL) {
        return SYNC_ERROR_INVALID;
    }
    *count = mutex->recursive_count;
    return SYNC_SUCCESS;
}
