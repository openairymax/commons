// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_source.c
 * @brief Unified config module - source adapter main entry.
 *
 * Keeps the source adapter base class and common API: config source
 * object create/release and generic load/save/change-detection/
 * attribute access.
 *
 * The per-type config sources are split into config_source_env.c and
 * config_source_memory.c.
 */

#include "config_source.h"

#include "config_source_internal.h"
#include "core_config.h"

#include <time.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "error.h"

config_source_t *config_source_create_base(config_source_type_t type, const char *name,
                                           const config_source_adapter_t *adapter)
{
    config_source_t *source = (config_source_t *)AIRY_CALLOC(1, sizeof(config_source_t));
    if (!source)
        return NULL;

    source->adapter = adapter;
    source->attributes.type = type;
    source->attributes.name = name ? AIRY_STRDUP(name) : NULL;
    source->attributes.priority = 50;
    source->attributes.read_only = false;
    source->attributes.watchable = false;
    source->attributes.timestamp = (uint64_t)time(NULL);
    source->attributes.version = 1;

    return source;
}

void config_source_free_base(config_source_t *source)
{
    if (!source)
        return;

    if (source->attributes.name) {
        AIRY_FREE((void *)source->attributes.name);
        source->attributes.name = NULL;
    }

    AIRY_FREE(source);
}

void config_source_destroy(config_source_t *source)
{
    if (!source)
        return;

    if (source->adapter && source->adapter->destroy) {
        source->adapter->destroy(source);
    } else {
        config_source_free_base(source);
    }
}

config_error_t config_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx || !source->adapter || !source->adapter->load) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    return source->adapter->load(source, ctx);
}

config_error_t config_source_save(config_source_t *source, const config_context_t *ctx)
{
    if (!source || !ctx || !source->adapter || !source->adapter->save) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    return source->adapter->save(source, ctx);
}

bool config_source_has_changed(config_source_t *source)
{
    if (!source || !source->adapter || !source->adapter->has_changed) {
        return false;
    }

    return source->adapter->has_changed(source);
}

const config_source_attr_t *config_source_get_attributes(config_source_t *source)
{
    if (!source || !source->adapter || !source->adapter->get_attributes) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    return source->adapter->get_attributes(source);
}

const char *config_source_type_to_string(config_source_type_t type)
{
    switch (type) {
    case CONFIG_SOURCE_FILE:
        return "file";
    case CONFIG_SOURCE_ENV:
        return "env";
    case CONFIG_SOURCE_ARGS:
        return "args";
    case CONFIG_SOURCE_MEMORY:
        return "memory";
    case CONFIG_SOURCE_NETWORK:
        return "network";
    case CONFIG_SOURCE_DATABASE:
        return "database";
    case CONFIG_SOURCE_DEFAULT:
        return "default";
    default:
        return "unknown";
    }
}
