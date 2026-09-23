// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

#include "memory_common.h"

#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/*
 * Windows 下 memory_safe 层的指针统一来自 memory_alloc（内部
 * _aligned_malloc）；_msize() 仅对 malloc 系指针合法，对
 * _aligned_malloc 指针取块大小是 UB，会破坏堆元数据并触发
 * 0xc0000374 堆损坏检测。Windows 侧放弃按块字节记账（与
 * AIRY_FREE 层"记次不记字节"口径一致），不在 free/realloc
 * 前对指针调 _msize。
 */
#define memory_safe_block_size(ptr) ((size_t)0)
#elif defined(__APPLE__)
/* macOS 无 <malloc.h>/malloc_usable_size()，对应 API 是 malloc_size()
 * （<malloc/malloc.h>）。 */
#include <malloc/malloc.h>
#define memory_safe_block_size(ptr) malloc_size(ptr)
#else
#include <malloc.h>
#define memory_safe_block_size(ptr) malloc_usable_size(ptr)
#endif

static memory_stats_t g_memory_stats = {.total_allocated = 0,
                                        .total_freed = 0,
                                        .current_allocated = 0,
                                        .peak_allocated = 0,
                                        .allocation_count = 0,
                                        .free_count = 0,
                                        .leak_count = 0};

static memory_strategy_t g_memory_strategy = MEMORY_STRATEGY_DEFAULT;

/* Note: the old memory_create_default_pool_config / memory_pool_init /
 * memory_pool_alloc / memory_pool_free / memory_pool_cleanup /
 * memory_pool_get_stats have been removed; they conflict with the new
 * implementations in memory_pool.c. See memory_pool.h for the new pool
 * API.
 */

void *memory_safe_alloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = AIRY_MALLOC(size);
    if (ptr) {
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_allocated += size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.current_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.current_allocated;
        }
    }

    return ptr;
}

void *memory_safe_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        memory_safe_free(ptr);
        return NULL;
    }

    size_t old_size = ptr ? memory_safe_block_size(ptr) : 0;
    void *new_ptr = AIRY_REALLOC(ptr, size);
    if (new_ptr) {
        if (old_size > 0) {
            g_memory_stats.total_freed += old_size;
            g_memory_stats.current_allocated -= old_size;
        }
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_allocated += size;
        g_memory_stats.allocation_count++;
        g_memory_stats.free_count++;
        if (g_memory_stats.current_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.current_allocated;
        }
    }

    return new_ptr;
}

void memory_safe_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    size_t size = memory_safe_block_size(ptr);
    AIRY_FREE(ptr);

    g_memory_stats.total_freed += size;
    g_memory_stats.current_allocated -= size;
    g_memory_stats.free_count++;
}

char *memory_safe_strdup(const char *src)
{
    if (!src) {
        return NULL;
    }

    size_t len = strlen(src) + 1;
    char *dest = memory_safe_alloc(len);
    if (dest) {
        __builtin_memcpy(dest, src, len);
    }

    return dest;
}

void memory_get_global_stats(memory_stats_t *stats)
{
    if (!stats) {
        return;
    }

    *stats = g_memory_stats;
}

void memory_reset_global_stats(void)
{
    AIRY_MEMSET(&g_memory_stats, 0, sizeof(g_memory_stats));
}

void memory_set_strategy(memory_strategy_t strategy)
{
    g_memory_strategy = strategy;
}

memory_strategy_t memory_get_strategy(void)
{
    return g_memory_strategy;
}
