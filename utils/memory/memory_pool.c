// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_pool.c
 * @brief Unified memory management module - memory pool lifecycle and
 *        block region management.
 *
 * Implements the memory pool instance lifecycle (create/destroy), the
 * lock/alignment primitives and block region allocation/release shared by
 * the whole pool family. Allocation/release and capacity management live
 * in memory_pool_alloc.c; statistics/queries/validation live in
 * memory_pool_stats.c.
 */

#include "memory_pool.h"

#include "memory_pool_internal.h"

#include "airy_memory.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "string_compat.h"
#include "logging.h"

#include <stdio.h>
#include <string.h>
#include "platform.h"
#include <stdint.h>

static size_t memory_pool_align_size(size_t size, size_t alignment)
{
    if (alignment == 0) {
        return size;
    }

    return ((size + alignment - 1) / alignment) * alignment;
}

bool memory_pool_lock_init(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return true;
    }

    return airy_mtx_init(&pool->lock) == 0;
}

void memory_pool_lock_destroy(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return;
    }

    airy_mtx_destroy(&pool->lock);
}

void memory_pool_lock(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return;
    }

    airy_mtx_lock(&pool->lock);
}

void memory_pool_unlock(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return;
    }

    airy_mtx_unlock(&pool->lock);
}

bool memory_pool_allocate_blocks(memory_pool_t *pool, size_t block_count)
{
    if (pool == NULL || block_count == 0) {
        return false;
    }

    size_t aligned_block_size =
        memory_pool_align_size(sizeof(memory_pool_block_t) + pool->options.block_size,
                               sizeof(void *));

    if (block_count > 0 && aligned_block_size > SIZE_MAX / block_count) {
        return false;
    }
    size_t total_size = block_count * aligned_block_size;

    if (pool->options.max_blocks > 0 &&
        pool->stats.total_blocks + block_count > pool->options.max_blocks) {
        return false;
    }

    /* Allocate a separate memory region for new blocks; never realloc to
     * merge old regions because realloc may move memory, invalidating all
     * previously handed-out block pointers. */
    void *new_memory = memory_aligned_alloc(sizeof(void *), total_size, "memory_pool");
    if (new_memory == NULL) {
        return false;
    }

    if (pool->memory_area != NULL) {
        memory_region_node_t *node =
            (memory_region_node_t *)memory_calloc(sizeof(memory_region_node_t), "old_region_node");
        if (node) {
            node->region = pool->memory_area;
            node->region_size = pool->memory_area_size;
            node->next = pool->old_regions;
            pool->old_regions = node;
        }
        AIRY_LOG_DEBUG(
            "memory_pool: expanding with new region (old=%p, new=%p, old_size=%zu, new_size=%zu)",
            pool->memory_area, new_memory, pool->memory_area_size, total_size);
    }

    pool->memory_area = new_memory;
    pool->memory_area_size = total_size;

    size_t new_capacity = pool->blocks_capacity + block_count;
    if (new_capacity > SIZE_MAX / sizeof(memory_pool_block_t *)) {
        return false;
    }
    memory_pool_block_t **new_blocks =
        memory_realloc(pool->blocks, new_capacity * sizeof(memory_pool_block_t *),
                       "memory_pool_blocks");

    if (new_blocks == NULL) {
        memory_free(pool->memory_area);
        pool->memory_area = NULL;
        return false;
    }

    pool->blocks = new_blocks;
    pool->blocks_capacity = new_capacity;

    uint8_t *memory_ptr = (uint8_t *)pool->memory_area;
    for (size_t i = 0; i < block_count; i++) {
        memory_pool_block_t *block = (memory_pool_block_t *)memory_ptr;

        block->next = NULL;
        block->allocated = false;
        block->index = pool->stats.total_blocks + i;
        block->pool = pool;

        block->next = pool->free_list;
        pool->free_list = block;

        pool->blocks[pool->stats.total_blocks + i] = block;

        memory_ptr += aligned_block_size;
    }

    pool->stats.total_blocks += block_count;
    pool->stats.free_blocks += block_count;
    pool->stats.total_memory += total_size;

    return true;
}

void memory_pool_free_blocks(memory_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    if (pool->memory_area != NULL) {
        memory_free(pool->memory_area);
        pool->memory_area = NULL;
        pool->memory_area_size = 0;
    }

    memory_region_node_t *region = pool->old_regions;
    while (region) {
        memory_region_node_t *next = region->next;
        if (region->region) {
            memory_free(region->region);
        }
        memory_free(region);
        region = next;
    }
    pool->old_regions = NULL;

    if (pool->blocks != NULL) {
        memory_free(pool->blocks);
        pool->blocks = NULL;
        pool->blocks_capacity = 0;
    }

    pool->free_list = NULL;
    pool->stats.total_blocks = 0;
    pool->stats.allocated_blocks = 0;
    pool->stats.free_blocks = 0;
    pool->stats.total_memory = 0;
    pool->stats.used_memory = 0;
}

memory_pool_t *memory_pool_create(const memory_pool_options_t *options)
{
    if (options == NULL || options->block_size == 0)
        return NULL;

    AIRY_LOG_INFO("memory_pool: memory_pool_create (block_size=%zu, initial_blocks=%zu, max_blocks=%zu, "
             "thread_safe=%s, name=%s)",
             options->block_size, options->initial_blocks, options->max_blocks,
             options->thread_safe ? "true" : "false", options->name ? options->name : "(unnamed)");

    memory_pool_t *pool = memory_calloc(sizeof(memory_pool_t), "memory_pool_instance");
    if (pool == NULL) {
        return NULL;
    }

    __builtin_memcpy(&pool->options, options, sizeof(memory_pool_options_t));

    if (pool->options.initial_blocks == 0) {
        pool->options.initial_blocks = 16;
    }

    if (pool->options.expansion_size == 0) {
        pool->options.expansion_size = 8;
    }

    __builtin_memset(&pool->stats, 0, sizeof(memory_pool_stats_t));
    pool->stats.block_size = pool->options.block_size;

    if (!memory_pool_lock_init(pool)) {
        memory_free(pool);
        return NULL;
    }

    if (pool->options.name != NULL) {
        pool->name = memory_calloc(strlen(pool->options.name) + 1, "memory_pool_name");
        if (pool->name != NULL) {
            __builtin_memcpy(pool->name, pool->options.name, strlen(pool->options.name) + 1);
        }
    }

    memory_pool_lock(pool);

    if (!memory_pool_allocate_blocks(pool, pool->options.initial_blocks)) {
        memory_pool_unlock(pool);
        memory_pool_lock_destroy(pool);
        if (pool->name != NULL) {
            memory_free(pool->name);
        }
        memory_free(pool);
        return NULL;
    }

    memory_pool_unlock(pool);

    AIRY_LOG_INFO("memory_pool: memory_pool_create ok (pool=%p, block_size=%zu, total_blocks=%zu)",
             (void *)pool, pool->options.block_size, pool->stats.total_blocks);

    return pool;
}

void memory_pool_destroy(memory_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    const char *pool_name = pool->name ? pool->name : "(unnamed)";
    AIRY_LOG_INFO("memory_pool: memory_pool_destroy (pool=%p, name=%s, total_blocks=%zu, "
             "allocated=%zu, free=%zu, allocs=%" PRIu64 ", frees=%" PRIu64 ", hits=%" PRIu64
             ", miss=%" PRIu64 ")",
             (void *)pool, pool_name, pool->stats.total_blocks, pool->stats.allocated_blocks,
             pool->stats.free_blocks, pool->stats.allocation_count, pool->stats.free_count,
             pool->stats.hit_count, pool->stats.miss_count);

    size_t leaked_blocks = 0;
    const char *pool_name_for_log = NULL;

    memory_pool_lock(pool);

    if (pool->stats.allocated_blocks > 0) {
        leaked_blocks = pool->stats.allocated_blocks;
        pool_name_for_log = pool->name;
    }

    memory_pool_free_blocks(pool);

    memory_pool_unlock(pool);

    if (leaked_blocks > 0) {
        AIRY_LOG_WARN("警告：销毁内存池时发现未释放的块");
        AIRY_LOG_WARN("内存池：%s", pool_name_for_log ? pool_name_for_log : "(unnamed)");
        AIRY_LOG_WARN("未释放块数：%zu", leaked_blocks);
    }

    memory_pool_lock_destroy(pool);

    if (pool->name != NULL) {
        memory_free(pool->name);
    }

    memory_free(pool);
}

memory_pool_t *memory_pool_create_default(size_t block_size)
{
    memory_pool_options_t options = {.block_size = block_size,
                                     .initial_blocks = 16,
                                     .max_blocks = 0,
                                     .expansion_size = 8,
                                     .thread_safe = true,
                                     .name = NULL};

    return memory_pool_create(&options);
}
