/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef MEMORY_COMMON_H
#define MEMORY_COMMON_H

#include "airy_memory.h"

#include <error.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef AIRY_MEMORY_STATS_T_DEFINED
#define AIRY_MEMORY_STATS_T_DEFINED
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t leak_count;
} memory_stats_t;
#endif

typedef enum {
    MEMORY_STRATEGY_DEFAULT = 0,
    MEMORY_STRATEGY_PERFORMANCE,
    MEMORY_STRATEGY_SAFETY,
    MEMORY_STRATEGY_LOW_LATENCY
} memory_strategy_t;

/* Note: the old memory_pool_t / memory_pool_config_t / memory_pool_init /
 * memory_pool_alloc etc. have been removed; they conflict with the new
 * implementations in memory_pool.h. See memory_pool.h for the new pool
 * API (memory_pool_create / memory_pool_alloc etc.). The retained
 * memory_pool_t is defined in memory_pool.h as an opaque pointer
 * (struct memory_pool *).
 */

void *memory_safe_alloc(size_t size);

void *memory_safe_realloc(void *ptr, size_t size);

void memory_safe_free(void *ptr);

char *memory_safe_strdup(const char *src);

void memory_get_global_stats(memory_stats_t *stats);

void memory_reset_global_stats(void);

void memory_set_strategy(memory_strategy_t strategy);

memory_strategy_t memory_get_strategy(void);

#endif
