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
    (void)attr;

    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_rwlock *r = (struct sync_rwlock *)sync_lock_new(sizeof(struct sync_rwlock));
    if (r == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    int result = airy_rwlock_init(&r->rwlock);
    if (result != 0) {
        AIRY_FREE(r);
        return sync_internal_posix_error_to_result(result);
    }

    r->hdr.initialized = true;
    *rwlock = r;
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_free(sync_rwlock_t rwlock)
{
    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (rwlock->hdr.initialized) {
        airy_rwlock_destroy(&rwlock->rwlock);
    }

    AIRY_FREE(rwlock);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_read_lock_ex(sync_rwlock_t rwlock, const sync_timeout_t *timeout)
{
    if (rwlock == NULL || !rwlock->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    return sync_rw_lock(&rwlock->rwlock, false, timeout);
}

sync_result_t sync_rwlock_try_read_lock(sync_rwlock_t rwlock)
{
    if (rwlock == NULL || !rwlock->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    int rc = airy_rwlock_tryrd(&rwlock->rwlock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }

    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_write_lock_ex(sync_rwlock_t rwlock, const sync_timeout_t *timeout)
{
    if (rwlock == NULL || !rwlock->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    return sync_rw_lock(&rwlock->rwlock, true, timeout);
}

sync_result_t sync_rwlock_try_write_lock(sync_rwlock_t rwlock)
{
    if (rwlock == NULL || !rwlock->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    int rc = airy_rwlock_trywr(&rwlock->rwlock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }

    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_unlock_ex(sync_rwlock_t rwlock)
{
    if (rwlock == NULL || !rwlock->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    int rc = airy_rwlock_unlock(&rwlock->rwlock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }

    return SYNC_SUCCESS;
}
