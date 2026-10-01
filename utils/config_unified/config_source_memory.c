// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_source_memory.c
 * @brief Unified config module - memory config source.
 *
 * Implements the read-only in-memory config source, single responsibility.
 */

#include "config_source.h"

#include "config_source_internal.h"
#include "logging_compat.h"

#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
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
