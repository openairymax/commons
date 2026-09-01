/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file arena.h
 * @brief P1.19: short-lifetime linear arena allocator.
 *
 * The arena allocator provides O(1) allocation and O(1) bulk release
 * (reset), suitable for short-lifetime allocations on request handling
 * paths.
 *
 * Architecture:
 *   arena_create()  -> create an arena (initial 64KB chunk)
 *   arena_alloc()   -> O(1) linear bump allocation
 *   arena_reset()   -> O(1) bulk release (rewind the bump pointers)
 *   arena_destroy() -> destroy the arena
 *
 * Features:
 *   - Chained growth: large allocations automatically add new chunks
 *     (linked list)
 *   - Thread-local storage: per-thread arenas reduce lock contention
 *   - mark/release: temporary rollback points (bump pointer snapshots)
 */

#ifndef AIRY_RT_ARENA_H
#define AIRY_RT_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define ARENA_DEFAULT_CHUNK_SIZE (64 * 1024)
#define ARENA_MAX_CHUNK_SIZE (1024 * 1024)
#define ARENA_ALIGNMENT 16

typedef struct airy_arena airy_arena_t;


typedef struct {
    size_t total_allocated;
    size_t current_used;
    size_t chunk_count;
    size_t total_chunk_bytes;
    uint64_t alloc_count;
    uint64_t reset_count;
    uint64_t fallback_count;
} arena_stats_t;


typedef struct {
    airy_arena_t *arena;
    void *bump;
    airy_arena_t *chunk;
} arena_mark_t;


/**
 * @brief P1.19.2: Create an arena allocator.
 *
 * @ownership alloc - the returned arena handle is owned by the caller
 * and must be released with arena_destroy
 *
 * @param chunk_size  Initial chunk size (0 uses the 64KB default)
 * @param max_chunks  Maximum number of chunks (0 means unlimited)
 * @return Arena handle, NULL on failure
 */
airy_arena_t *arena_create(size_t chunk_size, size_t max_chunks);

/**
 * @brief P1.19.2: Destroy an arena and free all memory.
 *
 * @ownership release - releases ownership of the arena handle; all
 * pointers allocated from the arena become invalid
 *
 * @param arena Arena handle
 */
void arena_destroy(airy_arena_t *arena);


/**
 * @brief P1.19.2: Allocate memory linearly from the arena (bump forward).
 *
 * @ownership borrow - the returned pointer's lifetime is managed by the
 * arena; invalid after arena_reset/destroy
 *
 * Allocation is O(1) and fragmentation-free. When the current chunk is
 * insufficient a new chunk is allocated automatically. Oversized
 * allocations (> chunk_size / 2) fall back to malloc directly and do not
 * consume arena space.
 *
 * @param arena Arena handle
 * @param size  Allocation size (bytes)
 * @return Allocated memory pointer (16-byte aligned), NULL on failure
 */
void *arena_alloc(airy_arena_t *arena, size_t size);

/**
 * @brief P1.19.2: Allocate and zero memory from the arena.
 *
 * @ownership borrow - the returned pointer's lifetime is managed by the
 * arena; invalid after arena_reset/destroy
 *
 * @param arena Arena handle
 * @param size  Allocation size (bytes)
 * @return Zeroed memory pointer, NULL on failure
 */
void *arena_calloc(airy_arena_t *arena, size_t size);


/**
 * @brief P1.19.2: Reset the whole arena (O(1)).
 *
 * Rewinds every chunk's bump pointer to the start; all previously
 * allocated memory becomes invalid. The caller must ensure no pointers
 * allocated from the arena are used anymore.
 *
 * @param arena Arena handle
 */
void arena_reset(airy_arena_t *arena);


/**
 * @brief P1.19.2: Create a rollback mark (snapshot of the bump pointer).
 *
 * Used for request-processing stages that allocate temporarily and then
 * roll back.
 *
 * @param arena Arena handle
 * @param mark  Output mark (caller-allocated)
 */
void arena_mark(airy_arena_t *arena, arena_mark_t *mark);

/**
 * @brief P1.19.2: Roll back to a mark position.
 *
 * All memory allocated via arena_alloc after the mark becomes invalid.
 *
 * @param mark Mark previously created via arena_mark
 */
void arena_release(arena_mark_t *mark);


/**
 * @brief Get arena stats.
 * @param arena Arena handle
 * @param stats Output stats
 * @return true on success
 */
bool arena_get_stats(airy_arena_t *arena, arena_stats_t *stats);

/**
 * @brief Check whether a pointer lies within one of the arena's chunks.
 * @param arena Arena handle
 * @param ptr   Pointer to check
 * @return true if it belongs to this arena
 */
bool arena_contains(airy_arena_t *arena, const void *ptr);

/**
 * @brief Get the arena's current total capacity.
 * @param arena Arena handle
 * @return Total capacity (bytes)
 */
size_t arena_capacity(airy_arena_t *arena);

/**
 * @brief Get the arena's current usage.
 * @param arena Arena handle
 * @return Used bytes
 */
size_t arena_used(airy_arena_t *arena);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_ARENA_H */