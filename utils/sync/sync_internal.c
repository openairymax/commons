// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

#include "sync_internal.h"

#include <errno.h>

sync_result_t sync_internal_posix_error_to_result(int error_code)
{
    switch (error_code) {
    case EINVAL:
        return SYNC_ERROR_INVALID;
    case ENOMEM:
        return SYNC_ERROR_MEMORY;
    case EPERM:
        return SYNC_ERROR_PERMISSION;
    case EBUSY:
        return SYNC_ERROR_BUSY;
    case ETIMEDOUT:
        return SYNC_ERROR_TIMEOUT;
    case EDEADLK:
        return SYNC_ERROR_DEADLOCK;
    default:
        return SYNC_ERROR_UNKNOWN;
    }
}

/* 获取互斥量的唯一入口：mutex 与 recursive_mutex 共用。平台原语与超时
 * 策略下沉平台母件（airy_mtx_lock / airy_mtx_timed）。 */
sync_result_t sync_mtx_lock(airy_mtx_t *mtx, const sync_timeout_t *timeout)
{
    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = airy_mtx_lock(mtx);
    } else {
        rc = airy_mtx_timed(mtx, (uint32_t)timeout->timeout_ms);
    }

    if (rc == ETIMEDOUT) {
        return SYNC_ERROR_TIMEOUT;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
    return SYNC_SUCCESS;
}

/* 获取读写锁的唯一入口：read（write=false）与 write（write=true）共用。
 * 平台原语与超时策略下沉平台母件（airy_rwlock_{wr,rd}lock /
 * airy_rwlock_timed）。 */
sync_result_t sync_rw_lock(airy_rwlock_t *rwlock, bool write, const sync_timeout_t *timeout)
{
    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = write ? airy_rwlock_wrlock(rwlock) : airy_rwlock_rdlock(rwlock);
    } else {
        rc = airy_rwlock_timed(rwlock, write, (uint32_t)timeout->timeout_ms);
    }

    if (rc == ETIMEDOUT) {
        return SYNC_ERROR_TIMEOUT;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
    return SYNC_SUCCESS;
}

sync_lock_hdr_t *sync_lock_new(size_t size)
{
    return (sync_lock_hdr_t *)AIRY_CALLOC(1, size);
}

sync_result_t sync_lock_free(sync_lock_hdr_t *hdr, airy_mtx_t *prim)
{
    if (hdr == NULL)
        return SYNC_ERROR_INVALID;

    /* initialized 未置位意味着平台原语尚未成功初始化（如 create 失败路径），
     * 此时只回收内存，不对原语调用 destroy。 */
    if (hdr->initialized)
        airy_mtx_destroy(prim);

    AIRY_FREE(hdr);
    return SYNC_SUCCESS;
}

sync_result_t sync_lock_unlock(sync_lock_hdr_t *hdr, airy_mtx_t *prim)
{
    if (hdr == NULL || !hdr->initialized)
        return SYNC_ERROR_INVALID;

    int rc = airy_mtx_unlock(prim);
    if (rc != 0)
        return sync_internal_posix_error_to_result(rc);

    return SYNC_SUCCESS;
}
