/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file thread_pool.h
 * @brief 通用工作线程池（commons 权威版本）
 *
 * 提供固定大小的工作线程池，支持任务提交、优雅关闭。
 * 所有 daemon 服务（gateway_d/llm_d/tool_d 等）及 atoms 编排器共享此基础设施。
 *
 * P0.17 阶段 5：从 daemons/common/include/thread_pool.h 迁移至 commons，
 * 消除 atoms→daemons 编译期反向依赖（IRON-6）。daemons 版保留为重导出兼容头。
 */

#ifndef AIRY_RT_THREAD_POOL_H
#define AIRY_RT_THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct thread_pool_s thread_pool_t;

typedef void (*thread_task_fn_t)(void *arg);


typedef struct {
    uint32_t min_threads;
    uint32_t max_threads;
    uint32_t queue_size;
    uint32_t idle_timeout_ms;
} thread_pool_config_t;


thread_pool_t *thread_pool_create(const thread_pool_config_t *config);

void thread_pool_destroy(thread_pool_t *pool);

int thread_pool_submit(thread_pool_t *pool, thread_task_fn_t task, void *arg);


uint32_t thread_pool_active_count(thread_pool_t *pool);

uint32_t thread_pool_pending_count(thread_pool_t *pool);

bool thread_pool_is_running(thread_pool_t *pool);


static inline void thread_pool_get_default_config(thread_pool_config_t *cfg)
{
    cfg->min_threads = 2;
    cfg->max_threads = 8;
    cfg->queue_size = 256;
    cfg->idle_timeout_ms = 30000;
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_THREAD_POOL_H */
