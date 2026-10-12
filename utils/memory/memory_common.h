/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef MEMORY_COMMON_H
#define MEMORY_COMMON_H

#include "airy_memory.h"

#include <error.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MEMORY_STRATEGY_DEFAULT = 0,
    MEMORY_STRATEGY_PERFORMANCE,
    MEMORY_STRATEGY_SAFETY,
    MEMORY_STRATEGY_LOW_LATENCY
} memory_strategy_t;

void *memory_safe_alloc(size_t size);

void *memory_safe_realloc(void *ptr, size_t size);

void memory_safe_free(void *ptr);

char *memory_safe_strdup(const char *src);

void memory_get_global_stats(memory_stats_t *stats);

void memory_reset_global_stats(void);

void memory_set_strategy(memory_strategy_t strategy);

memory_strategy_t memory_get_strategy(void);

#endif
