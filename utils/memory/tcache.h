/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file tcache.h
 * @brief P1.20: per-thread cache layer - reduces pool.c global lock contention.
 *
 * Each thread keeps a local cache (tcache) and fetches/returns blocks in
 * batches from the global memory pool, reducing lock contention on the
 * pool.c global airy_mtx_t.
 *
 * Design:
 *   - _Thread_local storage for each thread's cache
 *   - Batch fill: acquire multiple blocks in one lock operation
 *   - Batch flush: return multiple blocks at once when the cache is full
 *   - Cap: each tcache caches at most TCACHE_MAX_CACHED blocks
 *
 * Performance goal: > 30% lower single-thread allocation latency.
 */

#ifndef AIRY_RT_TCACHE_H
#define AIRY_RT_TCACHE_H

#include "memory_pool.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define TCACHE_DEFAULT_BATCH_SIZE 16
#define TCACHE_MAX_CACHED 64
#define TCACHE_FLUSH_THRESHOLD 48

typedef struct airy_tcache airy_tcache_t;


typedef struct {
    uint64_t alloc_count;
    uint64_t free_count;
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t batch_fill_count;
    uint64_t batch_flush_count;
    uint64_t bypass_count;
    double hit_rate;
} tcache_stats_t;


/**
 * @brief P1.20.1: Create a tcache (usually one per thread).
 *
 * @ownership alloc - the returned tcache handle is owned by the caller
 * and must be released with tcache_destroy
 *
 * @param pool       Associated memory pool
 * @param batch_size Batch size (0 uses the default)
 * @param max_cached Max cached blocks (0 uses the default)
 * @return tcache handle, NULL on failure
 */
airy_tcache_t *tcache_create(memory_pool_t *pool, size_t batch_size, size_t max_cached);

/**
 * @brief P1.20.1: Destroy a tcache (return all cached blocks to the pool).
 *
 * @ownership release - releases ownership of tc; tc is invalid after
 *
 * @param tc tcache handle
 */
void tcache_destroy(airy_tcache_t *tc);


/**
 * @brief P1.20.2: Fast allocation from the tcache.
 *
 * @ownership alloc - the returned block is owned by the caller and must
 * be returned via tcache_free
 *
 * Serves from the thread-local cache first; only hits the global pool
 * on a miss.
 *
 * @param tc tcache handle
 * @return Block pointer, NULL on failure
 */
void *tcache_alloc(airy_tcache_t *tc);

/**
 * @brief P1.20.2: Return a block to the tcache.
 *
 * @ownership release - releases ownership of ptr; ptr is invalid after
 *
 * Returns to the thread-local cache first; when the cache is full,
 * blocks are returned to the global pool in a batch.
 *
 * @param tc  tcache handle
 * @param ptr Block pointer (may be NULL)
 */
void tcache_free(airy_tcache_t *tc, void *ptr);


/**
 * @brief P1.20.1: Batch fill the tcache from the global pool.
 * @param tc tcache handle
 * @return Number of blocks filled
 */
size_t tcache_batch_fill(airy_tcache_t *tc);

/**
 * @brief P1.20.1: Batch return tcache blocks to the global pool.
 *
 * Returns cached blocks exceeding TCACHE_FLUSH_THRESHOLD.
 *
 * @param tc tcache handle
 * @return Number of blocks returned
 */
size_t tcache_batch_flush(airy_tcache_t *tc);

/**
 * @brief Immediately return all cached blocks to the global pool.
 * @param tc tcache handle
 */
void tcache_flush_all(airy_tcache_t *tc);


/**
 * @brief Get tcache stats.
 * @param tc    tcache handle
 * @param stats Output stats
 * @return true on success
 */
bool tcache_get_stats(airy_tcache_t *tc, tcache_stats_t *stats);

/**
 * @brief Get the tcache's current cached block count.
 * @param tc tcache handle
 * @return Cached block count
 */
size_t tcache_cached_count(airy_tcache_t *tc);

/**
 * @brief Check whether the tcache is full (reached the max_cached cap).
 * @param tc tcache handle
 * @return true if full
 */
bool tcache_is_full(airy_tcache_t *tc);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TCACHE_H */