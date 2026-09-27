// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

#include "sync_internal.h"

#include "airy_memory.h"

#include <errno.h>
#include <string.h>

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

void sync_internal_stats_reset(sync_stats_ctr_t *stats)
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

void sync_internal_stats_snapshot(const sync_stats_ctr_t *stats, sync_stats_t *out)
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
