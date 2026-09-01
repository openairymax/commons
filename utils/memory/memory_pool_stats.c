// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_pool_stats.c
 * @brief Unified memory management module - memory pool statistics, queries
 *        and validation domain.
 *
 * Implements statistics snapshots/reset, emptiness/fullness queries,
 * structural validation, block iteration and pool naming, single
 * responsibility. Split out of memory_pool.c.
 */

#include "memory_pool.h"

#include "memory_pool_internal.h"

#include "airy_memory.h"

#include "logging.h"

#include <string.h>

bool memory_pool_get_stats(memory_pool_t *pool, memory_pool_stats_t *stats)
{
    if (pool == NULL || stats == NULL) {
        return false;
    }

    memory_pool_lock(pool);
    __builtin_memcpy(stats, &pool->stats, sizeof(memory_pool_stats_t));
    memory_pool_unlock(pool);

    return true;
}

void memory_pool_reset_stats(memory_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    memory_pool_lock(pool);

    pool->stats.allocation_count = 0;
    pool->stats.free_count = 0;
    pool->stats.hit_count = 0;
    pool->stats.miss_count = 0;

    memory_pool_unlock(pool);
}

bool memory_pool_is_empty(memory_pool_t *pool)
{
    if (pool == NULL) {
        return true;
    }

    memory_pool_lock(pool);
    bool empty = (pool->stats.allocated_blocks == 0);
    memory_pool_unlock(pool);

    return empty;
}

bool memory_pool_is_full(memory_pool_t *pool)
{
    if (pool == NULL) {
        return false;
    }

    memory_pool_lock(pool);

    bool full = false;
    if (pool->options.max_blocks > 0) {
        full = (pool->stats.total_blocks >= pool->options.max_blocks);
    }

    memory_pool_unlock(pool);

    return full;
}

bool memory_pool_validate(memory_pool_t *pool)
{
    if (pool == NULL) {
        return false;
    }

    memory_pool_lock(pool);

    bool valid = true;

    if (pool->stats.total_blocks != pool->stats.allocated_blocks + pool->stats.free_blocks) {
        valid = false;
    }

    if (pool->stats.total_memory !=
        pool->stats.total_blocks * (sizeof(memory_pool_block_t) + pool->options.block_size)) {
        valid = false;
    }

    if (pool->stats.used_memory != pool->stats.allocated_blocks * pool->options.block_size) {
        valid = false;
    }

    size_t free_count = 0;
    memory_pool_block_t *current = pool->free_list;
    while (current != NULL) {
        free_count++;

        if (current->allocated) {
            valid = false;
            break;
        }

        current = current->next;
    }

    if (free_count != pool->stats.free_blocks) {
        valid = false;
    }

    memory_pool_unlock(pool);

    return valid;
}

void memory_pool_iterate(memory_pool_t *pool,
                         void (*callback)(void *block, bool allocated, void *user_data),
                         void *user_data)
{

    if (pool == NULL || callback == NULL) {
        return;
    }

    memory_pool_lock(pool);

    for (size_t i = 0; i < pool->stats.total_blocks; i++) {
        memory_pool_block_t *block = pool->blocks[i];
        if (block != NULL) {
            void *data_ptr = (uint8_t *)block + sizeof(memory_pool_block_t);
            callback(data_ptr, block->allocated, user_data);
        }
    }

    memory_pool_unlock(pool);
}

const char *memory_pool_get_name(memory_pool_t *pool)
{
    if (pool == NULL) {
        return NULL;
    }

    return pool->name;
}

void memory_pool_set_name(memory_pool_t *pool, const char *name)
{
    if (pool == NULL) {
        return;
    }

    memory_pool_lock(pool);

    if (pool->name != NULL) {
        memory_free(pool->name);
        pool->name = NULL;
    }

    if (name != NULL) {
        pool->name = memory_calloc(strlen(name) + 1, "memory_pool_name");
        if (pool->name != NULL) {
            __builtin_memcpy(pool->name, name, strlen(name) + 1);
        }
    }

    memory_pool_unlock(pool);
}
