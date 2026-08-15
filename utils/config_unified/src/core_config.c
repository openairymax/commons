// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file core_config.c
 * @brief Unified config module - core layer implementation.
 *
 * Implements the core layer of the unified config module, providing:
 * 1. A unified config data model and basic interfaces
 * 2. Type-safe config access interfaces
 * 3. Clear memory ownership to avoid leaks
 * 4. Thread-safe basic operations
 */

#include "core_config.h"
#include "logging.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "platform.h"
#include "string_compat.h"

#define INDEX_NOT_FOUND (-1)

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "error.h"

struct config_value {

    config_value_type_t type;

    union {
        bool bool_value;
        int32_t int_value;
        int64_t int64_value;
        double double_value;
        struct {
            char *str;
            size_t len;
        } string_value;
        struct {
            config_value_t **items;
            size_t count;
            size_t capacity;
        } array_value;
        struct {
            struct {
                char *key;
                config_value_t *value;
            } *items;
            size_t count;
            size_t capacity;
        } object_value;
        struct {
            void *data;
            size_t size;
        } binary_value;
    } data;
};

struct config_context {
    char *name;

    struct {
        char *key;
        config_value_t *value;
    } *items;

    size_t count;

    size_t capacity;

    bool locked;

    airy_mtx_t mutex;

    config_schema_t *schema;
    bool hot_reload_enabled;
    uint32_t reload_interval_ms;
    bool encryption_enabled;
};

static config_value_t *config_value_alloc(config_value_type_t type)
{
    config_value_t *value = (config_value_t *)AIRY_CALLOC(1, sizeof(config_value_t));
    if (value) {
        value->type = type;
    }
    return value;
}

static char *duplicate_string(const char *str)
{
    if (!str) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    size_t len = strlen(str) + 1;
    char *copy = (char *)AIRY_MALLOC(len);
    if (copy) {
        __builtin_memcpy(copy, str, len);
    }
    return copy;
}

static int find_item_index(const config_context_t *ctx, const char *key)
{
    if (!ctx || !key) {
        return INDEX_NOT_FOUND;
    }

    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->items[i].key, key) == 0) {
            return (int)i;
        }
    }

    return INDEX_NOT_FOUND;
}

static config_error_t expand_context_capacity(config_context_t *ctx)
{
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    size_t new_capacity = ctx->capacity == 0 ? 16 : ctx->capacity * 2;
    void *new_items = AIRY_REALLOC(ctx->items, new_capacity * sizeof(ctx->items[0]));

    if (!new_items) {
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }

    ctx->items = new_items;
    ctx->capacity = new_capacity;

    return CONFIG_SUCCESS;
}

config_value_t *config_value_create_null(void)
{
    return config_value_alloc(CONFIG_TYPE_NULL);
}

config_value_t *config_value_create_bool(bool value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_BOOL);
    if (val) {
        val->data.bool_value = value;
    }
    return val;
}

config_value_t *config_value_create_int(int32_t value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_INT);
    if (val) {
        val->data.int_value = value;
    }
    return val;
}

config_value_t *config_value_create_int64(int64_t value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_INT64);
    if (val) {
        val->data.int64_value = value;
    }
    return val;
}

config_value_t *config_value_create_double(double value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_DOUBLE);
    if (val) {
        val->data.double_value = value;
    }
    return val;
}

config_value_t *config_value_create_string(const char *value)
{
    if (!value) {
        return config_value_create_null();
    }

    config_value_t *val = config_value_alloc(CONFIG_TYPE_STRING);
    if (val) {
        val->data.string_value.str = duplicate_string(value);
        if (!val->data.string_value.str) {
            AIRY_FREE(val);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        val->data.string_value.len = strlen(value);
    }
    return val;
}

config_value_t *config_value_create_array(size_t capacity)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_ARRAY);
    if (val) {
        val->data.array_value.capacity = capacity > 0 ? capacity : 16;
        val->data.array_value.items = (config_value_t **)AIRY_CALLOC(val->data.array_value.capacity,
                                                                     sizeof(config_value_t *));
        if (!val->data.array_value.items) {
            AIRY_FREE(val);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        val->data.array_value.count = 0;
    }
    return val;
}

config_value_t *config_value_create_object(size_t capacity)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_OBJECT);
    if (val) {
        val->data.object_value.capacity = capacity > 0 ? capacity : 16;
        val->data.object_value.items =
            AIRY_CALLOC(val->data.object_value.capacity, sizeof(val->data.object_value.items[0]));
        if (!val->data.object_value.items) {
            AIRY_FREE(val);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        val->data.object_value.count = 0;
    }
    return val;
}

config_value_t *config_value_clone(const config_value_t *value)
{
    if (!value) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    config_value_t *copy = NULL;

    switch (value->type) {
    case CONFIG_TYPE_NULL:
        copy = config_value_create_null();
        break;

    case CONFIG_TYPE_BOOL:
        copy = config_value_create_bool(value->data.bool_value);
        break;

    case CONFIG_TYPE_INT:
        copy = config_value_create_int(value->data.int_value);
        break;

    case CONFIG_TYPE_INT64:
        copy = config_value_create_int64(value->data.int64_value);
        break;

    case CONFIG_TYPE_DOUBLE:
        copy = config_value_create_double(value->data.double_value);
        break;

    case CONFIG_TYPE_STRING:
        copy = config_value_create_string(value->data.string_value.str);
        break;

    case CONFIG_TYPE_ARRAY:
        copy = config_value_create_array(value->data.array_value.capacity);
        if (copy) {
            for (size_t i = 0; i < value->data.array_value.count; i++) {
                config_value_t *item_copy = config_value_clone(value->data.array_value.items[i]);
                if (item_copy) {
                    copy->data.array_value.items[copy->data.array_value.count++] = item_copy;
                }
            }
        }
        break;

    case CONFIG_TYPE_OBJECT:
        copy = config_value_create_object(value->data.object_value.capacity);
        if (copy) {
            for (size_t i = 0; i < value->data.object_value.count; i++) {
                const char *key = value->data.object_value.items[i].key;
                config_value_t *val = value->data.object_value.items[i].value;

                char *key_copy = duplicate_string(key);
                config_value_t *val_copy = config_value_clone(val);

                if (key_copy && val_copy) {
                    copy->data.object_value.items[copy->data.object_value.count].key = key_copy;
                    copy->data.object_value.items[copy->data.object_value.count].value = val_copy;
                    copy->data.object_value.count++;
                } else {
                    AIRY_FREE(key_copy);
                    config_value_destroy(val_copy);
                }
            }
        }
        break;

    case CONFIG_TYPE_BINARY:
        if (value->data.binary_value.data && value->data.binary_value.size > 0) {
            copy = config_value_alloc(CONFIG_TYPE_BINARY);
            if (copy) {
                copy->data.binary_value.data = AIRY_MALLOC(value->data.binary_value.size);
                if (copy->data.binary_value.data) {
                    __builtin_memcpy(copy->data.binary_value.data, value->data.binary_value.data,
                                     value->data.binary_value.size);
                    copy->data.binary_value.size = value->data.binary_value.size;
                } else {
                    AIRY_FREE(copy);
                    copy = NULL;
                }
            }
        } else {
            copy = config_value_alloc(CONFIG_TYPE_BINARY);
            if (copy) {
                copy->data.binary_value.data = NULL;
                copy->data.binary_value.size = 0;
            }
        }
        break;
    }

    return copy;
}

void config_value_destroy(config_value_t *value)
{
    if (!value) {
        return;
    }

    switch (value->type) {
    case CONFIG_TYPE_STRING:
        AIRY_FREE(value->data.string_value.str);
        break;

    case CONFIG_TYPE_ARRAY:
        for (size_t i = 0; i < value->data.array_value.count; i++) {
            config_value_destroy(value->data.array_value.items[i]);
        }
        AIRY_FREE(value->data.array_value.items);
        break;

    case CONFIG_TYPE_OBJECT:
        for (size_t i = 0; i < value->data.object_value.count; i++) {
            AIRY_FREE(value->data.object_value.items[i].key);
            config_value_destroy(value->data.object_value.items[i].value);
        }
        AIRY_FREE(value->data.object_value.items);
        break;

    case CONFIG_TYPE_BINARY:
        AIRY_FREE(value->data.binary_value.data);
        break;

    default:
        break;
    }

    AIRY_FREE(value);
}

config_error_t config_value_array_append(config_value_t *array, config_value_t *item)
{
    if (!array || !item)
        return CONFIG_ERROR_INVALID_ARG;
    if (array->type != CONFIG_TYPE_ARRAY)
        return CONFIG_ERROR_TYPE_MISMATCH;

    if (array->data.array_value.count >= array->data.array_value.capacity) {
        size_t new_cap = array->data.array_value.capacity * 2;
        config_value_t **new_items =
            (config_value_t **)AIRY_REALLOC(array->data.array_value.items,
                                            new_cap * sizeof(config_value_t *));
        if (!new_items)
            return CONFIG_ERROR_OUT_OF_MEMORY;
        array->data.array_value.items = new_items;
        array->data.array_value.capacity = new_cap;
    }

    array->data.array_value.items[array->data.array_value.count++] = item;
    return CONFIG_SUCCESS;
}

config_value_type_t config_value_get_type(const config_value_t *value)
{
    return value ? value->type : CONFIG_TYPE_NULL;
}

bool config_value_get_bool(const config_value_t *value, bool default_value)
{
    if (!value || value->type != CONFIG_TYPE_BOOL) {
        return default_value;
    }
    return value->data.bool_value;
}

int32_t config_value_get_int(const config_value_t *value, int32_t default_value)
{
    if (!value) {
        return default_value;
    }

    switch (value->type) {
    case CONFIG_TYPE_INT:
        return value->data.int_value;
    case CONFIG_TYPE_INT64:
        return (int32_t)value->data.int64_value;
    case CONFIG_TYPE_DOUBLE:
        return (int32_t)value->data.double_value;
    case CONFIG_TYPE_STRING:
        return (int32_t)strtol(value->data.string_value.str, NULL, 10);
    default:
        return default_value;
    }
}

int64_t config_value_get_int64(const config_value_t *value, int64_t default_value)
{
    if (!value) {
        return default_value;
    }

    switch (value->type) {
    case CONFIG_TYPE_INT:
        return (int64_t)value->data.int_value;
    case CONFIG_TYPE_INT64:
        return value->data.int64_value;
    case CONFIG_TYPE_DOUBLE:
        return (int64_t)value->data.double_value;
    case CONFIG_TYPE_STRING:
        return strtoll(value->data.string_value.str, NULL, 10);
    default:
        return default_value;
    }
}

double config_value_get_double(const config_value_t *value, double default_value)
{
    if (!value) {
        return default_value;
    }

    switch (value->type) {
    case CONFIG_TYPE_INT:
        return (double)value->data.int_value;
    case CONFIG_TYPE_INT64:
        return (double)value->data.int64_value;
    case CONFIG_TYPE_DOUBLE:
        return value->data.double_value;
    case CONFIG_TYPE_STRING:
        return strtod(value->data.string_value.str, NULL);
    default:
        return default_value;
    }
}

const char *config_value_get_string(const config_value_t *value, const char *default_value)
{
    if (!value || value->type != CONFIG_TYPE_STRING) {
        return default_value;
    }
    return value->data.string_value.str ? value->data.string_value.str : default_value;
}

config_context_t *config_context_create(const char *name)
{
    config_context_t *ctx = (config_context_t *)AIRY_CALLOC(1, sizeof(config_context_t));
    if (!ctx) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (name) {
        ctx->name = duplicate_string(name);
    } else {
        ctx->name = duplicate_string("default");
    }

    if (!ctx->name) {
        AIRY_FREE(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ctx->capacity = 16;
    ctx->items = AIRY_CALLOC(ctx->capacity, sizeof(ctx->items[0]));

    if (!ctx->items) {
        AIRY_FREE(ctx->name);
        AIRY_FREE(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ctx->count = 0;
    ctx->locked = false;
    airy_mtx_init(&ctx->mutex);

    return ctx;
}

void config_context_destroy(config_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    airy_mtx_destroy(&ctx->mutex);

    for (size_t i = 0; i < ctx->count; i++) {
        AIRY_FREE(ctx->items[i].key);
        config_value_destroy(ctx->items[i].value);
    }

    AIRY_FREE(ctx->items);
    AIRY_FREE(ctx->name);
    AIRY_FREE(ctx);
}

config_error_t config_context_set(config_context_t *ctx, const char *key, config_value_t *value)
{
    if (!ctx || !key || !value || ctx->locked) {
        if (value) {
            config_value_destroy(value);
        }
        return CONFIG_ERROR_INVALID_ARG;
    }

    /* v0.1.1 fix (concurrency bug E): config_context_set lacked mutex
     * protection. get/delete/foreach all held the mutex but set did not,
     * so concurrent config_source_load calls on the same context raced
     * between find_item_index + config_value_destroy + assignment
     * (TOCTOU), causing a double-free (ASAN reported config_value_destroy
     * invoked concurrently on the same pointer by two threads). Fix: guard
     * set with the mutex, consistent with get/delete. */
    airy_mtx_lock(&ctx->mutex);

    int index = find_item_index(ctx, key);

    if (index >= 0) {
        config_value_destroy(ctx->items[index].value);
        ctx->items[index].value = value;
        airy_mtx_unlock(&ctx->mutex);
        return CONFIG_SUCCESS;
    } else {
        if (ctx->count >= ctx->capacity) {
            config_error_t err = expand_context_capacity(ctx);
            if (err != CONFIG_SUCCESS) {
                config_value_destroy(value);
                airy_mtx_unlock(&ctx->mutex);
                return err;
            }
        }

        char *key_copy = duplicate_string(key);
        if (!key_copy) {
            config_value_destroy(value);
            airy_mtx_unlock(&ctx->mutex);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }

        ctx->items[ctx->count].key = key_copy;
        ctx->items[ctx->count].value = value;
        ctx->count++;

        airy_mtx_unlock(&ctx->mutex);
        return CONFIG_SUCCESS;
    }
}

const config_value_t *config_context_get(const config_context_t *ctx, const char *key)
{
    if (!ctx || !key) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    airy_mtx_lock((airy_mtx_t *)&ctx->mutex);
    int index = find_item_index(ctx, key);
    const config_value_t *result = index >= 0 ? ctx->items[index].value : NULL;
    airy_mtx_unlock((airy_mtx_t *)&ctx->mutex);
    return result;
}

config_error_t config_context_delete(config_context_t *ctx, const char *key)
{
    if (!ctx || !key) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    if (ctx->locked) {
        return CONFIG_ERROR_LOCKED;
    }

    airy_mtx_lock(&ctx->mutex);

    int index = find_item_index(ctx, key);
    if (index < 0) {
        airy_mtx_unlock(&ctx->mutex);
        return CONFIG_ERROR_NOT_FOUND;
    }

    AIRY_FREE(ctx->items[index].key);
    config_value_destroy(ctx->items[index].value);

    for (size_t i = index + 1; i < ctx->count; i++) {
        ctx->items[i - 1] = ctx->items[i];
    }

    ctx->count--;
    airy_mtx_unlock(&ctx->mutex);
    return CONFIG_SUCCESS;
}

bool config_context_has(const config_context_t *ctx, const char *key)
{
    if (!ctx || !key)
        return false;
    airy_mtx_lock((airy_mtx_t *)&ctx->mutex);
    bool result = find_item_index(ctx, key) >= 0;
    airy_mtx_unlock((airy_mtx_t *)&ctx->mutex);
    return result;
}

void config_context_clear(config_context_t *ctx)
{
    if (!ctx)
        return;

    if (ctx->locked)
        return;

    airy_mtx_lock(&ctx->mutex);

    for (size_t i = 0; i < ctx->count; i++) {
        AIRY_FREE(ctx->items[i].key);
        config_value_destroy(ctx->items[i].value);
    }

    ctx->count = 0;
    airy_mtx_unlock(&ctx->mutex);
}

size_t config_context_count(const config_context_t *ctx)
{
    if (!ctx)
        return 0;
    airy_mtx_lock((airy_mtx_t *)&ctx->mutex);
    size_t result = ctx->count;
    airy_mtx_unlock((airy_mtx_t *)&ctx->mutex);
    return result;
}

config_error_t config_context_lock(config_context_t *ctx)
{
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    ctx->locked = true;
    return CONFIG_SUCCESS;
}

config_error_t config_context_unlock(config_context_t *ctx)
{
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    ctx->locked = false;
    return CONFIG_SUCCESS;
}

config_context_t *config_context_clone(const config_context_t *ctx)
{
    if (!ctx)
        return NULL;

    config_context_t *clone = config_context_create(ctx->name);
    if (!clone)
        return NULL;

    for (size_t i = 0; i < ctx->count; i++) {
        char *key_copy = duplicate_string(ctx->items[i].key);
        if (!key_copy) {
            config_context_destroy(clone);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        config_value_t *val_copy = config_value_clone(ctx->items[i].value);
        if (!val_copy) {
            AIRY_FREE(key_copy);
            config_context_destroy(clone);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        if (clone->count >= clone->capacity) {
            config_error_t err = expand_context_capacity(clone);
            if (err != CONFIG_SUCCESS) {
                AIRY_FREE(key_copy);
                config_value_destroy(val_copy);
                config_context_destroy(clone);
                AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
            }
        }
        clone->items[clone->count].key = key_copy;
        clone->items[clone->count].value = val_copy;
        clone->count++;
    }

    clone->locked = ctx->locked;
    return clone;
}

config_error_t config_context_copy(config_context_t *dst, const config_context_t *src)
{
    if (!dst || !src)
        return CONFIG_ERROR_INVALID_ARG;
    if (dst->locked)
        return CONFIG_ERROR_LOCKED;

    config_context_clear(dst);

    for (size_t i = 0; i < src->count; i++) {
        char *key_copy = duplicate_string(src->items[i].key);
        if (!key_copy)
            return CONFIG_ERROR_OUT_OF_MEMORY;
        config_value_t *val_copy = config_value_clone(src->items[i].value);
        if (!val_copy) {
            AIRY_FREE(key_copy);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
        if (dst->count >= dst->capacity) {
            config_error_t err = expand_context_capacity(dst);
            if (err != CONFIG_SUCCESS) {
                AIRY_FREE(key_copy);
                config_value_destroy(val_copy);
                return err;
            }
        }
        dst->items[dst->count].key = key_copy;
        dst->items[dst->count].value = val_copy;
        dst->count++;
    }

    return CONFIG_SUCCESS;
}

const char *config_context_get_key_at(const config_context_t *ctx, size_t index)
{
    if (!ctx || index >= ctx->count)
        return NULL;
    return ctx->items[index].key;
}

const config_value_t *config_context_get_value_at(const config_context_t *ctx, size_t index)
{
    if (!ctx || index >= ctx->count)
        return NULL;
    return ctx->items[index].value;
}

struct config_iterator {
    const config_context_t *ctx;
    size_t pos;
};

const config_iterator_t *config_context_iterator(const config_context_t *ctx)
{
    if (!ctx)
        return NULL;
    config_iterator_t *it = (config_iterator_t *)AIRY_CALLOC(1, sizeof(config_iterator_t));
    if (!it)
        return NULL;
    it->ctx = ctx;
    it->pos = 0;
    return it;
}

void config_iterator_reset(const config_iterator_t *it)
{
    if (!it)
        return;
    ((config_iterator_t *)it)->pos = 0;
}

bool config_iterator_has_next(const config_iterator_t *it)
{
    if (!it || !it->ctx)
        return false;
    return it->pos < it->ctx->count;
}

const char *config_iterator_next_key(const config_iterator_t *it)
{
    if (!it || !it->ctx || it->pos >= it->ctx->count)
        return NULL;
    const char *key = it->ctx->items[it->pos].key;
    ((config_iterator_t *)it)->pos++;
    return key;
}

void config_context_set_schema(config_context_t *ctx, config_schema_t *schema)
{
    if (!ctx)
        return;
    ctx->schema = schema;
}

void config_context_set_hot_reload(config_context_t *ctx, bool enabled, uint32_t interval_ms)
{
    if (!ctx)
        return;
    ctx->hot_reload_enabled = enabled;
    ctx->reload_interval_ms = interval_ms;
}

void config_context_set_encryption(config_context_t *ctx, bool enabled)
{
    if (!ctx)
        return;
    ctx->encryption_enabled = enabled;
}

const char *config_error_to_string(config_error_t error)
{
    static const char *error_strings[] = {"Success",       "Invalid argument",
                                          "Not found",     "Type mismatch",
                                          "Out of memory", "I/O error",
                                          "Parse error",   "Validation failed",
                                          "Config locked", "Unsupported operation"};

    if (error >= 0 && error < sizeof(error_strings) / sizeof(error_strings[0])) {
        return error_strings[error];
    }

    return "Unknown error";
}

const char *config_type_to_string(config_value_type_t type)
{
    static const char *type_strings[] = {"Null",   "Boolean", "Int32",  "Int64", "Double",
                                         "String", "Array",   "Object", "Binary"};

    if (type >= 0 && type < sizeof(type_strings) / sizeof(type_strings[0])) {
        return type_strings[type];
    }

    return "未知类型";
}

void config_value_print(const config_value_t *value, int indent)
{
    if (!value) {
        AIRY_LOG_DEBUG("%*s(null)", indent, "");
        return;
    }

    switch (value->type) {
    case CONFIG_TYPE_NULL:
        AIRY_LOG_DEBUG("%*snull", indent, "");
        break;

    case CONFIG_TYPE_BOOL:
        AIRY_LOG_DEBUG("%*s%s", indent, "", value->data.bool_value ? "true" : "false");
        break;

    case CONFIG_TYPE_INT:
        AIRY_LOG_DEBUG("%*s%d", indent, "", value->data.int_value);
        break;

    case CONFIG_TYPE_INT64:
        AIRY_LOG_DEBUG("%*s%lld", indent, "", (long long)value->data.int64_value);
        break;

    case CONFIG_TYPE_DOUBLE:
        AIRY_LOG_DEBUG("%*s%g", indent, "", value->data.double_value);
        break;

    case CONFIG_TYPE_STRING:
        AIRY_LOG_DEBUG("%*s\"%s\"", indent, "", value->data.string_value.str);
        break;

    case CONFIG_TYPE_ARRAY:
        AIRY_LOG_DEBUG("%*s[", indent, "");
        for (size_t i = 0; i < value->data.array_value.count; i++) {
            config_value_print(value->data.array_value.items[i], indent + 2);
        }
        AIRY_LOG_DEBUG("%*s]", indent, "");
        break;

    case CONFIG_TYPE_OBJECT:
        AIRY_LOG_DEBUG("%*s{", indent, "");
        for (size_t i = 0; i < value->data.object_value.count; i++) {
            AIRY_LOG_DEBUG("%*s\"%s\": ", indent + 2, "", value->data.object_value.items[i].key);
            config_value_print(value->data.object_value.items[i].value, 0);
        }
        AIRY_LOG_DEBUG("%*s}", indent, "");
        break;

    case CONFIG_TYPE_BINARY:
        AIRY_LOG_DEBUG("%*s<binary data, size=%zu>", indent, "", value->data.binary_value.size);
        break;
    }
}
