// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cache_common.c
 * @brief cache_common（统一 LRU 缓存）单元测试
 *
 * 覆盖：生命周期 / put-get / 未命中 / clear / size / stats 计数 /
 * LRU 淘汰 / TTL 过期（B16-S6 表二 #1：自 llm_d test_cache.c 迁移）。
 */

#include "cache_common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void test_cache_create_destroy(void)
{
    printf("  test_cache_create_destroy...\n");

    cache_t cache = cache_create_string_cache(100, 3600);
    assert(cache != NULL);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_put_get(void)
{
    printf("  test_cache_put_get...\n");

    cache_t cache = cache_create_string_cache(100, 3600);
    assert(cache != NULL);

    const char *key = "test_key_123";
    const char *value = "test_response_content";

    cache_put_string(cache, key, value);

    char *retrieved = NULL;
    int ret = cache_get_string(cache, key, &retrieved);
    assert(ret == 1);
    assert(retrieved != NULL);
    assert(strcmp(retrieved, value) == 0);

    cache_stats_t st;
    cache_get_stats(cache, &st);
    assert(st.hits == 1);
    assert(st.misses == 0);
    assert(st.hit_rate == 1.0);

    free(retrieved);
    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_miss(void)
{
    printf("  test_cache_miss...\n");

    cache_t cache = cache_create_string_cache(100, 3600);
    assert(cache != NULL);

    char *retrieved = NULL;
    int ret = cache_get_string(cache, "nonexistent_key", &retrieved);
    assert(ret == 0);
    assert(retrieved == NULL);

    cache_stats_t st;
    cache_get_stats(cache, &st);
    assert(st.hits == 0);
    assert(st.misses == 1);
    assert(st.hit_rate == 0.0);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_clear(void)
{
    printf("  test_cache_clear...\n");

    cache_t cache = cache_create_string_cache(100, 3600);
    assert(cache != NULL);

    cache_put_string(cache, "key1", "value1");
    cache_put_string(cache, "key2", "value2");
    cache_put_string(cache, "key3", "value3");

    cache_clear(cache);

    char *retrieved = NULL;
    assert(cache_get_string(cache, "key1", &retrieved) == 0);
    assert(retrieved == NULL);
    assert(cache_get_string(cache, "key2", &retrieved) == 0);
    assert(retrieved == NULL);
    assert(cache_get_string(cache, "key3", &retrieved) == 0);
    assert(retrieved == NULL);

    cache_stats_t st;
    cache_get_stats(cache, &st);
    assert(st.entries == 0);
    assert(st.hits == 0);
    assert(st.misses == 3);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_size(void)
{
    printf("  test_cache_size...\n");

    cache_t cache = cache_create_string_cache(100, 3600);
    assert(cache != NULL);
    assert(cache_get_capacity(cache) == 100);

    cache_put_string(cache, "key1", "value1");
    cache_put_string(cache, "key2", "value2");

    assert(cache_get_size(cache) == 2);

    cache_stats_t st;
    cache_get_stats(cache, &st);
    assert(st.entries == 2);
    assert(st.capacity == 100);
    assert(st.evictions == 0);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_stats(void)
{
    printf("  test_cache_stats...\n");

    cache_t cache = cache_create_string_cache(100, 3600);
    assert(cache != NULL);

    cache_put_string(cache, "hit1", "value1");
    cache_put_string(cache, "hit2", "value2");

    char *retrieved = NULL;
    assert(cache_get_string(cache, "hit1", &retrieved) == 1);
    free(retrieved);
    assert(cache_get_string(cache, "hit2", &retrieved) == 1);
    free(retrieved);
    assert(cache_get_string(cache, "miss1", &retrieved) == 0);
    assert(cache_get_string(cache, "miss2", &retrieved) == 0);

    cache_stats_t st;
    cache_get_stats(cache, &st);
    assert(st.hits == 2);
    assert(st.misses == 2);
    assert(st.hit_rate == 0.5);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_eviction(void)
{
    printf("  test_cache_eviction...\n");

    cache_t cache = cache_create_string_cache(2, 3600);
    assert(cache != NULL);

    cache_put_string(cache, "key1", "value1");
    cache_put_string(cache, "key2", "value2");
    cache_put_string(cache, "key3", "value3");

    assert(cache_get_size(cache) == 2);

    cache_stats_t st;
    cache_get_stats(cache, &st);
    assert(st.entries == 2);
    assert(st.capacity == 2);
    assert(st.evictions == 1);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_ttl(void)
{
    printf("  test_cache_ttl...\n");

    cache_t cache = cache_create_string_cache(100, 1);
    assert(cache != NULL);

    const char *key = "ttl_test_key";
    const char *value = "ttl_test_value";

    cache_put_string(cache, key, value);

    char *retrieved = NULL;
    int ret = cache_get_string(cache, key, &retrieved);
    (void)ret; /* suppress unused warning when -Werror */
    if (retrieved != NULL) {
        free(retrieved);
        retrieved = NULL;
    }

    /* 不能依赖单次 sleep(2)——被信号中断时 sleep() 提前返回（返回剩余
     * 秒数），偶发出现"TTL 未过期"假失败。改为按墙钟推进，循环等到
     * time(NULL) 至少前进 2 秒，消除 EINTR 抖动。 */
    {
        time_t start = time(NULL);
        while (time(NULL) - start < 2) {
#ifdef _WIN32
            Sleep(50);
#else
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000}; /* 50ms */
            nanosleep(&ts, NULL);
#endif
        }
    }

    retrieved = NULL;
    ret = cache_get_string(cache, key, &retrieved);
    assert(ret == 0);
    assert(retrieved == NULL);

    cache_stats_t st;
    cache_get_stats(cache, &st);
    assert(st.hits == 1);
    assert(st.misses == 1);
    assert(st.entries == 0);

    cache_destroy(cache);

    printf("    PASSED\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  Cache Common Unit Tests\n");
    printf("=========================================\n");

    test_cache_create_destroy();
    test_cache_put_get();
    test_cache_miss();
    test_cache_clear();
    test_cache_size();
    test_cache_stats();
    test_cache_eviction();
    test_cache_ttl();

    printf("\nAll cache_common tests PASSED\n");
    return 0;
}
