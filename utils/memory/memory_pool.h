/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file memory_pool.h
 * @brief Unified memory management module: memory pool management.
 *
 * Provides efficient memory pool management, reducing memory fragmentation
 * and allocation overhead. Suitable for scenarios that frequently allocate
 * and free same-size memory blocks.
 */

#ifndef AIRY_RT_MEMORY_POOL_H
#define AIRY_RT_MEMORY_POOL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_pool_api Memory pool API
 * @{
 */

/**
 * @brief Memory pool options
 */
typedef struct {
    size_t block_size;
    size_t initial_blocks;
    size_t max_blocks;
    size_t expansion_size;
    bool thread_safe;
    const char *name;
} memory_pool_options_t;

/**
 * @brief Memory pool handle (opaque type)
 */
typedef struct memory_pool memory_pool_t;

/**
 * @brief Memory pool statistics
 */
typedef struct {
    size_t block_size;
    size_t total_blocks;
    size_t allocated_blocks;
    size_t free_blocks;
    size_t total_memory;
    size_t used_memory;
    size_t allocation_count;
    size_t free_count;
    size_t hit_count;
    size_t miss_count;
} memory_pool_stats_t;

/**
 * @brief Create a memory pool
 *
 * @ownership alloc -- the returned pool handle is caller-owned; release
 *                     with memory_pool_destroy
 *
 * @param[in] options Memory pool options (must not be NULL)
 * @return Pool handle on success, NULL on failure
 *
 * @note The pool preallocates initial_blocks blocks on creation
 */
memory_pool_t *memory_pool_create(const memory_pool_options_t *options);

/**
 * @brief Destroy a memory pool
 *
 * @ownership release -- releases ownership of the pool handle; pool is
 *                       invalid after destruction
 *
 * @param[in] pool Memory pool handle
 *
 * @note Warns if the pool still contains unreleased blocks
 * @note Checks for leaks if memory debugging is enabled
 */
void memory_pool_destroy(memory_pool_t *pool);

/**
 * @brief Allocate a block from the memory pool
 *
 * @ownership alloc -- the returned block is caller-owned; return it with
 *                     memory_pool_free
 *
 * @param[in] pool Memory pool handle
 * @return Block pointer on success, NULL on failure
 *
 * @note The block size is the block_size specified at creation
 * @note The allocated block is uninitialized
 */
void *memory_pool_alloc(memory_pool_t *pool);

/**
 * @brief Allocate and zero a block from the memory pool
 *
 * @ownership alloc -- the returned block is caller-owned; return it with
 *                     memory_pool_free
 *
 * @param[in] pool Memory pool handle
 * @return Block pointer on success, NULL on failure
 *
 * @note The allocated block is zeroed
 */
void *memory_pool_calloc(memory_pool_t *pool);

/**
 * @brief Return a block to the memory pool
 *
 * @ownership release -- releases ownership of ptr; ptr is invalid after
 *                       the call
 *
 * @param[in] pool Memory pool handle
 * @param[in] ptr Block pointer to release
 *
 * @note No-op if ptr is NULL
 * @note Only blocks allocated from the same pool may be released
 */
void memory_pool_free(memory_pool_t *pool, void *ptr);

/**
 * @brief Safely release a block and set the pointer to NULL
 *
 * @param[in] pool Memory pool handle
 * @param[inout] ptr_ptr Pointer to the block pointer
 *
 * @note Automatically sets the pointer to NULL after release
 */
#define MEMORY_POOL_FREE_SAFE(pool, ptr_ptr)           \
    do {                                               \
        if ((ptr_ptr) != NULL && *(ptr_ptr) != NULL) { \
            memory_pool_free((pool), *(ptr_ptr));      \
            *(ptr_ptr) = NULL;                         \
        }                                              \
    } while (0)

/**
 * @brief Get memory pool statistics
 *
 * @param[in] pool Memory pool handle
 * @param[out] stats Statistics output buffer
 * @return true on success, false on failure
 */
bool memory_pool_get_stats(memory_pool_t *pool, memory_pool_stats_t *stats);

/**
 * @brief Reset memory pool statistics
 *
 * @param[in] pool Memory pool handle
 */
void memory_pool_reset_stats(memory_pool_t *pool);

/**
 * @brief Preallocate blocks
 *
 * @param[in] pool Memory pool handle
 * @param[in] count Number of blocks to preallocate
 * @return true on success, false on failure
 *
 * @note Preallocation improves the performance of subsequent allocations
 */
bool memory_pool_prealloc(memory_pool_t *pool, size_t count);

/**
 * @brief Clear all free blocks in the pool
 *
 * @param[in] pool Memory pool handle
 *
 * @note Only free blocks are cleared; allocated blocks are unaffected
 * @note After the call, the pool shrinks to contain only allocated blocks
 */
void memory_pool_clear(memory_pool_t *pool);

/**
 * @brief Check whether the pool is empty
 *
 * @param[in] pool Memory pool handle
 * @return true if the pool has no allocated blocks
 */
bool memory_pool_is_empty(memory_pool_t *pool);

/**
 * @brief Check whether the pool is full
 *
 * @param[in] pool Memory pool handle
 * @return true if the pool is full (reached the max_blocks limit)
 */
bool memory_pool_is_full(memory_pool_t *pool);

/**
 * @brief Expand the memory pool
 *
 * @param[in] pool Memory pool handle
 * @param[in] additional_blocks Number of blocks to add
 * @return true on success, false on failure
 */
bool memory_pool_expand(memory_pool_t *pool, size_t additional_blocks);

/**
 * @brief Shrink the memory pool
 *
 * @param[in] pool Memory pool handle
 * @param[in] blocks_to_keep Minimum blocks to keep (including allocated)
 * @return Number of blocks successfully released
 *
 * @note Only free blocks can be shrunk; at least blocks_to_keep blocks are
 *       retained
 */
size_t memory_pool_shrink(memory_pool_t *pool, size_t blocks_to_keep);

/**
 * @brief Validate pool integrity
 *
 * @param[in] pool Memory pool handle
 * @return true if intact, false if corrupted
 *
 * @note Requires memory debugging to be enabled
 */
bool memory_pool_validate(memory_pool_t *pool);

/**
 * @brief Iterate over all blocks in the pool
 *
 * @param[in] pool Memory pool handle
 * @param[in] callback Callback invoked for each block
 * @param[in] user_data User data passed to the callback
 *
 * @note Callback prototype: void callback(void* block, bool allocated,
 *       void* user_data)
 * @note Mainly for debugging and monitoring
 */
void memory_pool_iterate(memory_pool_t *pool,
                         void (*callback)(void *block, bool allocated, void *user_data),
                         void *user_data);

/**
 * @brief Get the memory pool name
 *
 * @param[in] pool Memory pool handle
 * @return Pool name (may be NULL)
 */
const char *memory_pool_get_name(memory_pool_t *pool);

/**
 * @brief Set the memory pool name
 *
 * @param[in] pool Memory pool handle
 * @param[in] name New name (may be NULL)
 */
void memory_pool_set_name(memory_pool_t *pool, const char *name);

/**
 * @brief Create a pool with default options
 *
 * @param[in] block_size Block size
 * @return Pool handle on success, NULL on failure
 *
 * @note Uses default options: initial_blocks=16, max_blocks=0,
 *       expansion_size=8, thread_safe=true
 */
memory_pool_t *memory_pool_create_default(size_t block_size);


/**
 * @brief P1.20.3: Batch-allocate blocks (single lock acquisition)
 *
 * @ownership alloc -- the returned blocks are caller-owned; return each
 *                     with memory_pool_free
 *
 * Allocates multiple blocks in one lock operation, reducing lock
 * contention. Designed for tcache batch filling, also usable for other
 * batch-allocation scenarios.
 *
 * @param[in]  pool       Memory pool handle
 * @param[in]  count      Number of blocks requested
 * @param[out] out_blocks Output block pointer array (caller-allocated,
 *                        size >= count)
 * @return Number of blocks actually allocated (may be < count if the pool
 *         is empty)
 *
 * @note The returned blocks are uninitialized
 * @note Performance: N-1 fewer lock operations than a loop of
 *       memory_pool_alloc calls
 */
size_t memory_pool_batch_alloc(memory_pool_t *pool, size_t count, void **out_blocks);

/**
 * @brief P1.20.3: Batch-release blocks (single lock acquisition)
 *
 * @ownership release -- releases ownership of all pointers in blocks
 *
 * Releases multiple blocks in one lock operation, reducing lock
 * contention. Designed for tcache batch return, also usable for other
 * batch-release scenarios.
 *
 * @param[in] pool   Memory pool handle
 * @param[in] blocks Block pointer array to release
 * @param[in] count  Number of blocks
 * @return Number of blocks successfully released
 *
 * @note Invalid blocks (NULL or not from this pool) are skipped
 * @note Performance: N-1 fewer lock operations than a loop of
 *       memory_pool_free calls
 */
size_t memory_pool_batch_free(memory_pool_t *pool, void **blocks, size_t count);

/** @} */ /* end of memory_pool_api */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_MEMORY_POOL_H */
