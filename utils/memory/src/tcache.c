// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tcache.c
 * @brief P1.20: per-Thread 缓存层实现 — 减少 pool.c 全局锁竞争
 *
 * 每个线程维护本地缓存（tcache），批量从全局内存池获取/归还内存块，
 * 减少对 pool.c 全局 mutex 的锁竞争。
 *
 * 设计：
 *   - 每个线程独立缓存，无需锁（单线程访问）
 *   - 批量获取（batch_fill）：一次锁操作获取多个块
 *   - 批量归还（batch_flush）：缓存满时一次归还多个块
 *   - 限流上限：每个 tcache 最多缓存 TCACHE_MAX_CACHED 个块
 *
 * 性能目标：单线程分配延迟降低 > 30%
 *
 */

#include "tcache.h"
#include "logging.h"
#include "airy_memory.h"

#include <inttypes.h>
#include <string.h>

/* ============================================================================
 * 内部数据结构
 * ============================================================================
 */

/**
 * @brief tcache 缓存槽（单向链表节点）
 *
 * 使用单向链表管理缓存的空闲块，分配时从链表头部取出（LIFO），
 * 释放时插入到链表头部，最大化 CPU 缓存局部性。
 */
typedef struct tcache_slot {
    struct tcache_slot *next;
} tcache_slot_t;

/**
 * @brief tcache 内部结构
 */
struct airy_tcache {
    memory_pool_t *pool;
    tcache_slot_t *head;
    size_t cached_count;
    size_t max_cached;
    size_t batch_size;

    uint64_t alloc_count;
    uint64_t free_count;
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t batch_fill_count;
    uint64_t batch_flush_count;
    uint64_t bypass_count;
};


airy_tcache_t *tcache_create(memory_pool_t *pool, size_t batch_size, size_t max_cached)
{
    if (!pool)
        return NULL;

    if (batch_size == 0) {
        batch_size = TCACHE_DEFAULT_BATCH_SIZE;
    }
    if (max_cached == 0) {
        max_cached = TCACHE_MAX_CACHED;
    }
    if (batch_size > max_cached) {
        batch_size = max_cached;
    }

    LOG_INFO("tcache: tcache_create (batch_size=%zu, max_cached=%zu)", batch_size, max_cached);

    airy_tcache_t *tc = (airy_tcache_t *)AIRY_CALLOC(1, sizeof(airy_tcache_t));
    if (!tc) {
        LOG_ERROR("tcache: tcache_create failed to alloc tcache struct");
        return NULL;
    }

    tc->pool = pool;
    tc->head = NULL;
    tc->cached_count = 0;
    tc->max_cached = max_cached;
    tc->batch_size = batch_size;

    size_t filled = tcache_batch_fill(tc);
    LOG_INFO("tcache: tcache_create ok (tc=%p, pre_filled=%zu, cached=%zu)", (void *)tc, filled,
             tc->cached_count);

    return tc;
}

void tcache_destroy(airy_tcache_t *tc)
{
    if (!tc)
        return;

    LOG_INFO("tcache: tcache_destroy (tc=%p, cached=%zu, allocs=%" PRIu64 ", hits=%" PRIu64
             ", miss=%" PRIu64 ", fill=%" PRIu64 ", flush=%" PRIu64 ", bypass=%" PRIu64 ")",
             (void *)tc, tc->cached_count, tc->alloc_count, tc->hit_count, tc->miss_count,
             tc->batch_fill_count, tc->batch_flush_count, tc->bypass_count);

    tcache_flush_all(tc);

    AIRY_FREE(tc);

    LOG_DEBUG("tcache: tcache_destroy done");
}


void *tcache_alloc(airy_tcache_t *tc)
{
    if (!tc)
        return NULL;

    tc->alloc_count++;

    if (tc->head) {
        tcache_slot_t *slot = tc->head;
        tc->head = slot->next;
        tc->cached_count--;
        tc->hit_count++;
        LOG_DEBUG("tcache: tcache_alloc HIT (tc=%p, ptr=%p, cached=%zu/%zu, alloc#=%" PRIu64 ")",
                  (void *)tc, (void *)slot, tc->cached_count, tc->max_cached, tc->alloc_count);
        return (void *)slot;
    }

    tc->miss_count++;
    LOG_DEBUG("tcache: tcache_alloc MISS (tc=%p, cached=0, miss#=%" PRIu64 ")", (void *)tc,
              tc->miss_count);

    size_t filled = tcache_batch_fill(tc);
    if (filled == 0) {

        tc->bypass_count++;
        void *ptr = memory_pool_alloc(tc->pool);
        LOG_WARN("tcache: tcache_alloc BYPASS (tc=%p, ptr=%p, bypass#=%" PRIu64 ")", (void *)tc,
                 ptr, tc->bypass_count);
        return ptr;
    }

    tcache_slot_t *slot = tc->head;
    tc->head = slot->next;
    tc->cached_count--;
    tc->hit_count++;
    LOG_DEBUG("tcache: tcache_alloc FILLED (tc=%p, ptr=%p, filled=%zu, cached=%zu/%zu)", (void *)tc,
              (void *)slot, filled, tc->cached_count, tc->max_cached);
    return (void *)slot;
}

void tcache_free(airy_tcache_t *tc, void *ptr)
{
    if (!tc || !ptr)
        return;

    tc->free_count++;

    if (tc->cached_count >= tc->max_cached) {

        size_t flushed = tcache_batch_flush(tc);
        (void)flushed;
        LOG_DEBUG("tcache: tcache_free FLUSH (tc=%p, flushed=%zu, cached=%zu/%zu)", (void *)tc,
                  flushed, tc->cached_count, tc->max_cached);
    }

    tcache_slot_t *slot = (tcache_slot_t *)ptr;
    slot->next = tc->head;
    tc->head = slot;
    tc->cached_count++;

    LOG_DEBUG("tcache: tcache_free ok (tc=%p, ptr=%p, cached=%zu/%zu, free#=%" PRIu64 ")",
              (void *)tc, ptr, tc->cached_count, tc->max_cached, tc->free_count);
}


size_t tcache_batch_fill(airy_tcache_t *tc)
{
    if (!tc)
        return 0;

    size_t remaining = tc->max_cached - tc->cached_count;
    if (remaining == 0)
        return 0;

    size_t batch = (tc->batch_size < remaining) ? tc->batch_size : remaining;

    LOG_DEBUG("tcache: tcache_batch_fill START (tc=%p, batch=%zu, remaining=%zu)", (void *)tc,
              batch, remaining);

    void *blocks[TCACHE_DEFAULT_BATCH_SIZE > 64 ? TCACHE_DEFAULT_BATCH_SIZE : 64];
    size_t filled = memory_pool_batch_alloc(tc->pool, batch, blocks);

    for (size_t i = 0; i < filled; i++) {
        tcache_slot_t *slot = (tcache_slot_t *)blocks[i];
        slot->next = tc->head;
        tc->head = slot;
        tc->cached_count++;
    }

    if (filled > 0) {
        tc->batch_fill_count++;
    }

    LOG_DEBUG(
        "tcache: tcache_batch_fill DONE (tc=%p, filled=%zu/%zu, cached=%zu/%zu, fill#=%" PRIu64 ")",
        (void *)tc, filled, batch, tc->cached_count, tc->max_cached, tc->batch_fill_count);

    return filled;
}

size_t tcache_batch_flush(airy_tcache_t *tc)
{
    if (!tc)
        return 0;

    if (tc->cached_count <= TCACHE_FLUSH_THRESHOLD) {
        return 0;
    }

    size_t to_flush =
        tc->cached_count -
        (TCACHE_FLUSH_THRESHOLD > tc->max_cached / 2 ? tc->max_cached / 2 : TCACHE_FLUSH_THRESHOLD);
    if (to_flush == 0)
        return 0;

    if (to_flush > tc->batch_size) {
        to_flush = tc->batch_size;
    }

    LOG_DEBUG("tcache: tcache_batch_flush START (tc=%p, to_flush=%zu, cached=%zu/%zu)", (void *)tc,
              to_flush, tc->cached_count, tc->max_cached);

    void *blocks[TCACHE_DEFAULT_BATCH_SIZE > 64 ? TCACHE_DEFAULT_BATCH_SIZE : 64];
    size_t collected = 0;
    for (size_t i = 0; i < to_flush; i++) {
        if (!tc->head)
            break;
        tcache_slot_t *slot = tc->head;
        tc->head = slot->next;
        tc->cached_count--;
        blocks[collected++] = (void *)slot;
    }

    size_t flushed = 0;
    if (collected > 0) {
        flushed = memory_pool_batch_free(tc->pool, blocks, collected);
        tc->batch_flush_count++;
    }

    LOG_DEBUG("tcache: tcache_batch_flush DONE (tc=%p, flushed=%zu/%zu, cached=%zu, flush#=%" PRIu64
              ")",
              (void *)tc, flushed, to_flush, tc->cached_count, tc->batch_flush_count);

    return flushed;
}

void tcache_flush_all(airy_tcache_t *tc)
{
    if (!tc)
        return;

    while (tc->head) {
        tcache_slot_t *slot = tc->head;
        tc->head = slot->next;
        tc->cached_count--;

        memory_pool_free(tc->pool, (void *)slot);
    }
}


bool tcache_get_stats(airy_tcache_t *tc, tcache_stats_t *stats)
{
    if (!tc || !stats)
        return false;

    stats->alloc_count = tc->alloc_count;
    stats->free_count = tc->free_count;
    stats->hit_count = tc->hit_count;
    stats->miss_count = tc->miss_count;
    stats->batch_fill_count = tc->batch_fill_count;
    stats->batch_flush_count = tc->batch_flush_count;
    stats->bypass_count = tc->bypass_count;

    uint64_t total = tc->hit_count + tc->miss_count;
    if (total > 0) {
        stats->hit_rate = 100.0 * (double)tc->hit_count / (double)total;
    } else {
        stats->hit_rate = 0.0;
    }

    return true;
}

size_t tcache_cached_count(airy_tcache_t *tc)
{
    if (!tc)
        return 0;
    return tc->cached_count;
}

bool tcache_is_full(airy_tcache_t *tc)
{
    if (!tc)
        return false;
    return tc->cached_count >= tc->max_cached;
}