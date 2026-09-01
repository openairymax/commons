// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_pool_alloc.c
 * @brief Unified memory management module - memory pool allocation/release
 *        and capacity management domain.
 *
 * Implements single/batch block allocation and release, plus capacity
 * management (prealloc/clear/expand/shrink), single responsibility.
 * Split out of memory_pool.c.
 */

#include "memory_pool.h"

#include "memory_pool_internal.h"

#include "airy_memory.h"

#include "logging.h"

#include <inttypes.h>

void *memory_pool_alloc(memory_pool_t *pool)
{
    if (pool == NULL) {
        return NULL;
    }

    memory_pool_lock(pool);

    if (pool->free_list == NULL) {
        pool->stats.miss_count++;
        AIRY_LOG_DEBUG("memory_pool: memory_pool_alloc MISS (pool=%p, free_blocks=0, miss#=%" PRIu64 ")",
                  (void *)pool, pool->stats.miss_count);

        if (!memory_pool_allocate_blocks(pool, pool->options.expansion_size)) {
            memory_pool_unlock(pool);
            AIRY_LOG_WARN("memory_pool: memory_pool_alloc EXPAND_FAILED (pool=%p)", (void *)pool);
            return NULL;
        }
    } else {
        pool->stats.hit_count++;
    }

    memory_pool_block_t *block = pool->free_list;
    pool->free_list = block->next;

    block->allocated = true;
    block->next = NULL;

    void *data_ptr = (uint8_t *)block + sizeof(memory_pool_block_t);

    pool->stats.allocated_blocks++;
    pool->stats.free_blocks--;
    pool->stats.used_memory += pool->options.block_size;
    pool->stats.allocation_count++;

    memory_pool_unlock(pool);

    AIRY_LOG_DEBUG("memory_pool: memory_pool_alloc ok (pool=%p, ptr=%p, block_index=%zu, "
              "free=%zu/%zu, alloc#=%" PRIu64 ")",
              (void *)pool, data_ptr, block->index, pool->stats.free_blocks,
              pool->stats.total_blocks, pool->stats.allocation_count);

    return data_ptr;
}

void *memory_pool_calloc(memory_pool_t *pool)
{
    void *ptr = memory_pool_alloc(pool);
    if (ptr != NULL) {
        __builtin_memset(ptr, 0, pool->options.block_size);
    }
    return ptr;
}

size_t memory_pool_batch_alloc(memory_pool_t *pool, size_t count, void **out_blocks)
{
    if (pool == NULL || out_blocks == NULL || count == 0) {
        return 0;
    }

    AIRY_LOG_DEBUG("memory_pool: memory_pool_batch_alloc START (pool=%p, count=%zu, free=%zu)",
              (void *)pool, count, pool->stats.free_blocks);

    memory_pool_lock(pool);

    size_t allocated = 0;
    for (size_t i = 0; i < count; i++) {

        if (pool->free_list == NULL) {
            if (!memory_pool_allocate_blocks(pool, pool->options.expansion_size)) {
                break;
            }
        }

        memory_pool_block_t *block = pool->free_list;
        pool->free_list = block->next;

        block->allocated = true;
        block->next = NULL;

        void *data_ptr = (uint8_t *)block + sizeof(memory_pool_block_t);
        out_blocks[allocated] = data_ptr;

        pool->stats.allocated_blocks++;
        pool->stats.free_blocks--;
        pool->stats.used_memory += pool->options.block_size;
        pool->stats.allocation_count++;
        allocated++;
    }

    pool->stats.hit_count += allocated;
    if (allocated < count) {
        pool->stats.miss_count += (count - allocated);
    }

    memory_pool_unlock(pool);

    AIRY_LOG_DEBUG("memory_pool: memory_pool_batch_alloc DONE (pool=%p, requested=%zu, allocated=%zu, "
              "free=%zu/%zu, alloc_total=%" PRIu64 ")",
              (void *)pool, count, allocated, pool->stats.free_blocks, pool->stats.total_blocks,
              pool->stats.allocation_count);

    return allocated;
}

size_t memory_pool_batch_free(memory_pool_t *pool, void **blocks, size_t count)
{
    if (pool == NULL || blocks == NULL || count == 0) {
        return 0;
    }

    AIRY_LOG_DEBUG("memory_pool: memory_pool_batch_free START (pool=%p, count=%zu, allocated=%zu)",
              (void *)pool, count, pool->stats.allocated_blocks);

    memory_pool_lock(pool);

    size_t freed = 0;
    for (size_t i = 0; i < count; i++) {
        if (blocks[i] == NULL)
            continue;

        memory_pool_block_t *block =
            (memory_pool_block_t *)((uint8_t *)blocks[i] - sizeof(memory_pool_block_t));

        if (block->pool != pool || !block->allocated) {
            AIRY_LOG_WARN("memory_pool: memory_pool_batch_free skip invalid block (pool=%p, ptr=%p)",
                     (void *)pool, blocks[i]);
            continue;
        }

        block->allocated = false;
        block->next = pool->free_list;
        pool->free_list = block;

        pool->stats.allocated_blocks--;
        pool->stats.free_blocks++;
        pool->stats.used_memory -= pool->options.block_size;
        pool->stats.free_count++;
        freed++;
    }

    memory_pool_unlock(pool);

    AIRY_LOG_DEBUG("memory_pool: memory_pool_batch_free DONE (pool=%p, count=%zu, freed=%zu, "
              "free=%zu/%zu, free_total=%" PRIu64 ")",
              (void *)pool, count, freed, pool->stats.free_blocks, pool->stats.total_blocks,
              pool->stats.free_count);

    return freed;
}

void memory_pool_free(memory_pool_t *pool, void *ptr)
{
    if (pool == NULL || ptr == NULL) {
        return;
    }

    memory_pool_block_t *block =
        (memory_pool_block_t *)((uint8_t *)ptr - sizeof(memory_pool_block_t));

    memory_pool_lock(pool);

    /* O(1) ownership validation via the embedded pool pointer
     * (replaces the former O(n) linear scan) */
    if (block->pool != pool || !block->allocated) {
        AIRY_LOG_ERROR("错误：尝试释放无效的内存池块");
        memory_pool_unlock(pool);
        return;
    }

    block->allocated = false;

    block->next = pool->free_list;
    pool->free_list = block;

    pool->stats.allocated_blocks--;
    pool->stats.free_blocks++;
    pool->stats.used_memory -= pool->options.block_size;
    pool->stats.free_count++;

    memory_pool_unlock(pool);

    AIRY_LOG_DEBUG("memory_pool: memory_pool_free ok (pool=%p, ptr=%p, block_index=%zu, "
              "free=%zu/%zu, free#=%" PRIu64 ")",
              (void *)pool, ptr, block->index, pool->stats.free_blocks, pool->stats.total_blocks,
              pool->stats.free_count);
}

bool memory_pool_prealloc(memory_pool_t *pool, size_t count)
{
    if (pool == NULL || count == 0) {
        return false;
    }

    AIRY_LOG_INFO("memory_pool: memory_pool_prealloc (pool=%p, count=%zu)", (void *)pool, count);

    memory_pool_lock(pool);
    bool result = memory_pool_allocate_blocks(pool, count);
    memory_pool_unlock(pool);

    return result;
}

void memory_pool_clear(memory_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    AIRY_LOG_INFO("memory_pool: memory_pool_clear (pool=%p, allocated=%zu, total=%zu)", (void *)pool,
             pool->stats.allocated_blocks, pool->stats.total_blocks);

    memory_pool_lock(pool);

    size_t blocks_to_keep = pool->stats.allocated_blocks;

    if (blocks_to_keep == 0) {
        memory_pool_free_blocks(pool);

        if (pool->options.initial_blocks > 0) {
            memory_pool_allocate_blocks(pool, pool->options.initial_blocks);
        }
    }

    memory_pool_unlock(pool);
}

bool memory_pool_expand(memory_pool_t *pool, size_t additional_blocks)
{
    if (pool == NULL || additional_blocks == 0) {
        return false;
    }

    AIRY_LOG_INFO("memory_pool: memory_pool_expand (pool=%p, additional=%zu, total=%zu→%zu)",
             (void *)pool, additional_blocks, pool->stats.total_blocks,
             pool->stats.total_blocks + additional_blocks);

    memory_pool_lock(pool);
    bool result = memory_pool_allocate_blocks(pool, additional_blocks);
    memory_pool_unlock(pool);

    return result;
}

size_t memory_pool_shrink(memory_pool_t *pool, size_t blocks_to_keep)
{
    if (pool == NULL) {
        return 0;
    }

    AIRY_LOG_INFO("memory_pool: memory_pool_shrink (pool=%p, keep=%zu, total=%zu)", (void *)pool,
             blocks_to_keep, pool->stats.total_blocks);

    memory_pool_lock(pool);

    if (blocks_to_keep < pool->stats.allocated_blocks) {
        blocks_to_keep = pool->stats.allocated_blocks;
    }

    size_t blocks_to_free = 0;
    if (pool->stats.total_blocks > blocks_to_keep) {
        blocks_to_free = pool->stats.total_blocks - blocks_to_keep;
    }

    if (blocks_to_free == 0) {
        memory_pool_unlock(pool);
        return 0;
    }

    size_t freed = 0;
    memory_pool_block_t *prev = NULL;
    memory_pool_block_t *current = pool->free_list;

    while (current != NULL && freed < blocks_to_free) {
        memory_pool_block_t *next = current->next;

        AIRY_FREE(current);
        pool->stats.total_blocks--;
        pool->stats.free_blocks--;
        freed++;

        if (prev == NULL) {
            pool->free_list = next;
        } else {
            prev->next = next;
        }
        current = next;
    }

    memory_pool_unlock(pool);
    return freed;
}
