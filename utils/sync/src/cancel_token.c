// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file cancel_token.c
 * @brief 取消令牌实现（改进1：Codex parallel.rs cancel_token 模式）
 *
 * 原子取消标志 + 条件变量唤醒 + 有界回调唤醒链：
 *   - cancel：置位原子标志 → 广播条件变量 → 触发全部唤醒回调
 *   - is_canceled：无锁原子读（取消判定热路径零开销）
 *   - wait：条件变量阻塞等待（非忙轮询），取消/超时返回
 *   - reset：复位标志（取消后恢复重跑复用，回调链保留）
 *
 * 回调采用"锁内快照 + 锁外执行"，回调可安全调用本模块 API（无自死锁）。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cancel_token.h"

#include "airy_memory.h"
#include "error.h"

#include <string.h>

int airy_cancel_token_init(airy_cancel_token_t *token)
{
    if (!token)
        return AIRY_EINVAL;

    __builtin_memset(token, 0, sizeof(*token));
    if (airy_mtx_init(&token->lock) != 0)
        return AIRY_ERR_SYS_MUTEX;
    if (airy_cond_init(&token->cond) != 0) {
        airy_mtx_destroy(&token->lock);
        return AIRY_ERR_SYS_CONDITION;
    }
    airy_atomic_store(&token->cancelled, 0);
    token->init_done = 1;
    token->cb_count = 0;
    return 0;
}

void airy_cancel_token_destroy(airy_cancel_token_t *token)
{
    if (!token || !token->init_done)
        return;
    airy_cond_destroy(&token->cond);
    airy_mtx_destroy(&token->lock);
    token->init_done = 0;
    token->cb_count = 0;
}

void airy_cancel_token_cancel(airy_cancel_token_t *token)
{
    if (!token || !token->init_done)
        return;

    /* 快照回调链（锁内），标志置位 + 广播（锁内），回调锁外触发 */
    airy_cancel_cb_t snap[AIRY_CANCEL_TOKEN_MAX_CBS];
    void *snap_ctx[AIRY_CANCEL_TOKEN_MAX_CBS];
    size_t snap_n = 0;

    airy_mtx_lock(&token->lock);
    int was_canceled = (airy_atomic_load(&token->cancelled) != 0);
    airy_atomic_store(&token->cancelled, 1);
    airy_cond_broadcast(&token->cond);
    if (!was_canceled) {
        snap_n = token->cb_count;
        for (size_t i = 0; i < snap_n && i < AIRY_CANCEL_TOKEN_MAX_CBS; i++) {
            snap[i] = token->cbs[i];
            snap_ctx[i] = token->cb_ctxs[i];
        }
    }
    airy_mtx_unlock(&token->lock);

    for (size_t i = 0; i < snap_n; i++) {
        if (snap[i])
            snap[i](snap_ctx[i]);
    }
}

bool airy_cancel_token_is_canceled(const airy_cancel_token_t *token)
{
    if (!token || !token->init_done)
        return false;
    return airy_atomic_load((airy_atomic_int_t *)&token->cancelled) != 0;
}

void airy_cancel_token_reset(airy_cancel_token_t *token)
{
    if (!token || !token->init_done)
        return;
    airy_mtx_lock(&token->lock);
    airy_atomic_store(&token->cancelled, 0);
    airy_mtx_unlock(&token->lock);
}

int airy_cancel_token_register(airy_cancel_token_t *token, airy_cancel_cb_t cb, void *ctx)
{
    if (!token || !cb || !token->init_done)
        return AIRY_EINVAL;

    airy_mtx_lock(&token->lock);
    if (token->cb_count >= AIRY_CANCEL_TOKEN_MAX_CBS) {
        airy_mtx_unlock(&token->lock);
        return AIRY_ERR_OVERFLOW;
    }
    token->cbs[token->cb_count] = cb;
    token->cb_ctxs[token->cb_count] = ctx;
    token->cb_count++;
    airy_mtx_unlock(&token->lock);
    return 0;
}

int airy_cancel_token_wait(airy_cancel_token_t *token, uint32_t timeout_ms)
{
    if (!token || !token->init_done)
        return AIRY_EINVAL;

    airy_mtx_lock(&token->lock);
    if (airy_atomic_load(&token->cancelled) != 0) {
        airy_mtx_unlock(&token->lock);
        return 0; /* 已取消 */
    }
    if (timeout_ms == 0) {
        while (airy_atomic_load(&token->cancelled) == 0)
            airy_cond_wait(&token->cond, &token->lock);
        airy_mtx_unlock(&token->lock);
        return 0;
    }
    /* 带超时：deadline 方式计算剩余时间，容忍虚假唤醒 */
    uint64_t deadline = airy_time_ms() + (uint64_t)timeout_ms;
    int rc = 1; /* 默认超时 */
    for (;;) {
        uint64_t now = airy_time_ms();
        if (airy_atomic_load(&token->cancelled) != 0) {
            rc = 0;
            break;
        }
        if (now >= deadline)
            break;
        uint32_t remaining = (uint32_t)(deadline - now);
        if (remaining == 0)
            remaining = 1;
        airy_cond_timedwait(&token->cond, &token->lock, remaining);
    }
    airy_mtx_unlock(&token->lock);
    return rc;
}
