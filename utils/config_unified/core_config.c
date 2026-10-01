// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file core_config.c
 * @brief Unified config module - core layer (config context domain).
 *
 * Implements the config context: create/destroy and thread-safe
 * set/get/has/clear/count plus clone/copy. The typed value model
 * lives in core_config_value.c (single responsibility per file).
 */

#include "core_config.h"

#include "core_config_internal.h"

#include "airy_memory.h"
#include "error.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>

#define INDEX_NOT_FOUND (-1)

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
    if (!ctx || !key || !value) {
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

    return clone;
}

config_error_t config_context_copy(config_context_t *dst, const config_context_t *src)
{
    if (!dst || !src)
        return CONFIG_ERROR_INVALID_ARG;

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
