// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file sync_condition.c
 * @brief Condition variable implementation.
 */

#include "sync_internal.h"

#include "error_codes.h"

sync_result_t sync_condition_create(sync_condition_t *condition, const sync_attr_t *attr)
{
    (void)attr;

    if (condition == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_condition *c =
        (struct sync_condition *)sync_lock_new(sizeof(struct sync_condition));
    if (c == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    int result = airy_cond_init(&c->cond);
    if (result != 0) {
        AIRY_FREE(c);
        return sync_internal_posix_error_to_result(result);
    }

    c->hdr.initialized = true;
    *condition = c;
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_free(sync_condition_t condition)
{
    if (condition == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (condition->hdr.initialized) {
        airy_cond_destroy(&condition->cond);
    }

    AIRY_FREE(condition);
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_wait_ex(sync_condition_t condition, sync_mutex_t mutex,
                                     const sync_timeout_t *timeout)
{
    if (condition == NULL || mutex == NULL || !condition->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = airy_cond_wait(&condition->cond, &mutex->mutex);
    } else {
        rc = airy_cond_timedwait(&condition->cond, &mutex->mutex,
                                 (uint32_t)timeout->timeout_ms);
    }
    if (rc == AIRY_ERR_TIMEOUT) {
        return SYNC_ERROR_TIMEOUT;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }

    return SYNC_SUCCESS;
}

sync_result_t sync_condition_signal_ex(sync_condition_t condition)
{
    if (condition == NULL || !condition->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    int rc = airy_cond_signal(&condition->cond);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_broadcast_ex(sync_condition_t condition)
{
    if (condition == NULL || !condition->hdr.initialized) {
        return SYNC_ERROR_INVALID;
    }

    int rc = airy_cond_broadcast(&condition->cond);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
    return SYNC_SUCCESS;
}
