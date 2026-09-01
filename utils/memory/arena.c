// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file arena.c
 * @brief P1.19: arena linear allocator implementation.
 *
 * Implements the arena_create/destroy/alloc/calloc/reset/mark/release
 * API. Chunks are chained via a linked list, bump-pointer linear
 * allocation, O(1) bulk release.
 *
 */

#include "arena.h"

#include "logging.h"
#include "airy_memory.h"
#include "platform.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct arena_chunk {
    struct arena_chunk *next;
    uint8_t *start;
    uint8_t *bump;
    uint8_t *end;
    size_t size;
} arena_chunk_t;

struct airy_arena {
    arena_chunk_t *first_chunk;
    arena_chunk_t *current;
    size_t chunk_size;
    size_t max_chunks;
    size_t num_chunks;

    size_t total_allocated;
    uint64_t alloc_count;
    uint64_t reset_count;
    uint64_t fallback_count;

    airy_mtx_t lock;
    bool thread_safe;
};

static arena_chunk_t *chunk_create(size_t size)
{
    arena_chunk_t *chunk = (arena_chunk_t *)AIRY_MALLOC(sizeof(arena_chunk_t));
    if (!chunk) {
        AIRY_LOG_ERROR("arena: chunk_create failed to alloc chunk_t (size=%zu)", size);
        return NULL;
    }

    chunk->start = (uint8_t *)AIRY_MALLOC(size);
    if (!chunk->start) {
        AIRY_LOG_ERROR("arena: chunk_create failed to alloc data region (size=%zu)", size);
        AIRY_FREE(chunk);
        return NULL;
    }

    chunk->bump = chunk->start;
    chunk->end = chunk->start + size;
    chunk->size = size;
    chunk->next = NULL;

    AIRY_LOG_DEBUG("arena: chunk_create ok (size=%zu, start=%p, end=%p)", size, (void *)chunk->start,
              (void *)chunk->end);
    return chunk;
}

static void chunk_destroy(arena_chunk_t *chunk)
{
    while (chunk) {
        arena_chunk_t *next = chunk->next;
        AIRY_FREE(chunk->start);
        AIRY_FREE(chunk);
        chunk = next;
    }
}

airy_arena_t *arena_create(size_t chunk_size, size_t max_chunks)
{
    if (chunk_size == 0) {
        chunk_size = ARENA_DEFAULT_CHUNK_SIZE;
    }
    if (chunk_size > ARENA_MAX_CHUNK_SIZE) {
        chunk_size = ARENA_MAX_CHUNK_SIZE;
    }

    AIRY_LOG_INFO("arena: arena_create (chunk_size=%zu, max_chunks=%zu)", chunk_size, max_chunks);

    airy_arena_t *arena = (airy_arena_t *)AIRY_CALLOC(1, sizeof(airy_arena_t));
    if (!arena) {
        AIRY_LOG_ERROR("arena: arena_create failed to alloc arena struct");
        return NULL;
    }

    arena->chunk_size = chunk_size;
    arena->max_chunks = max_chunks;
    arena->thread_safe = true;

    arena->first_chunk = chunk_create(chunk_size);
    if (!arena->first_chunk) {
        AIRY_LOG_ERROR("arena: arena_create failed to create first chunk");
        AIRY_FREE(arena);
        return NULL;
    }
    arena->current = arena->first_chunk;
    arena->num_chunks = 1;

    airy_mtx_init(&arena->lock);

    AIRY_LOG_INFO("arena: arena_create ok (chunk_size=%zu, arena=%p)", chunk_size, (void *)arena);
    return arena;
}

void arena_destroy(airy_arena_t *arena)
{
    if (!arena)
        return;

    AIRY_LOG_INFO("arena: arena_destroy (arena=%p, chunks=%zu, total_alloc=%zu, allocs=%" PRIu64 ")",
             (void *)arena, arena->num_chunks, arena->total_allocated, arena->alloc_count);

    airy_mtx_destroy(&arena->lock);
    chunk_destroy(arena->first_chunk);
    AIRY_FREE(arena);

    AIRY_LOG_DEBUG("arena: arena_destroy done");
}

void *arena_alloc(airy_arena_t *arena, size_t size)
{
    if (!arena || size == 0)
        return NULL;

    size_t aligned = (size + ARENA_ALIGNMENT - 1) & ~((size_t)ARENA_ALIGNMENT - 1);

    airy_mtx_lock(&arena->lock);

    if (aligned > arena->chunk_size / 2) {
        arena->fallback_count++;
        arena->total_allocated += aligned;
        arena->alloc_count++;
        airy_mtx_unlock(&arena->lock);
        void *ptr = AIRY_MALLOC(aligned);
        AIRY_LOG_DEBUG("arena: arena_alloc FALLBACK (size=%zu, aligned=%zu, ptr=%p, fallback#=%" PRIu64
                  ")",
                  size, aligned, ptr, arena->fallback_count);
        return ptr;
    }

    if (arena->current->bump + aligned > arena->current->end) {

        if (arena->max_chunks > 0 && arena->num_chunks >= arena->max_chunks) {
            airy_mtx_unlock(&arena->lock);
            AIRY_LOG_WARN("arena: arena_alloc OOM (size=%zu, aligned=%zu, chunks=%zu/%zu)", size,
                     aligned, arena->num_chunks, arena->max_chunks);
            return NULL;
        }

        size_t new_size = arena->chunk_size;
        if (arena->num_chunks > 0 && new_size < ARENA_MAX_CHUNK_SIZE) {
            new_size = arena->chunk_size * (1 << (arena->num_chunks > 4 ? 4 : arena->num_chunks));
            if (new_size > ARENA_MAX_CHUNK_SIZE)
                new_size = ARENA_MAX_CHUNK_SIZE;
        }

        AIRY_LOG_INFO("arena: arena_alloc NEW_CHUNK (chunk#=%zu→%zu, new_size=%zu, aligned=%zu)",
                 arena->num_chunks, arena->num_chunks + 1, new_size, aligned);

        arena_chunk_t *new_chunk = chunk_create(new_size);
        if (!new_chunk) {
            airy_mtx_unlock(&arena->lock);
            AIRY_LOG_ERROR("arena: arena_alloc failed to create new chunk (size=%zu)", new_size);
            return NULL;
        }

        arena->current->next = new_chunk;
        arena->current = new_chunk;
        arena->num_chunks++;
    }

    void *ptr = arena->current->bump;
    arena->current->bump += aligned;

    arena->total_allocated += aligned;
    arena->alloc_count++;

    airy_mtx_unlock(&arena->lock);

    AIRY_LOG_DEBUG("arena: arena_alloc ok (size=%zu, aligned=%zu, ptr=%p, chunk#=%zu, alloc#=%" PRIu64
              ")",
              size, aligned, ptr, arena->num_chunks, arena->alloc_count);
    return ptr;
}

void *arena_calloc(airy_arena_t *arena, size_t size)
{
    void *ptr = arena_alloc(arena, size);
    if (ptr) {
        AIRY_MEMSET(ptr, 0, size); /* BAN-154 */
    }
    return ptr;
}

void arena_reset(airy_arena_t *arena)
{
    if (!arena)
        return;

    AIRY_LOG_INFO("arena: arena_reset (arena=%p, chunks=%zu, reset#=%" PRIu64 "→%" PRIu64 ")",
             (void *)arena, arena->num_chunks, arena->reset_count, arena->reset_count + 1);

    airy_mtx_lock(&arena->lock);

    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        c->bump = c->start;

        AIRY_MEMSET(c->start, 0, c->size); /* BAN-154 */
    }

    arena->current = arena->first_chunk;

    arena->reset_count++;

    airy_mtx_unlock(&arena->lock);

    AIRY_LOG_DEBUG("arena: arena_reset done");
}

void arena_mark(airy_arena_t *arena, arena_mark_t *mark)
{
    if (!arena || !mark)
        return;

    airy_mtx_lock(&arena->lock);

    mark->arena = arena;
    mark->bump = arena->current->bump;
    mark->chunk = (airy_arena_t *)arena->current;

    airy_mtx_unlock(&arena->lock);

    AIRY_LOG_DEBUG("arena: arena_mark (arena=%p, bump=%p, chunk=%p)", (void *)arena, (void *)mark->bump,
              (void *)mark->chunk);
}

void arena_release(arena_mark_t *mark)
{
    if (!mark || !mark->arena || !mark->chunk)
        return;

    airy_arena_t *arena = mark->arena;
    arena_chunk_t *target = (arena_chunk_t *)mark->chunk;

    AIRY_LOG_INFO("arena: arena_release (arena=%p, target_chunk=%p, bump=%p)", (void *)arena,
             (void *)target, mark->bump);

    airy_mtx_lock(&arena->lock);

    arena_chunk_t *c = arena->first_chunk;
    while (c && c != target) {
        c->bump = c->start;
        AIRY_MEMSET(c->start, 0, c->size); /* BAN-154 */
        c = c->next;
    }

    if (c == target) {
        c->bump = (uint8_t *)mark->bump;
        if (c->bump < c->start)
            c->bump = c->start;

        if (c->bump < c->end) {
            AIRY_MEMSET(c->bump, 0, (size_t)(c->end - c->bump)); /* BAN-154 */
        }

        for (arena_chunk_t *nc = c->next; nc; nc = nc->next) {
            nc->bump = nc->start;
            AIRY_MEMSET(nc->start, 0, nc->size);
        }
    }

    arena->current = target;

    airy_mtx_unlock(&arena->lock);

    AIRY_LOG_DEBUG("arena: arena_release done");
}

bool arena_get_stats(airy_arena_t *arena, arena_stats_t *stats)
{
    if (!arena || !stats)
        return false;

    airy_mtx_lock(&arena->lock);

    stats->total_allocated = arena->total_allocated;
    stats->alloc_count = arena->alloc_count;
    stats->reset_count = arena->reset_count;
    stats->fallback_count = arena->fallback_count;
    stats->chunk_count = arena->num_chunks;

    stats->current_used = 0;
    stats->total_chunk_bytes = 0;
    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        stats->current_used += (size_t)(c->bump - c->start);
        stats->total_chunk_bytes += c->size;
    }

    airy_mtx_unlock(&arena->lock);
    return true;
}

bool arena_contains(airy_arena_t *arena, const void *ptr)
{
    if (!arena || !ptr)
        return false;

    airy_mtx_lock(&arena->lock);

    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        if ((const uint8_t *)ptr >= c->start && (const uint8_t *)ptr < c->end) {
            airy_mtx_unlock(&arena->lock);
            return true;
        }
    }

    airy_mtx_unlock(&arena->lock);
    return false;
}

size_t arena_capacity(airy_arena_t *arena)
{
    if (!arena)
        return 0;

    airy_mtx_lock(&arena->lock);
    size_t cap = 0;
    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        cap += c->size;
    }
    airy_mtx_unlock(&arena->lock);
    return cap;
}

size_t arena_used(airy_arena_t *arena)
{
    if (!arena)
        return 0;

    airy_mtx_lock(&arena->lock);
    size_t used = 0;
    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        used += (size_t)(c->bump - c->start);
    }
    airy_mtx_unlock(&arena->lock);
    return used;
}

static _Thread_local airy_arena_t *g_tls_arena = NULL;

airy_arena_t *airy_arena_get_current(void)
{
    return g_tls_arena;
}

void airy_arena_set_current(airy_arena_t *arena)
{
    g_tls_arena = arena;
}