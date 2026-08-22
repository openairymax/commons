// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file sync_barrier.c
 * @brief Barrier implementation.
 */

#include "sync_internal.h"
#include "sync_platform.h"

#include <string.h>
#include <time.h>

sync_result_t sync_barrier_create(sync_barrier_t *barrier, unsigned int count,
                                  const sync_attr_t *attr)
{
    if (barrier == NULL || count == 0) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_barrier *b = (struct sync_barrier *)AIRY_CALLOC(1, sizeof(struct sync_barrier));
    if (b == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    b->type = SYNC_TYPE_BARRIER;
    if (attr != NULL && attr->name != NULL) {
        b->name = sync_internal_strdup(attr->name);
    }
    AIRY_MEMSET(&b->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeCriticalSection(&b->barrier.cs);
    InitializeConditionVariable(&b->barrier.cond);
    b->count = count;
    b->current = 0;
    b->generation = 0;
#else
    int result = platform_barrier_init(&b->barrier, count);
    if (result != 0) {
        AIRY_FREE(b->name);
        AIRY_FREE(b);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    b->initialized = true;
    *barrier = b;
    return SYNC_SUCCESS;
}

sync_result_t sync_barrier_free(sync_barrier_t barrier)
{
    if (barrier == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!barrier->initialized) {
        AIRY_FREE(barrier->name);
        AIRY_FREE(barrier);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    DeleteCriticalSection(&barrier->barrier.cs);
#else
    platform_barrier_destroy(&barrier->barrier);
#endif

    AIRY_FREE(barrier->name);
    AIRY_FREE(barrier);
    return SYNC_SUCCESS;
}

sync_result_t sync_barrier_wait_ex(sync_barrier_t barrier, const sync_timeout_t *timeout)
{
    if (barrier == NULL || !barrier->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    EnterCriticalSection(&barrier->barrier.cs);
    barrier->current++;

    if (barrier->current >= barrier->count) {
        barrier->current = 0;
        barrier->generation++;
        WakeAllConditionVariable(&barrier->barrier.cond);
        LeaveCriticalSection(&barrier->barrier.cs);
        return SYNC_SUCCESS;
    }

    unsigned int gen = barrier->generation;
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;

    while (barrier->generation == gen) {
        if (!SleepConditionVariableCS(&barrier->barrier.cond, &barrier->barrier.cs, wait_ms)) {
            if (GetLastError() == ERROR_TIMEOUT) {
                LeaveCriticalSection(&barrier->barrier.cs);
                return SYNC_ERROR_TIMEOUT;
            }
        }
    }
    LeaveCriticalSection(&barrier->barrier.cs);
    return SYNC_SUCCESS;
#else
    /* POSIX barrier 无超时语义，与 Windows 分支的 timeout 参数对齐 */
    (void)timeout;
    /* 统一走 platform 抽象层：serial thread（1）与普通到达（0）均视为成功，
     * 与 Windows 分支语义一致；负值为失败。 */
    int rc = platform_barrier_wait(&barrier->barrier);
    if (rc < 0) {
        return sync_internal_posix_error_to_result(rc);
    }
    return SYNC_SUCCESS;
#endif
}

sync_result_t sync_barrier_reset(sync_barrier_t barrier, unsigned int new_count)
{
    if (barrier == NULL || !barrier->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    EnterCriticalSection(&barrier->barrier.cs);
    if (new_count > 0) {
        barrier->count = new_count;
    }
    barrier->current = 0;
    barrier->generation++;
    LeaveCriticalSection(&barrier->barrier.cs);
    return SYNC_SUCCESS;
#else
    (void)new_count;
    return SYNC_ERROR_INVALID;
#endif
}
