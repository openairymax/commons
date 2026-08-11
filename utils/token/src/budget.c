// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file budget.c
 * @brief Token预算管理实现（跨平台）
 *
 * @details
 * 本模块实现Token预算管理功能：
 * - 支持输入/输出Token分离统计
 * - 提供预算重置和查询接口
 * - 线程安全的预算操作
 */

#include "error.h"
#include "airy_memory.h"
#include "platform.h"
#include "string_compat.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "atomic_compat.h"

#ifdef _WIN32
#else
#include <unistd.h>
#endif

/**
 * @brief 跨平台互斥锁类型
 */
#ifdef _WIN32
typedef airy_mtx_t budget_mutex_t;
#else
typedef airy_mtx_t budget_mutex_t;
#endif

/**
 * @brief 初始化互斥锁
 */
static int budget_mutex_init(budget_mutex_t *mutex)
{
#ifdef _WIN32
    airy_mtx_init(mutex);
    return 0;
#else
    return airy_mtx_init(mutex);
#endif
}

/**
 * @brief 销毁互斥锁
 */
static void budget_mutex_destroy(budget_mutex_t *mutex)
{
#ifdef _WIN32
    airy_mtx_destroy(mutex);
#else
    airy_mtx_destroy(mutex);
#endif
}

/**
 * @brief 加锁
 */
static void budget_mutex_lock(budget_mutex_t *mutex)
{
#ifdef _WIN32
    airy_mtx_lock(mutex);
#else
    airy_mtx_lock(mutex);
#endif
}

/**
 * @brief 解锁
 */
static void budget_mutex_unlock(budget_mutex_t *mutex)
{
#ifdef _WIN32
    airy_mtx_unlock(mutex);
#else
    airy_mtx_unlock(mutex);
#endif
}

/**
 * @brief Token预算内部结构
 */
struct airy_token_budget {
    size_t max_tokens;
    atomic_size_t used_tokens;
    atomic_size_t input_tokens;
    atomic_size_t output_tokens;
    atomic_uint request_count;
    atomic_uint denied_count;
    budget_mutex_t mutex;
    time_t reset_time;
    size_t window_seconds;
};

/**
 * @brief 检查预算是否充足
 */
static int check_budget_available(airy_token_budget_t *budget, size_t input, size_t output)
{
    if (!budget) {
        return AIRY_EINVAL;
    }

    size_t total = atomic_load(&budget->used_tokens);
    size_t requested = input + output;

    if (total + requested > budget->max_tokens) {
        atomic_fetch_add(&budget->denied_count, 1);
        return AIRY_EINVAL;
    }

    return 0;
}

airy_token_budget_t *airy_token_budget_create(size_t max_tokens)
{
    if (max_tokens == 0) {
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    airy_token_budget_t *budget = (airy_token_budget_t *)AIRY_MALLOC(sizeof(airy_token_budget_t));
    if (!budget) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    AIRY_MEMSET(budget, 0, sizeof(airy_token_budget_t));

    budget->max_tokens = max_tokens;
    atomic_init(&budget->used_tokens, 0);
    atomic_init(&budget->input_tokens, 0);
    atomic_init(&budget->output_tokens, 0);
    atomic_init(&budget->request_count, 0);
    atomic_init(&budget->denied_count, 0);

    if (budget_mutex_init(&budget->mutex) != 0) {
        AIRY_FREE(budget);
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    budget->reset_time = 0;
    budget->window_seconds = 0;

    return budget;
}

void airy_token_budget_destroy(airy_token_budget_t *budget)
{
    if (!budget) {
        return;
    }

    budget_mutex_destroy(&budget->mutex);
    AIRY_FREE(budget);
}

int airy_token_budget_add(airy_token_budget_t *budget, size_t input_tokens, size_t output_tokens)
{
    if (!budget) {
        return AIRY_EINVAL;
    }

    budget_mutex_lock(&budget->mutex);

    if (check_budget_available(budget, input_tokens, output_tokens) != 0) {
        budget_mutex_unlock(&budget->mutex);
        return AIRY_EINVAL;
    }

    atomic_fetch_add(&budget->used_tokens, input_tokens + output_tokens);
    atomic_fetch_add(&budget->input_tokens, input_tokens);
    atomic_fetch_add(&budget->output_tokens, output_tokens);
    atomic_fetch_add(&budget->request_count, 1);

    budget_mutex_unlock(&budget->mutex);

    return 0;
}

size_t airy_token_budget_remaining(airy_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    size_t used = atomic_load(&budget->used_tokens);

    if (used >= budget->max_tokens) {
        return 0;
    }

    return budget->max_tokens - used;
}

void airy_token_budget_reset(airy_token_budget_t *budget)
{
    if (!budget) {
        return;
    }

    budget_mutex_lock(&budget->mutex);

    atomic_store(&budget->used_tokens, 0);
    atomic_store(&budget->input_tokens, 0);
    atomic_store(&budget->output_tokens, 0);

    budget_mutex_unlock(&budget->mutex);
}

size_t airy_token_budget_used(airy_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->used_tokens);
}

size_t airy_token_budget_input(airy_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->input_tokens);
}

size_t airy_token_budget_output(airy_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->output_tokens);
}

uint32_t airy_token_budget_requests(airy_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->request_count);
}

uint32_t airy_token_budget_denied(airy_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->denied_count);
}

int airy_token_budget_set_window(airy_token_budget_t *budget, size_t window_seconds)
{
    if (!budget) {
        return AIRY_EINVAL;
    }

    budget_mutex_lock(&budget->mutex);

    budget->window_seconds = window_seconds;
    budget->reset_time = time(NULL) + window_seconds;

    budget_mutex_unlock(&budget->mutex);

    return 0;
}

int airy_token_budget_check_window(airy_token_budget_t *budget)
{
    if (!budget) {
        return AIRY_EINVAL;
    }

    budget_mutex_lock(&budget->mutex);

    time_t now = time(NULL);

    if (budget->reset_time > 0 && now >= budget->reset_time) {
        atomic_store(&budget->used_tokens, 0);
        atomic_store(&budget->input_tokens, 0);
        atomic_store(&budget->output_tokens, 0);

        budget->reset_time = now + budget->window_seconds;
    }

    budget_mutex_unlock(&budget->mutex);

    return 0;
}
