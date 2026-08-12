/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cache_common.h
 * @brief Common cache library.
 *
 * Provides generic cache implementations, including:
 * - LRU cache
 * - Hash-table cache
 * - Cache with expiration
 */

#ifndef AIRY_RT_CACHE_COMMON_H
#define AIRY_RT_CACHE_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cache key-value callback function types
 */
typedef void (*cache_free_func_t)(void *data);
typedef void *(*cache_copy_func_t)(const void *data);
typedef int (*cache_compare_func_t)(const void *a, const void *b);
typedef unsigned int (*cache_hash_func_t)(const void *key);

/**
 * @brief Cache configuration structure
 */
typedef struct {
    size_t capacity;
    int ttl_sec;
    cache_hash_func_t hash_func;
    cache_compare_func_t compare_func;
    cache_free_func_t key_free_func;
    cache_free_func_t value_free_func;
    cache_copy_func_t key_copy_func;
    cache_copy_func_t value_copy_func;
} cache_config_t;

/**
 * @brief Cache handle type
 */
typedef struct cache_impl *cache_t;

/**
 * @brief Create the default cache configuration
 * @return Default cache configuration
 */
cache_config_t cache_create_default_config(void);

/**
 * @brief Create a cache
 * @param manager Cache configuration
 * @return Cache handle
 */
cache_t cache_create(const cache_config_t *manager);

/**
 * @brief Destroy a cache
 * @param cache Cache handle
 */
void cache_destroy(cache_t cache);

/**
 * @brief Get a value from the cache
 * @param cache Cache handle
 * @param key Key
 * @param out_value Output value
 * @return 1 if found, 0 if not found, -1 on error
 */
int cache_get(cache_t cache, const void *key, void **out_value);

/**
 * @brief Store a value in the cache
 * @param cache Cache handle
 * @param key Key
 * @param value Value
 */
void cache_put(cache_t cache, const void *key, const void *value);

/**
 * @brief Delete a value from the cache
 * @param cache Cache handle
 * @param key Key
 */
void cache_delete(cache_t cache, const void *key);

/**
 * @brief Clear the cache
 * @param cache Cache handle
 */
void cache_clear(cache_t cache);

/**
 * @brief Get the cache size
 * @param cache Cache handle
 * @return Cache size
 */
size_t cache_get_size(cache_t cache);

/**
 * @brief Get the cache capacity
 * @param cache Cache handle
 * @return Cache capacity
 */
size_t cache_get_capacity(cache_t cache);

/**
 * @brief Set the cache capacity
 * @param cache Cache handle
 * @param capacity New capacity
 */
void cache_set_capacity(cache_t cache, size_t capacity);

/**
 * @brief Get the cache expiration time
 * @param cache Cache handle
 * @return Expiration time (seconds)
 */
int cache_get_ttl(cache_t cache);

/**
 * @brief Set the cache expiration time
 * @param cache Cache handle
 * @param ttl_sec Expiration time (seconds)
 */
void cache_set_ttl(cache_t cache, int ttl_sec);

/**
 * @brief Default hash function for string keys
 * @param key String key
 * @return Hash value
 */
unsigned int cache_string_hash(const void *key);

/**
 * @brief Default compare function for string keys
 * @param a String key 1
 * @param b String key 2
 * @return Comparison result
 */
int cache_string_compare(const void *a, const void *b);

/**
 * @brief Default copy function for strings
 * @param data String
 * @return Copied string
 */
void *cache_string_copy(const void *data);

/**
 * @brief Default free function for strings
 * @param data String
 */
void cache_string_free(void *data);

/**
 * @brief Create a string cache (using the default string handlers)
 * @param capacity Cache capacity
 * @param ttl_sec Expiration time (seconds)
 * @return Cache handle
 */
cache_t cache_create_string_cache(size_t capacity, int ttl_sec);

/**
 * @brief Get a value from a string cache
 * @param cache Cache handle
 * @param key String key
 * @param out_value Output string value
 * @return 1 if found, 0 if not found, -1 on error
 */
int cache_get_string(cache_t cache, const char *key, char **out_value);

/**
 * @brief Store a value in a string cache
 * @param cache Cache handle
 * @param key String key
 * @param value String value
 */
void cache_put_string(cache_t cache, const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_COMMON_H */
