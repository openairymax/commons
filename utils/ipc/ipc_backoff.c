// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file ipc_backoff.c
 * @brief IPC 重连退避与失败窗口聚合原语实现（0.1.19 §3.6 D7）。
 *
 * 纯算术、无锁、无分配：状态由调用方内嵌持有（单 target 一个实例，
 * 多 target 由调用方开数组），避免哈希表与全局状态，体积最省。
 *
 * @see ipc_backoff.h
 */

#include "ipc_backoff.h"

#define IPC_BKOFF_DEF_BASE_MS 200u
#define IPC_BKOFF_DEF_CAP_MS 5000u
#define IPC_AGG_DEF_WIN_SEC 60u

void ipc_bkoff_init(ipc_bkoff_t *b, uint32_t base_ms, uint32_t cap_ms)
{
    if (!b)
        return;
    if (base_ms == 0)
        base_ms = IPC_BKOFF_DEF_BASE_MS;
    if (cap_ms == 0)
        cap_ms = IPC_BKOFF_DEF_CAP_MS;
    if (cap_ms < base_ms)
        cap_ms = base_ms;
    b->base_ms = base_ms;
    b->cap_ms = cap_ms;
    b->cur_ms = base_ms;
}

uint32_t ipc_bkoff_next(ipc_bkoff_t *b)
{
    if (!b)
        return 0;
    uint32_t delay = b->cur_ms;
    if (b->cur_ms < b->cap_ms) {
        uint64_t next = (uint64_t)b->cur_ms * 2u;
        b->cur_ms = (next > b->cap_ms) ? b->cap_ms : (uint32_t)next;
    }
    return delay;
}

void ipc_bkoff_reset(ipc_bkoff_t *b)
{
    if (b)
        b->cur_ms = b->base_ms;
}

void ipc_agg_init(ipc_agg_t *a, uint32_t win_sec)
{
    if (!a)
        return;
    a->win_sec = (win_sec == 0) ? IPC_AGG_DEF_WIN_SEC : win_sec;
    a->win_start = 0;
    a->count = 0;
}

bool ipc_agg_feed(ipc_agg_t *a, uint64_t now_sec, uint64_t *prev_count)
{
    if (!a) {
        if (prev_count)
            *prev_count = 0;
        return true;
    }
    if (a->count > 0 && now_sec < a->win_start + a->win_sec) {
        a->count++;
        if (prev_count)
            *prev_count = 0;
        return false;
    }
    uint64_t prev = (a->count > 1) ? a->count : 0;
    a->win_start = now_sec;
    a->count = 1;
    if (prev_count)
        *prev_count = prev;
    return true;
}

void ipc_agg_reset(ipc_agg_t *a)
{
    if (!a)
        return;
    a->win_start = 0;
    a->count = 0;
}
