// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cache_common.c
 * @brief 通用缓存库实现
 */

#include "cache_common.h"

#include "../memory/include/memory_common.h"
#include "../sync/include/sync_common.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "error.h"

#define HASH_SIZE 1024

/**
 * @brief 缓存条目结构体
 */
typedef struct cache_entry {
    void *key;
    void *value;
    time_t timestamp;
    struct cache_entry *prev;
    struct cache_entry *next;
    struct cache_entry *hnext;
} cache_entry_t;

/**
 * @brief 哈希桶结构体
 */
typedef struct cache_bucket {
    cache_entry_t *head;
    sync_mutex_t lock;
} cache_bucket_t;

/**
 * @brief 缓存实现结构体
 */
typedef struct cache_impl {
    cache_bucket_t buckets[HASH_SIZE];
    cache_entry_t *lru_head;
    cache_entry_t *lru_tail;
    size_t capacity;
    size_t size;
    int ttl_sec;
    sync_mutex_t lru_lock;
    cache_config_t manager;
} cache_impl_t;

/**
 * @brief 创建默认缓存配置
 */
cache_config_t cache_create_default_config(void)
{
    cache_config_t manager = {.capacity = 1000,
                              .ttl_sec = 3600,
                              .hash_func = cache_string_hash,
                              .compare_func = cache_string_compare,
                              .key_free_func = cache_string_free,
                              .value_free_func = cache_string_free,
                              .key_copy_func = cache_string_copy,
                              .value_copy_func = cache_string_copy};
    return manager;
}

/**
 * @brief 字符串键默认哈希函数
 */
unsigned int cache_string_hash(const void *key)
{
    const char *str = (const char *)key;
    unsigned int h = 5381;
    while (*str) {
        h = (h << 5) + h + *str++;
    }
    return h % HASH_SIZE;
}

/**
 * @brief 字符串键默认比较函数
 */
int cache_string_compare(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/**
 * @brief 字符串默认复制函数
 */
void *cache_string_copy(const void *data)
{
    if (!data) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    return memory_safe_strdup((const char *)data);
}

/**
 * @brief 字符串默认释放函数
 */
void cache_string_free(void *data)
{
    if (data) {
        memory_safe_free(data);
    }
}

/**
 * @brief 创建缓存条目
 */
static cache_entry_t *cache_entry_create(const cache_config_t *manager, const void *key,
                                         const void *value)
{
    cache_entry_t *entry = memory_safe_alloc(sizeof(cache_entry_t));
    if (!entry) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    entry->key = manager->key_copy_func(key);
    if (!entry->key) {
        memory_safe_free(entry);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    entry->value = manager->value_copy_func(value);
    if (!entry->value) {
        manager->key_free_func(entry->key);
        memory_safe_free(entry);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    entry->timestamp = time(NULL);
    entry->prev = entry->next = entry->hnext = NULL;

    return entry;
}

/**
 * @brief 释放缓存条目
 */
static void cache_entry_free(const cache_config_t *manager, cache_entry_t *entry)
{
    if (!entry) {
        return;
    }

    manager->key_free_func(entry->key);
    manager->value_free_func(entry->value);
    memory_safe_free(entry);
}

/**
 * @brief 从 LRU 链表中移除条目
 */
static void lru_remove(cache_impl_t *cache, cache_entry_t *entry)
{
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (cache->lru_head == entry) {
        cache->lru_head = entry->next;
    }
    if (cache->lru_tail == entry) {
        cache->lru_tail = entry->prev;
    }
    entry->prev = entry->next = NULL;
}

/**
 * @brief 将条目移到 LRU 链表头部
 */
static void lru_move_to_head(cache_impl_t *cache, cache_entry_t *entry)
{
    if (cache->lru_head == entry) {
        return;
    }

    lru_remove(cache, entry);

    entry->next = cache->lru_head;
    if (cache->lru_head) {
        cache->lru_head->prev = entry;
    }
    cache->lru_head = entry;
    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

/**
 * @brief 驱逐 LRU 条目
 */
static void evict_lru(cache_impl_t *cache)
{
    if (!cache->lru_tail) {
        return;
    }

    cache_entry_t *victim = cache->lru_tail;
    unsigned int idx = cache->manager.hash_func(victim->key);

    sync_mutex_lock(&cache->buckets[idx].lock);

    cache_entry_t **p = &cache->buckets[idx].head;
    while (*p) {
        if (*p == victim) {
            *p = victim->hnext;
            break;
        }
        p = &(*p)->hnext;
    }

    sync_mutex_unlock(&cache->buckets[idx].lock);

    lru_remove(cache, victim);
    cache_entry_free(&cache->manager, victim);
    cache->size--;
}

/**
 * @brief 创建缓存
 */
cache_t cache_create(const cache_config_t *manager)
{
    cache_impl_t *cache = memory_safe_alloc(sizeof(cache_impl_t));
    if (!cache) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    __builtin_memset(cache, 0, sizeof(cache_impl_t));

    if (manager) {
        cache->manager = *manager;
    } else {
        cache->manager = cache_create_default_config();
    }

    cache->capacity = cache->manager.capacity;
    cache->ttl_sec = cache->manager.ttl_sec;

    sync_mutex_init(&cache->lru_lock);
    for (int i = 0; i < HASH_SIZE; i++) {
        sync_mutex_init(&cache->buckets[i].lock);
    }

    return (cache_t)cache;
}

/**
 * @brief 销毁缓存
 */
void cache_destroy(cache_t cache)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    for (int i = 0; i < HASH_SIZE; i++) {
        sync_mutex_lock(&impl->buckets[i].lock);

        cache_entry_t *entry = impl->buckets[i].head;
        while (entry) {
            cache_entry_t *next = entry->hnext;
            cache_entry_free(&impl->manager, entry);
            entry = next;
        }

        impl->buckets[i].head = NULL;
        sync_mutex_unlock(&impl->buckets[i].lock);
        sync_mutex_destroy(&impl->buckets[i].lock);
    }

    sync_mutex_destroy(&impl->lru_lock);
    memory_safe_free(impl);
}

/**
 * @brief 从缓存获取值
 */
int cache_get(cache_t cache, const void *key, void **out_value)
{
    if (!cache || !key || !out_value) {
        return AIRY_EINVAL;
    }

    *out_value = NULL;
    cache_impl_t *impl = (cache_impl_t *)cache;

    unsigned int idx = impl->manager.hash_func(key);

    sync_mutex_lock(&impl->buckets[idx].lock);

    cache_entry_t *entry = impl->buckets[idx].head;
    while (entry) {
        if (impl->manager.compare_func(entry->key, key) == 0) {
            break;
        }
        entry = entry->hnext;
    }

    if (!entry) {
        sync_mutex_unlock(&impl->buckets[idx].lock);
        return 0;
    }

    /*
     * 检查过期时间：必须持有 bucket lock，否则 entry 可能在释放锁后被其他线程释放，
     * 导致后续访问 use-after-free。不能调用 cache_delete()，因为它内部调用 cache_put()
     * 会尝试重新获取 bucket lock 造成自死锁；改为直接从哈希桶链表摘除。
     */
    if (impl->ttl_sec > 0 && (time(NULL) - entry->timestamp) > impl->ttl_sec) {
        cache_entry_t **p = &impl->buckets[idx].head;
        while (*p) {
            if (*p == entry) {
                *p = entry->hnext;
                break;
            }
            p = &(*p)->hnext;
        }

        sync_mutex_unlock(&impl->buckets[idx].lock);

        sync_mutex_lock(&impl->lru_lock);
        lru_remove(impl, entry);
        impl->size--;
        sync_mutex_unlock(&impl->lru_lock);

        cache_entry_free(&impl->manager, entry);
        return 0;
    }

    /*
     * 复制值：在持有 bucket lock 的前提下完成，确保 entry 在此期间不会被驱逐。
     * value_copy_func 可能分配内存，但不会获取 bucket lock，故不会自死锁。
     */
    *out_value = impl->manager.value_copy_func(entry->value);

    /*
     * 更新 LRU 位置：使用 trylock 而非阻塞锁。evict_lru()（由 cache_put 调用）的
     * 锁顺序为 lru_lock → bucket_lock；若此处阻塞等待 lru_lock，而持有 lru_lock 的
     * 线程又等待 bucket_lock，将形成 AB-BA 死锁。LRU 仅是访问热度优化，trylock 失败
     * 时跳过不影响正确性。
     */
    if (sync_mutex_trylock(&impl->lru_lock) == 0) {
        lru_move_to_head(impl, entry);
        sync_mutex_unlock(&impl->lru_lock);
    }

    sync_mutex_unlock(&impl->buckets[idx].lock);
    return 1;
}

/**
 * @brief 向缓存存入值
 */
void cache_put(cache_t cache, const void *key, const void *value)
{
    if (!cache || !key) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;
    if (impl->capacity == 0) {
        return;
    }

    unsigned int idx = impl->manager.hash_func(key);

    sync_mutex_lock(&impl->buckets[idx].lock);

    cache_entry_t **p = &impl->buckets[idx].head;
    while (*p) {
        if (impl->manager.compare_func((*p)->key, key) == 0) {
            cache_entry_t *entry = *p;
            *p = entry->hnext;

            sync_mutex_unlock(&impl->buckets[idx].lock);

            sync_mutex_lock(&impl->lru_lock);
            lru_remove(impl, entry);
            impl->size--;
            sync_mutex_unlock(&impl->lru_lock);

            cache_entry_free(&impl->manager, entry);

            sync_mutex_lock(&impl->buckets[idx].lock);
            break;
        }
        p = &(*p)->hnext;
    }

    if (!value) {
        sync_mutex_unlock(&impl->buckets[idx].lock);
        return;
    }

    cache_entry_t *entry = cache_entry_create(&impl->manager, key, value);
    if (!entry) {
        sync_mutex_unlock(&impl->buckets[idx].lock);
        return;
    }

    entry->hnext = impl->buckets[idx].head;
    impl->buckets[idx].head = entry;

    sync_mutex_unlock(&impl->buckets[idx].lock);

    sync_mutex_lock(&impl->lru_lock);
    entry->next = impl->lru_head;
    if (impl->lru_head) {
        impl->lru_head->prev = entry;
    }
    impl->lru_head = entry;
    if (!impl->lru_tail) {
        impl->lru_tail = entry;
    }
    impl->size++;
    sync_mutex_unlock(&impl->lru_lock);

    if (impl->size > impl->capacity) {
        sync_mutex_lock(&impl->lru_lock);
        evict_lru(impl);
        sync_mutex_unlock(&impl->lru_lock);
    }
}

/**
 * @brief 从缓存删除值
 */
void cache_delete(cache_t cache, const void *key)
{
    if (!cache || !key) {
        return;
    }

    cache_put(cache, key, NULL);
}

/**
 * @brief 清空缓存
 */
void cache_clear(cache_t cache)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    for (int i = 0; i < HASH_SIZE; i++) {
        sync_mutex_lock(&impl->buckets[i].lock);

        cache_entry_t *entry = impl->buckets[i].head;
        while (entry) {
            cache_entry_t *next = entry->hnext;
            cache_entry_free(&impl->manager, entry);
            entry = next;
        }

        impl->buckets[i].head = NULL;
        sync_mutex_unlock(&impl->buckets[i].lock);
    }

    sync_mutex_lock(&impl->lru_lock);
    impl->lru_head = impl->lru_tail = NULL;
    impl->size = 0;
    sync_mutex_unlock(&impl->lru_lock);
}

/**
 * @brief 获取缓存大小
 */
size_t cache_get_size(cache_t cache)
{
    if (!cache) {
        return 0;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;
    return impl->size;
}

/**
 * @brief 获取缓存容量
 */
size_t cache_get_capacity(cache_t cache)
{
    if (!cache) {
        return 0;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;
    return impl->capacity;
}

/**
 * @brief 设置缓存容量
 */
void cache_set_capacity(cache_t cache, size_t capacity)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;
    impl->capacity = capacity;

    sync_mutex_lock(&impl->lru_lock);
    while (impl->size > impl->capacity) {
        evict_lru(impl);
    }
    sync_mutex_unlock(&impl->lru_lock);
}

/**
 * @brief 获取缓存过期时间
 */
int cache_get_ttl(cache_t cache)
{
    if (!cache) {
        return 0;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;
    return impl->ttl_sec;
}

/**
 * @brief 设置缓存过期时间
 */
void cache_set_ttl(cache_t cache, int ttl_sec)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;
    impl->ttl_sec = ttl_sec;
}

/**
 * @brief 创建字符串缓存
 */
cache_t cache_create_string_cache(size_t capacity, int ttl_sec)
{
    cache_config_t manager = cache_create_default_config();
    manager.capacity = capacity;
    manager.ttl_sec = ttl_sec;

    return cache_create(&manager);
}

/**
 * @brief 从字符串缓存获取值
 */
int cache_get_string(cache_t cache, const char *key, char **out_value)
{
    return cache_get(cache, key, (void **)out_value);
}

/**
 * @brief 向字符串缓存存入值
 */
void cache_put_string(cache_t cache, const char *key, const char *value)
{
    cache_put(cache, key, value);
}
