// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cache_common.c
 * @brief Generic cache library implementation.
 */

#include "cache_common.h"

#include "../memory/memory_common.h"
#include "../sync/sync.h"
#include "atomic_compat.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "error.h"

#define HASH_SIZE 1024

/**
 * @brief Cache entry structure.
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
 * @brief Cache implementation structure.
 *
 * 单一互斥锁守护下列全部字段。每个条目同时挂在哈希桶链与全局 LRU
 * 链表两个容器中，若分处两把锁分两阶段摘除，并发回收者会在窗口内
 * 释放仍被另一容器引用的条目（use-after-free / 双重释放）。用一把
 * 锁在同一临界区内摘除两个容器，才能保证两视图始终一致。
 */
typedef struct cache_impl {
    cache_entry_t *buckets[HASH_SIZE];
    cache_entry_t *lru_head;
    cache_entry_t *lru_tail;
    size_t capacity;
    size_t size;
    int ttl_sec;
    sync_mutex_t lock;
    atomic_uint64_t hits;
    atomic_uint64_t misses;
    atomic_uint64_t evictions;
    cache_config_t manager;
} cache_impl_t;

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

unsigned int cache_string_hash(const void *key)
{
    const char *str = (const char *)key;
    unsigned int h = 5381;
    while (*str) {
        h = (h << 5) + h + *str++;
    }
    return h % HASH_SIZE;
}

int cache_string_compare(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

void *cache_string_copy(const void *data)
{
    if (!data) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    return memory_safe_strdup((const char *)data);
}

void cache_string_free(void *data)
{
    if (data) {
        memory_safe_free(data);
    }
}

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

static void cache_entry_free(const cache_config_t *manager, cache_entry_t *entry)
{
    if (!entry) {
        return;
    }

    manager->key_free_func(entry->key);
    manager->value_free_func(entry->value);
    memory_safe_free(entry);
}

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

static cache_entry_t *bucket_find(cache_impl_t *cache, unsigned int idx, const void *key)
{
    for (cache_entry_t *entry = cache->buckets[idx]; entry; entry = entry->hnext) {
        if (cache->manager.compare_func(entry->key, key) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void bucket_unlink(cache_impl_t *cache, unsigned int idx, cache_entry_t *entry)
{
    cache_entry_t **link = &cache->buckets[idx];
    while (*link) {
        if (*link == entry) {
            *link = entry->hnext;
            return;
        }
        link = &(*link)->hnext;
    }
}

/**
 * @brief Detach an entry from both containers and free it.
 *
 * 调用者必须持有 cache->lock。在同一临界区内同时摘除哈希链与 LRU 链，
 * 是保证两个视图一致、杜绝释放仍被引用条目的关键。
 */
static void entry_reap(cache_impl_t *cache, unsigned int idx, cache_entry_t *entry)
{
    bucket_unlink(cache, idx, entry);
    lru_remove(cache, entry);
    cache_entry_free(&cache->manager, entry);
    cache->size--;
}

static void evict_lru(cache_impl_t *cache)
{
    if (!cache->lru_tail) {
        return;
    }

    cache_entry_t *victim = cache->lru_tail;
    unsigned int idx = cache->manager.hash_func(victim->key);

    entry_reap(cache, idx, victim);
    atomic_fetch_add(&cache->evictions, 1);
}

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

    if (sync_mutex_create(&cache->lock, NULL) != SYNC_SUCCESS) {
        memory_safe_free(cache);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "cache lock creation failed");
    }

    return (cache_t)cache;
}

/**
 * @brief Free every entry and empty both containers.
 *
 * 调用者须持有 cache->lock，或在无并发（destroy 路径）时独占调用。
 */
static void cache_drain(cache_impl_t *cache)
{
    for (unsigned int i = 0; i < HASH_SIZE; i++) {
        cache_entry_t *entry = cache->buckets[i];
        while (entry) {
            cache_entry_t *next = entry->hnext;
            cache_entry_free(&cache->manager, entry);
            entry = next;
        }
        cache->buckets[i] = NULL;
    }

    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->size = 0;
}

void cache_destroy(cache_t cache)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    sync_mutex_lock_ex(impl->lock, NULL);
    cache_drain(impl);
    sync_mutex_unlock_ex(impl->lock);

    sync_mutex_free(impl->lock);
    memory_safe_free(impl);
}

int cache_get(cache_t cache, const void *key, void **out_value)
{
    if (!cache || !key || !out_value) {
        return AIRY_EINVAL;
    }

    *out_value = NULL;
    cache_impl_t *impl = (cache_impl_t *)cache;

    unsigned int idx = impl->manager.hash_func(key);

    sync_mutex_lock_ex(impl->lock, NULL);

    cache_entry_t *entry = bucket_find(impl, idx, key);
    if (!entry) {
        sync_mutex_unlock_ex(impl->lock);
        atomic_fetch_add(&impl->misses, 1);
        return 0;
    }

    if (impl->ttl_sec > 0 && (time(NULL) - entry->timestamp) > impl->ttl_sec) {
        entry_reap(impl, idx, entry);
        sync_mutex_unlock_ex(impl->lock);
        atomic_fetch_add(&impl->misses, 1);
        return 0;
    }

    *out_value = impl->manager.value_copy_func(entry->value);
    lru_move_to_head(impl, entry);

    atomic_fetch_add(&impl->hits, 1);

    sync_mutex_unlock_ex(impl->lock);
    return 1;
}

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

    sync_mutex_lock_ex(impl->lock, NULL);

    cache_entry_t *existing = bucket_find(impl, idx, key);
    if (existing) {
        entry_reap(impl, idx, existing);
    }

    if (value) {
        cache_entry_t *entry = cache_entry_create(&impl->manager, key, value);
        if (entry) {
            entry->hnext = impl->buckets[idx];
            impl->buckets[idx] = entry;
            lru_move_to_head(impl, entry);
            impl->size++;
        }
    }

    while (impl->size > impl->capacity) {
        evict_lru(impl);
    }

    sync_mutex_unlock_ex(impl->lock);
}

void cache_delete(cache_t cache, const void *key)
{
    cache_put(cache, key, NULL);
}

/**
 * @brief Clear the cache.
 */
void cache_clear(cache_t cache)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    sync_mutex_lock_ex(impl->lock, NULL);
    cache_drain(impl);
    sync_mutex_unlock_ex(impl->lock);
}

/**
 * @brief Get the cache size.
 */
size_t cache_get_size(cache_t cache)
{
    if (!cache) {
        return 0;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    sync_mutex_lock_ex(impl->lock, NULL);
    size_t size = impl->size;
    sync_mutex_unlock_ex(impl->lock);

    return size;
}

size_t cache_get_capacity(cache_t cache)
{
    if (!cache) {
        return 0;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    sync_mutex_lock_ex(impl->lock, NULL);
    size_t capacity = impl->capacity;
    sync_mutex_unlock_ex(impl->lock);

    return capacity;
}

void cache_get_stats(cache_t cache, cache_stats_t *out)
{
    if (!cache || !out) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;
    uint64_t hits = (uint64_t)atomic_load(&impl->hits);
    uint64_t misses = (uint64_t)atomic_load(&impl->misses);

    sync_mutex_lock_ex(impl->lock, NULL);
    out->entries = impl->size;
    out->capacity = impl->capacity;
    sync_mutex_unlock_ex(impl->lock);

    out->hits = (size_t)hits;
    out->misses = (size_t)misses;
    out->evictions = (size_t)(uint64_t)atomic_load(&impl->evictions);
    out->hit_rate = (hits + misses) > 0 ? (double)hits / (double)(hits + misses) : 0.0;
}

void cache_set_capacity(cache_t cache, size_t capacity)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    sync_mutex_lock_ex(impl->lock, NULL);
    impl->capacity = capacity;
    while (impl->size > impl->capacity) {
        evict_lru(impl);
    }
    sync_mutex_unlock_ex(impl->lock);
}

int cache_get_ttl(cache_t cache)
{
    if (!cache) {
        return 0;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    sync_mutex_lock_ex(impl->lock, NULL);
    int ttl_sec = impl->ttl_sec;
    sync_mutex_unlock_ex(impl->lock);

    return ttl_sec;
}

void cache_set_ttl(cache_t cache, int ttl_sec)
{
    if (!cache) {
        return;
    }

    cache_impl_t *impl = (cache_impl_t *)cache;

    sync_mutex_lock_ex(impl->lock, NULL);
    impl->ttl_sec = ttl_sec;
    sync_mutex_unlock_ex(impl->lock);
}

cache_t cache_create_string_cache(size_t capacity, int ttl_sec)
{
    cache_config_t manager = cache_create_default_config();
    manager.capacity = capacity;
    manager.ttl_sec = ttl_sec;

    return cache_create(&manager);
}

int cache_get_string(cache_t cache, const char *key, char **out_value)
{
    return cache_get(cache, key, (void **)out_value);
}

void cache_put_string(cache_t cache, const char *key, const char *value)
{
    cache_put(cache, key, value);
}
