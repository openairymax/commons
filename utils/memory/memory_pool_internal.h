/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file memory_pool_internal.h
 * @brief Memory pool module - internal shared definitions.
 *
 * After memory_pool.c was split by functional domain, this header carries
 * the pool object layouts and cross-file helper declarations:
 *   - memory_pool.c        block region management + lifecycle
 *   - memory_pool_alloc.c  allocation/release and capacity management
 *   - memory_pool_stats.c  statistics, queries and validation
 */

#ifndef AIRY_RT_MEMORY_POOL_INTERNAL_H
#define AIRY_RT_MEMORY_POOL_INTERNAL_H

#include "memory_pool.h"

#include <stdbool.h>
#include <stddef.h>

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* Lock primitives (memory_pool.c) */
bool memory_pool_lock_init(memory_pool_t *pool);
void memory_pool_lock_destroy(memory_pool_t *pool);
void memory_pool_lock(memory_pool_t *pool);
void memory_pool_unlock(memory_pool_t *pool);

/* Block region management (memory_pool.c) */
bool memory_pool_allocate_blocks(memory_pool_t *pool, size_t block_count);
void memory_pool_free_blocks(memory_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_MEMORY_POOL_INTERNAL_H */
