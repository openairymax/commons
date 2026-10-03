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

    struct sync_mutex *m =
        (struct sync_mutex *)sync_lock_new(sizeof(struct sync_mutex), SYNC_TYPE_MUTEX, attr);
    if (m == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    bool recursive = (attr != NULL) && (attr->flags & SYNC_FLAG_RECURSIVE) != 0;
    int result = platform_mtx_init(&m->mutex, recursive);
    if (result != 0) {
        sync_lock_free((sync_lock_hdr_t *)m, &m->mutex);
        return sync_internal_posix_error_to_result(result);
    }

    m->hdr.initialized = true;
    *mutex = m;
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_free(sync_mutex_t mutex)
{
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    return sync_lock_free((sync_lock_hdr_t *)mutex, &mutex->mutex);
}

sync_result_t sync_mutex_lock_ex(sync_mutex_t mutex, const sync_timeout_t *timeout)
{
    if (mutex == NULL || !mutex->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    return sync_mtx_lock(&mutex->mutex, timeout, &mutex->hdr.stats);
}

sync_result_t sync_mutex_try_lock(sync_mutex_t mutex)
{
    if (mutex == NULL || !mutex->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    int rc = platform_mutex_trylock(&mutex->mutex);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }

    sync_internal_update_stats_lock(&mutex->hdr.stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_unlock_ex(sync_mutex_t mutex)
{
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    return sync_lock_unlock((sync_lock_hdr_t *)mutex, &mutex->mutex);
}
