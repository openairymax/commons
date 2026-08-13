// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_pool.c
 * @brief Unified memory management module - memory pool implementation.
 *
 * Implements an efficient memory pool that reduces fragmentation and
 * allocation overhead. Free blocks are managed via a linked list, with
 * thread safety and dynamic growth support.
 */

#include "memory_pool.h"

#include "airy_memory.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "logging.h"

#include <stdio.h>
#include <string.h>
#include "platform.h"
#include <stdint.h>

/**
 * @brief Memory pool block structure.
 * @note Performance optimization: the block header embeds the pool pointer
 * for O(1) ownership validation, avoiding O(n) linear scans.
 */
typedef struct memory_pool_block {
    struct memory_pool_block *next;
    bool allocated;
    size_t index;
    struct memory_pool *pool;
} memory_pool_block_t;

/**
 * @brief Old region list node (tracks old regions produced by growth;
 * released together at destruction).
 */
typedef struct memory_region_node {
    void *region;
    size_t region_size;
    struct memory_region_node *next;
} memory_region_node_t;

struct memory_pool {
    memory_pool_options_t options;

    void *memory_area;
    size_t memory_area_size;
    memory_pool_block_t **blocks;
    size_t blocks_capacity;

    memory_pool_block_t *free_list;

    memory_pool_stats_t stats;

    airy_mtx_t lock;

    char *name;

    memory_region_node_t *old_regions;
};

static bool memory_pool_lock_init(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return true;
    }

    return airy_mtx_init(&pool->lock) == 0;
}

static void memory_pool_lock_destroy(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return;
    }

    airy_mtx_destroy(&pool->lock);
}

static void memory_pool_lock(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return;
    }

    airy_mtx_lock(&pool->lock);
}

static void memory_pool_unlock(memory_pool_t *pool)
{
    if (!pool->options.thread_safe) {
        return;
    }

    airy_mtx_unlock(&pool->lock);
}

static size_t memory_pool_align_size(size_t size, size_t alignment)
{
    if (alignment == 0) {
        return size;
    }

    return ((size + alignment - 1) / alignment) * alignment;
}

static bool memory_pool_allocate_blocks(memory_pool_t *pool, size_t block_count)
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
        LOG_DEBUG(
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

static void memory_pool_free_blocks(memory_pool_t *pool)
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

    LOG_INFO("memory_pool: memory_pool_create (block_size=%zu, initial_blocks=%zu, max_blocks=%zu, "
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

    LOG_INFO("memory_pool: memory_pool_create ok (pool=%p, block_size=%zu, total_blocks=%zu)",
             (void *)pool, pool->options.block_size, pool->stats.total_blocks);

    return pool;
}

void memory_pool_destroy(memory_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    const char *pool_name = pool->name ? pool->name : "(unnamed)";
    LOG_INFO("memory_pool: memory_pool_destroy (pool=%p, name=%s, total_blocks=%zu, "
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
        LOG_WARN("警告：销毁内存池时发现未释放的块");
        LOG_WARN("内存池：%s", pool_name_for_log ? pool_name_for_log : "(unnamed)");
        LOG_WARN("未释放块数：%zu", leaked_blocks);
    }

    memory_pool_lock_destroy(pool);

    if (pool->name != NULL) {
        memory_free(pool->name);
    }

    memory_free(pool);
}

void *memory_pool_alloc(memory_pool_t *pool)
{
    if (pool == NULL) {
        return NULL;
    }

    memory_pool_lock(pool);

    if (pool->free_list == NULL) {
        pool->stats.miss_count++;
        LOG_DEBUG("memory_pool: memory_pool_alloc MISS (pool=%p, free_blocks=0, miss#=%" PRIu64 ")",
                  (void *)pool, pool->stats.miss_count);

        if (!memory_pool_allocate_blocks(pool, pool->options.expansion_size)) {
            memory_pool_unlock(pool);
            LOG_WARN("memory_pool: memory_pool_alloc EXPAND_FAILED (pool=%p)", (void *)pool);
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

    LOG_DEBUG("memory_pool: memory_pool_alloc ok (pool=%p, ptr=%p, block_index=%zu, "
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

    LOG_DEBUG("memory_pool: memory_pool_batch_alloc START (pool=%p, count=%zu, free=%zu)",
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

    LOG_DEBUG("memory_pool: memory_pool_batch_alloc DONE (pool=%p, requested=%zu, allocated=%zu, "
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

    LOG_DEBUG("memory_pool: memory_pool_batch_free START (pool=%p, count=%zu, allocated=%zu)",
              (void *)pool, count, pool->stats.allocated_blocks);

    memory_pool_lock(pool);

    size_t freed = 0;
    for (size_t i = 0; i < count; i++) {
        if (blocks[i] == NULL)
            continue;

        memory_pool_block_t *block =
            (memory_pool_block_t *)((uint8_t *)blocks[i] - sizeof(memory_pool_block_t));

        if (block->pool != pool || !block->allocated) {
            LOG_WARN("memory_pool: memory_pool_batch_free skip invalid block (pool=%p, ptr=%p)",
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

    LOG_DEBUG("memory_pool: memory_pool_batch_free DONE (pool=%p, count=%zu, freed=%zu, "
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
        LOG_ERROR("错误：尝试释放无效的内存池块");
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

    LOG_DEBUG("memory_pool: memory_pool_free ok (pool=%p, ptr=%p, block_index=%zu, "
              "free=%zu/%zu, free#=%" PRIu64 ")",
              (void *)pool, ptr, block->index, pool->stats.free_blocks, pool->stats.total_blocks,
              pool->stats.free_count);
}

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

bool memory_pool_prealloc(memory_pool_t *pool, size_t count)
{
    if (pool == NULL || count == 0) {
        return false;
    }

    LOG_INFO("memory_pool: memory_pool_prealloc (pool=%p, count=%zu)", (void *)pool, count);

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

    LOG_INFO("memory_pool: memory_pool_clear (pool=%p, allocated=%zu, total=%zu)", (void *)pool,
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

bool memory_pool_expand(memory_pool_t *pool, size_t additional_blocks)
{
    if (pool == NULL || additional_blocks == 0) {
        return false;
    }

    LOG_INFO("memory_pool: memory_pool_expand (pool=%p, additional=%zu, total=%zu→%zu)",
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

    LOG_INFO("memory_pool: memory_pool_shrink (pool=%p, keep=%zu, total=%zu)", (void *)pool,
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