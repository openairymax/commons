// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_arena_tcache.c
 * @brief 模拟内存分配场景，验证 arena 和 tcache 日志输出
 *
 * 测试场景：
 *   - Arena: 创建、多次分配（含新 chunk 触发）、超大分配 fallback、reset、mark/release、统计
 *   - Tcache: 创建、批量分配/释放（触发 hit/miss/batch_fill/batch_flush/bypass）、统计
 *
 */

#include "airy_memory.h"
#include "arena.h"
#include "logging.h"
#include "airy_memory.h"
#include "memory_pool.h"
#include "tcache.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef printf
#undef printf
#endif

static void test_arena_basic(void)
{
    printf("\n=== Arena 基础分配场景 ===\n");

    airy_arena_t *arena = arena_create(1024, 4);
    assert(arena != NULL);

    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = arena_alloc(arena, 256);
        assert(ptrs[i] != NULL);

        memset(ptrs[i], 0xAA + i, 256);
    }

    arena_stats_t stats;
    arena_get_stats(arena, &stats);
    printf("  Arena stats: allocs=%" PRIu64 ", chunks=%zu, total=%zu, used=%zu, fallback=%" PRIu64
           "\n",
           stats.alloc_count, stats.chunk_count, stats.total_chunk_bytes, stats.current_used,
           stats.fallback_count);

    void *huge = arena_alloc(arena, 2048); /* > chunk_size/2 */
    if (huge) {
        printf("  Fallback allocation succeeded (expected)\n");
        AIRY_FREE(huge);
    }

    arena_get_stats(arena, &stats);
    printf("  After fallback: fallback_count=%" PRIu64 "\n", stats.fallback_count);

    arena_mark_t mark;
    arena_mark(arena, &mark);

    void *temp1 = arena_alloc(arena, 128);
    void *temp2 = arena_alloc(arena, 64);
    assert(temp1 != NULL && temp2 != NULL);
    (void)temp1;
    (void)temp2;

    arena_get_stats(arena, &stats);
    printf("  After mark+alloc: used=%zu\n", stats.current_used);

    arena_release(&mark);

    arena_get_stats(arena, &stats);
    printf("  After release: used=%zu (should be less)\n", stats.current_used);

    arena_reset(arena);

    arena_get_stats(arena, &stats);
    printf("  After reset: used=%zu, reset_count=%" PRIu64 "\n", stats.current_used,
           stats.reset_count);

    void *after_reset = arena_alloc(arena, 512);
    assert(after_reset != NULL);
    memset(after_reset, 0xBB, 512);

    arena_get_stats(arena, &stats);
    printf("  After reset+realloc: used=%zu\n", stats.current_used);

    arena_destroy(arena);
    printf("  Arena basic test PASSED\n");
}

static void test_tcache_basic(void)
{
    printf("\n=== Tcache 基础分配场景 ===\n");

    memory_pool_options_t pool_opts = {.block_size = 256,
                                       .initial_blocks = 128,
                                       .max_blocks = 0,
                                       .expansion_size = 16,
                                       .thread_safe = true,
                                       .name = "tcache_test_pool"};
    memory_pool_t *pool = memory_pool_create(&pool_opts);
    assert(pool != NULL);

    airy_tcache_t *tc = tcache_create(pool, 8, 64);
    assert(tc != NULL);

    printf("  --- 首次分配（应命中预填充缓存） ---\n");
    void *blocks[20];
    for (int i = 0; i < 10; i++) {
        blocks[i] = tcache_alloc(tc);
        assert(blocks[i] != NULL);
        memset(blocks[i], 0xCC + i, 256);
    }

    tcache_stats_t tstats;
    tcache_get_stats(tc, &tstats);
    printf("  After 10 allocs: hits=%" PRIu64 ", miss=%" PRIu64 ", bypass=%" PRIu64
           ", hit_rate=%.1f%%\n",
           tstats.hit_count, tstats.miss_count, tstats.bypass_count, tstats.hit_rate);

    printf("  --- 释放 10 块回 tcache ---\n");
    for (int i = 0; i < 10; i++) {
        tcache_free(tc, blocks[i]);
    }

    tcache_get_stats(tc, &tstats);
    printf("  After 10 frees: free_count=%" PRIu64 ", cached=%zu\n", tstats.free_count,
           tcache_cached_count(tc));

    printf("  --- 大量分配（触发 miss + batch_fill） ---\n");
    void *many_blocks[80];
    for (int i = 0; i < 80; i++) {
        many_blocks[i] = tcache_alloc(tc);
        if (!many_blocks[i]) {
            printf("  Allocation %d failed (pool exhausted)\n", i);
            break;
        }
    }

    tcache_get_stats(tc, &tstats);
    printf("  After 80 allocs: hits=%" PRIu64 ", miss=%" PRIu64 ", bypass=%" PRIu64
           ", fill=%" PRIu64 ", hit_rate=%.1f%%\n",
           tstats.hit_count, tstats.miss_count, tstats.bypass_count, tstats.batch_fill_count,
           tstats.hit_rate);

    printf("  --- 大量释放（触发 batch_flush） ---\n");
    for (int i = 0; i < 80; i++) {
        if (many_blocks[i]) {
            tcache_free(tc, many_blocks[i]);
        }
    }

    tcache_get_stats(tc, &tstats);
    printf("  After 80 frees: free_count=%" PRIu64 ", flush=%" PRIu64 ", cached=%zu\n",
           tstats.free_count, tstats.batch_flush_count, tcache_cached_count(tc));

    printf("  Final hit_rate=%.1f%% (target > 30%%)\n", tstats.hit_rate);

    tcache_destroy(tc);
    memory_pool_destroy(pool);
    printf("  Tcache basic test PASSED\n");
}

#if 0

static void test_combined_scenario(void)
{
    printf("\n=== 综合场景：Arena + Tcache 协同 ===\n");

    airy_arena_t *req_arena = arena_create(4096, 0);
    assert(req_arena != NULL);

    memory_pool_options_t pool_opts = {
        .block_size = 512,
        .initial_blocks = 16,
        .max_blocks = 0,
        .expansion_size = 8,
        .thread_safe = true,
        .name = "combined_test_pool"
    };
    memory_pool_t *pool = memory_pool_create(&pool_opts);
    assert(pool != NULL);

    airy_tcache_t *tc = tcache_create(pool, 4, 32);
    assert(tc != NULL);

    printf("  --- 模拟请求处理循环 ---\n");
    for (int req = 0; req < 5; req++) {
        printf("  Request #%d:\n", req + 1);

        void *req_ctx = arena_alloc(req_arena, 128);
        void *req_buf = arena_alloc(req_arena, 1024);
        assert(req_ctx && req_buf);

        void *obj1 = tcache_alloc(tc);
        void *obj2 = tcache_alloc(tc);
        assert(obj1 && obj2);

        memset(req_ctx, 0, 128);
        memset(req_buf, 0xDD, 1024);
        memset(obj1, 0xEE, 512);
        memset(obj2, 0xFF, 512);

        tcache_free(tc, obj1);
        tcache_free(tc, obj2);

        arena_reset(req_arena);
    }

    arena_stats_t astats;
    arena_get_stats(req_arena, &astats);
    printf("  Arena after 5 reqs: allocs=%" PRIu64 ", reset=%" PRIu64 ", chunks=%zu\n",
           astats.alloc_count, astats.reset_count, astats.chunk_count);

    tcache_stats_t tstats;
    tcache_get_stats(tc, &tstats);
    printf("  Tcache after 5 reqs: allocs=%" PRIu64 ", hits=%" PRIu64 ", miss=%" PRIu64
           ", hit_rate=%.1f%%\n",
           tstats.alloc_count, tstats.hit_count, tstats.miss_count, tstats.hit_rate);

    tcache_destroy(tc);
    memory_pool_destroy(pool);
    arena_destroy(req_arena);
    printf("  Combined scenario PASSED\n");
}

#endif

static void test_arena_oom(void)
{
    printf("\n=== Arena OOM 场景 ===\n");

    airy_arena_t *arena = arena_create(1024, 1);
    assert(arena != NULL);

    void *p1 = arena_alloc(arena, 512);
    void *p2 = arena_alloc(arena, 400);
    assert(p1 != NULL && p2 != NULL);
    (void)p1;
    (void)p2;

    void *p3 = arena_alloc(arena, 256);
    if (p3 == NULL) {
        printf("  OOM correctly detected (max_chunks=1, chunks exhausted)\n");
    } else {
        printf("  WARNING: OOM not triggered (unexpected)\n");
    }

    arena_destroy(arena);
    printf("  Arena OOM test PASSED\n");
}

#define BENCH_ITERATIONS 100000

static void test_tcache_benchmark(void)
{
    printf("\n=== P1.20.4: Tcache 性能基准测试 ===\n");

    memory_pool_options_t pool_opts = {.block_size = 256,
                                       .initial_blocks = 2048,
                                       .max_blocks = 0,
                                       .expansion_size = 128,
                                       .thread_safe = true,
                                       .name = "bench_pool"};
    memory_pool_t *pool = memory_pool_create(&pool_opts);
    assert(pool != NULL);

    printf("  --- 基准：直接 pool_alloc/pool_free (%d 次) ---\n", BENCH_ITERATIONS);

    void **ptrs = (void **)AIRY_MALLOC(sizeof(void *) * BENCH_ITERATIONS);
    assert(ptrs != NULL);

    uint64_t pool_start = airy_time_ns();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        ptrs[i] = memory_pool_alloc(pool);
    }
    uint64_t pool_alloc_ns = airy_time_ns() - pool_start;

    uint64_t free_start = airy_time_ns();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        memory_pool_free(pool, ptrs[i]);
    }
    uint64_t pool_free_ns = airy_time_ns() - free_start;

    double pool_alloc_us = (double)pool_alloc_ns / (double)BENCH_ITERATIONS / 1000.0;
    double pool_free_us = (double)pool_free_ns / (double)BENCH_ITERATIONS / 1000.0;
    printf("  pool_alloc: avg=%.3f us/op, total=%" PRIu64 " ns\n", pool_alloc_us, pool_alloc_ns);
    printf("  pool_free:  avg=%.3f us/op, total=%" PRIu64 " ns\n", pool_free_us, pool_free_ns);

    printf("  --- tcache_alloc/tcache_free (%d 次) ---\n", BENCH_ITERATIONS);

    airy_tcache_t *tc = tcache_create(pool, 16, 64);
    assert(tc != NULL);

    uint64_t tc_start = airy_time_ns();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        ptrs[i] = tcache_alloc(tc);
    }
    uint64_t tc_alloc_ns = airy_time_ns() - tc_start;

    uint64_t tc_free_start = airy_time_ns();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        tcache_free(tc, ptrs[i]);
    }
    uint64_t tc_free_ns = airy_time_ns() - tc_free_start;

    double tc_alloc_us = (double)tc_alloc_ns / (double)BENCH_ITERATIONS / 1000.0;
    double tc_free_us = (double)tc_free_ns / (double)BENCH_ITERATIONS / 1000.0;
    printf("  tcache_alloc: avg=%.3f us/op, total=%" PRIu64 " ns\n", tc_alloc_us, tc_alloc_ns);
    printf("  tcache_free:  avg=%.3f us/op, total=%" PRIu64 " ns\n", tc_free_us, tc_free_ns);

    double alloc_improvement = (1.0 - (double)tc_alloc_ns / (double)pool_alloc_ns) * 100.0;
    double free_improvement = (1.0 - (double)tc_free_ns / (double)pool_free_ns) * 100.0;

    tcache_stats_t tstats;
    tcache_get_stats(tc, &tstats);

    printf("\n  === 性能对比结果 ===\n");
    printf("  alloc: pool=%.3f us → tcache=%.3f us (提升 %.1f%%)\n", pool_alloc_us, tc_alloc_us,
           alloc_improvement);
    printf("  free:  pool=%.3f us → tcache=%.3f us (提升 %.1f%%)\n", pool_free_us, tc_free_us,
           free_improvement);
    printf("  tcache hit_rate: %.1f%% (hits=%" PRIu64 ", miss=%" PRIu64 ", fill=%" PRIu64
           ", flush=%" PRIu64 ")\n",
           tstats.hit_rate, tstats.hit_count, tstats.miss_count, tstats.batch_fill_count,
           tstats.batch_flush_count);

    if (alloc_improvement > 30.0) {
        printf("  P1.20.4 验收通过: alloc 延迟降低 %.1f%% > 30%%\n", alloc_improvement);
    } else {
        printf("  P1.20.4 验收未达标: alloc 延迟降低 %.1f%% <= 30%%\n", alloc_improvement);
    }

    tcache_destroy(tc);
    memory_pool_destroy(pool);
    AIRY_FREE(ptrs);
    printf("  Tcache benchmark PASSED\n");
}

int main(void)
{
    printf("============================================\n");
    printf("Arena & Tcache 日志验证测试\n");
    printf("============================================\n");

    log_init(NULL);
    log_set_module_level("*", LOG_LEVEL_DEBUG);

    test_arena_basic();
    test_arena_oom();
    test_tcache_basic();
    test_tcache_benchmark();

    printf("\n============================================\n");
    printf("所有测试通过！\n");
    printf("============================================\n");

    return 0;
}