/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file thread_pool.h
 * @brief Generic worker thread pool (commons authoritative version).
 *
 * Provides a fixed-size worker thread pool with task submission and
 * graceful shutdown. All daemon services (gateway_d/llm_d/tool_d etc.)
 * and the atoms orchestrators share this infrastructure.
 *
 * P0.17 phase 5: migrated from daemons/common/include/thread_pool.h to
 * commons, removing the compile-time reverse dependency atoms->daemons
 * (IRON-6). The daemons version is kept as a re-export compat header.
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
