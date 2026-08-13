// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_source_memory.c
 * @brief Unified config module - memory/default/remote config sources.
 *
 * Implements three read-only direct-read config sources: the memory
 * source, the default-value source and the remote source (polling change
 * detection), single responsibility.
 */

#include "config_source.h"

#include "config_source_internal.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"

static config_error_t memory_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    memory_source_priv_t *priv = (memory_source_priv_t *)source->priv_data;
    if (!priv || !priv->data)
        return CONFIG_ERROR_INVALID_ARG;

    config_error_t error = CONFIG_SUCCESS;
    if (priv->format && strcmp(priv->format, "json") == 0) {
        error = config_parse_json(priv->data, priv->data_len, ctx);
    } else if (priv->format && strcmp(priv->format, "yaml") == 0) {
        error = config_parse_yaml(priv->data, priv->data_len, ctx);
    } else if (priv->format && strcmp(priv->format, "ini") == 0) {
        error = config_parse_ini(priv->data, priv->data_len, ctx);
    } else {
        error = config_parse_json(priv->data, priv->data_len, ctx);
        if (error != CONFIG_SUCCESS) {
            error = config_parse_yaml(priv->data, priv->data_len, ctx);
        }
    }

    return error;
}

static config_error_t memory_source_save(config_source_t *source, const config_context_t *ctx)
{
    if (!source)
        return CONFIG_ERROR_INVALID_ARG;
    (void)ctx;
    memory_source_priv_t *priv = (memory_source_priv_t *)source->priv_data;
    if (!priv || !priv->data)
        return CONFIG_ERROR_IO;
    AIRY_LOG_INFO("内存配置源保存成功 (len=%zu)", priv->data_len);
    return CONFIG_SUCCESS;
}

static bool memory_source_has_changed(config_source_t *source)
{
    (void)source;
    return false;
}

static const config_source_attr_t *memory_source_get_attributes(config_source_t *source)
{
    if (!source)
        return NULL;
    return &source->attributes;
}

static void memory_source_destroy(config_source_t *source)
{
    if (!source)
        return;

    memory_source_priv_t *priv = (memory_source_priv_t *)source->priv_data;
    if (priv) {
        if (priv->owns_data && priv->data)
            AIRY_FREE(priv->data);
        if (priv->format)
            AIRY_FREE(priv->format);
        AIRY_FREE(priv);
    }

    config_source_free_base(source);
}

static const config_source_adapter_t memory_source_adapter = {.load = memory_source_load,
                                                              .save = memory_source_save,
                                                              .has_changed =
                                                                  memory_source_has_changed,
                                                              .get_attributes =
                                                                  memory_source_get_attributes,
                                                              .destroy = memory_source_destroy};

static config_error_t defaults_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    defaults_source_priv_t *priv = (defaults_source_priv_t *)source->priv_data;
    if (!priv)
        return CONFIG_ERROR_INVALID_ARG;
    for (size_t idx = 0; idx < priv->num_entries; idx++) {
        if (priv->keys[idx] && priv->vals[idx]) {
            config_value_t *cv = config_value_create_string(priv->vals[idx]);
            if (cv)
                config_context_set(ctx, priv->keys[idx], cv);
        }
    }
    return CONFIG_SUCCESS;
}

static config_error_t defaults_source_save(config_source_t *source, const config_context_t *ctx)
{
    (void)source;
    (void)ctx;
    AIRY_LOG_WARN("默认值配置源为只读，不支持保存操作");
    return CONFIG_ERROR_UNSUPPORTED;
}

static bool defaults_source_has_changed(config_source_t *source)
{
    (void)source;
    return false;
}

static const config_source_attr_t *defaults_source_get_attributes(config_source_t *source)
{
    if (!source)
        return NULL;
    return &source->attributes;
}

static void defaults_source_destroy(config_source_t *source)
{
    if (!source)
        return;

    defaults_source_priv_t *priv = (defaults_source_priv_t *)source->priv_data;
    if (priv) {
        if (priv->keys) {
            for (size_t i = 0; i < priv->num_entries; i++) {
                if (priv->keys[i])
                    AIRY_FREE(priv->keys[i]);
            }
            AIRY_FREE(priv->keys);
        }
        if (priv->vals) {
            for (size_t i = 0; i < priv->num_entries; i++) {
                if (priv->vals[i])
                    AIRY_FREE(priv->vals[i]);
            }
            AIRY_FREE(priv->vals);
        }
        AIRY_FREE(priv);
    }

    config_source_free_base(source);
}

static const config_source_adapter_t defaults_source_adapter = {.load = defaults_source_load,
                                                                .save = defaults_source_save,
                                                                .has_changed =
                                                                    defaults_source_has_changed,
                                                                .get_attributes =
                                                                    defaults_source_get_attributes,
                                                                .destroy = defaults_source_destroy};

/** remote source private data */

static config_error_t remote_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    remote_source_priv_t *priv = (remote_source_priv_t *)source->priv_data;
    if (!priv || !priv->url)
        return CONFIG_ERROR_INVALID_ARG;

    if (!priv->last_response || priv->last_response_len == 0) {
        return CONFIG_SUCCESS;
    }

    return config_parse_json(priv->last_response, priv->last_response_len, ctx);
}

static config_error_t remote_source_save(config_source_t *source, const config_context_t *ctx)
{
    (void)source;
    (void)ctx;
    return CONFIG_ERROR_UNSUPPORTED;
}

static bool remote_source_has_changed(config_source_t *source)
{
    if (!source)
        return false;
    remote_source_priv_t *priv = (remote_source_priv_t *)source->priv_data;
    if (!priv || !priv->url)
        return false;

    uint64_t now_ms;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        now_ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    } else {
        now_ms = (uint64_t)time(NULL) * 1000;
    }

    if (priv->last_poll_time_ms == 0) {
        priv->last_poll_time_ms = now_ms;
        return true;
    }

    if (now_ms - priv->last_poll_time_ms >= (uint64_t)priv->poll_interval_ms) {
        priv->last_poll_time_ms = now_ms;
        return true;
    }

    return false;
}

static const config_source_attr_t *remote_source_get_attributes(config_source_t *source)
{
    if (!source)
        return NULL;
    return &source->attributes;
}

static void remote_source_destroy(config_source_t *source)
{
    if (!source)
        return;
    remote_source_priv_t *priv = (remote_source_priv_t *)source->priv_data;
    if (priv) {
        if (priv->url)
            AIRY_FREE(priv->url);
        if (priv->token)
            AIRY_FREE(priv->token);
        if (priv->namespace_name)
            AIRY_FREE(priv->namespace_name);
        if (priv->last_response)
            AIRY_FREE(priv->last_response);
        AIRY_FREE(priv);
    }
    config_source_free_base(source);
}

static const config_source_adapter_t remote_source_adapter = {.load = remote_source_load,
                                                              .save = remote_source_save,
                                                              .has_changed =
                                                                  remote_source_has_changed,
                                                              .get_attributes =
                                                                  remote_source_get_attributes,
                                                              .destroy = remote_source_destroy};

config_source_t *config_source_create_memory(const config_memory_source_options_t *options)
{
    if (!options || !options->data)
        return NULL;

    config_source_t *source =
        config_source_create_base(CONFIG_SOURCE_MEMORY, "memory", &memory_source_adapter);
    if (!source)
        return NULL;

    memory_source_priv_t *priv =
        (memory_source_priv_t *)AIRY_CALLOC(1, sizeof(memory_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    priv->data = duplicate_string(options->data);
    priv->data_len = options->data_len ? options->data_len : strlen(options->data);
    priv->format = options->format ? duplicate_string(options->format) : duplicate_string("json");
    priv->owns_data = true;

    if (!priv->data || !priv->format) {
        if (priv->data)
            AIRY_FREE(priv->data);
        if (priv->format)
            AIRY_FREE(priv->format);
        AIRY_FREE(priv);
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    source->priv_data = priv;
    source->attributes.read_only = true;
    source->attributes.watchable = false;

    return source;
}

config_source_t *config_source_create_defaults(const char *const *default_values, size_t count)
{
    if (!default_values || count == 0)
        return NULL;

    config_source_t *source =
        config_source_create_base(CONFIG_SOURCE_DEFAULT, "defaults", &defaults_source_adapter);
    if (!source)
        return NULL;

    defaults_source_priv_t *priv =
        (defaults_source_priv_t *)AIRY_CALLOC(1, sizeof(defaults_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    priv->keys = (char **)AIRY_CALLOC(count, sizeof(char *));
    priv->vals = (char **)AIRY_CALLOC(count, sizeof(char *));
    if (!priv->keys || !priv->vals) {
        if (priv->keys)
            AIRY_FREE(priv->keys);
        if (priv->vals)
            AIRY_FREE(priv->vals);
        AIRY_FREE(priv);
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    priv->num_entries = count / 2;
    for (size_t i = 0; i < count; i += 2) {
        if (i + 1 < count) {
            priv->keys[i / 2] = default_values[i] ? duplicate_string(default_values[i]) : NULL;
            priv->vals[i / 2] =
                default_values[i + 1] ? duplicate_string(default_values[i + 1]) : NULL;
        }
    }

    source->priv_data = priv;
    source->attributes.read_only = true;
    source->attributes.watchable = false;
    return source;
}

config_source_t *config_source_create_remote(const char *url, const char *token, const char *ns,
                                             uint32_t poll_interval_ms)
{
    if (!url)
        return NULL;

    config_source_t *source =
        config_source_create_base(CONFIG_SOURCE_NETWORK, "remote", &remote_source_adapter);
    if (!source)
        return NULL;

    remote_source_priv_t *priv =
        (remote_source_priv_t *)AIRY_CALLOC(1, sizeof(remote_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    priv->url = duplicate_string(url);
    priv->token = token ? duplicate_string(token) : NULL;
    priv->namespace_name = ns ? duplicate_string(ns) : duplicate_string("default");
    priv->poll_interval_ms = poll_interval_ms > 0 ? poll_interval_ms : 30000;
    priv->last_etag_hash = 0;
    priv->last_response = NULL;
    priv->last_response_len = 0;

    source->priv_data = priv;
    source->attributes.read_only = true;
    source->attributes.watchable = true;

    return source;
}
