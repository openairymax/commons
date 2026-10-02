// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

#include "sync_internal.h"

#include "airy_memory.h"

#include <errno.h>

char *sync_internal_strdup(const char *str)
{
    if (!str)
        return NULL;
    size_t len = strlen(str) + 1;
    char *dup = (char *)AIRY_MALLOC(len);
    if (dup)
        __builtin_memcpy(dup, str, len);
    return dup;
}

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

void sync_stats_reset(sync_stats_ctr_t *stats)
{
    if (!stats)
        return;

    atomic_init(&stats->lock_count, (size_t)0);
    atomic_init(&stats->unlock_count, (size_t)0);
    atomic_init(&stats->wait_count, (size_t)0);
    atomic_init(&stats->timeout_count, (size_t)0);
    atomic_init(&stats->deadlock_count, (size_t)0);
    atomic_init(&stats->total_wait_time_ms, (uint64_t)0);
    atomic_init(&stats->max_wait_time_ms, (uint64_t)0);
}

void sync_stats_snapshot(const sync_stats_ctr_t *stats, sync_stats_t *out)
{
    if (!stats || !out)
        return;

    /* 直接解引用即原子读取。不使用 atomic_load()：非 stdatomic 路径下其
     * 8 字节分支会经 (int) 收窄，截断 64 位计数。 */
    out->lock_count = (size_t)stats->lock_count;
    out->unlock_count = (size_t)stats->unlock_count;
    out->wait_count = (size_t)stats->wait_count;
    out->timeout_count = (size_t)stats->timeout_count;
    out->deadlock_count = (size_t)stats->deadlock_count;
    out->total_wait_time_ms = (uint64_t)stats->total_wait_time_ms;
    out->max_wait_time_ms = (uint64_t)stats->max_wait_time_ms;
}

static void stats_max_ms(atomic_uint64_t *dst, uint64_t value)
{
    uint64_t cur = (uint64_t)*dst;
    while (value > cur) {
        if (atomic_compare_exchange_weak(dst, &cur, value))
            break;
    }
}

void sync_internal_update_stats_lock(sync_stats_ctr_t *stats, int64_t elapsed_ms)
{
    if (!stats)
        return;

    atomic_fetch_add(&stats->lock_count, (size_t)1);
    atomic_fetch_add(&stats->total_wait_time_ms, (uint64_t)elapsed_ms);
    stats_max_ms(&stats->max_wait_time_ms, (uint64_t)elapsed_ms);
}

void sync_internal_update_stats_timeout(sync_stats_ctr_t *stats)
{
    if (!stats)
        return;

    atomic_fetch_add(&stats->timeout_count, (size_t)1);
}

void sync_internal_update_stats_wait(sync_stats_ctr_t *stats, int64_t elapsed_ms)
{
    if (!stats)
        return;

    atomic_fetch_add(&stats->wait_count, (size_t)1);
    atomic_fetch_add(&stats->total_wait_time_ms, (uint64_t)elapsed_ms);
    stats_max_ms(&stats->max_wait_time_ms, (uint64_t)elapsed_ms);
}

static int64_t sync_start_timer(const sync_timeout_t *timeout)
{
    if (timeout != NULL && timeout->timeout_ms > 0) {
        return (int64_t)clock();
    }
    return 0;
}

/* 获取结果收口为同步语义的唯一实现：超时计数、POSIX 错误映射、成功路径的
 * 等待时长统计。mutex 与 rwlock 两条路径共用，避免分支与统计逻辑重复。 */
static sync_result_t sync_finish_lock(int rc, int64_t start_time, sync_stats_ctr_t *stats)
{
    if (rc == ETIMEDOUT) {
        sync_internal_update_stats_timeout(stats);
        return SYNC_ERROR_TIMEOUT;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }

    int64_t elapsed = 0;
    if (start_time > 0) {
        elapsed = ((int64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
    }
    sync_internal_update_stats_lock(stats, elapsed);
    return SYNC_SUCCESS;
}

/* 获取互斥量的唯一入口：mutex 与 recursive_mutex 共用。平台原语与超时策略
 * 下沉 sync_platform（platform_mutex_lock / platform_mtx_timed），本层只保留
 * 计时与统计。 */
sync_result_t sync_mtx_lock(platform_mutex_t *mtx, const sync_timeout_t *timeout,
                            sync_stats_ctr_t *stats)
{
    int64_t start_time = sync_start_timer(timeout);

    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = platform_mutex_lock(mtx);
    } else {
        rc = platform_mtx_timed(mtx, (uint32_t)timeout->timeout_ms);
    }

    return sync_finish_lock(rc, start_time, stats);
}

/* 获取读写锁的唯一入口：read（write=false）与 write（write=true）共用。
 * 平台原语与超时策略下沉 sync_platform（platform_rwlock_{rd,wr}lock /
 * platform_rw_timed），本层只保留计时与统计。 */
sync_result_t sync_rw_lock(platform_rwlock_t *rwlock, bool write, const sync_timeout_t *timeout,
                           sync_stats_ctr_t *stats)
{
    int64_t start_time = sync_start_timer(timeout);

    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = write ? platform_rwlock_wrlock(rwlock) : platform_rwlock_rdlock(rwlock);
    } else {
        rc = platform_rw_timed(rwlock, write, (uint32_t)timeout->timeout_ms);
    }

    return sync_finish_lock(rc, start_time, stats);
}
