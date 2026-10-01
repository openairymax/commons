// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_service.c
 * @brief Unified config module - service layer main entry.
 *
 * Keeps the service layer entry: config service lifecycle management
 * (create/load). Schema defaults application lives in
 * config_service_validator.c.
 */

#include "config_service.h"

#include "config_source.h"
#include "core_config.h"

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "error.h"

config_context_t *config_service_create(const char *service_name, config_schema_t *schema,
                                        bool enable_hot_reload, bool enable_encryption)
{
    if (!service_name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    config_context_t *ctx = config_context_create(service_name);
    if (!ctx) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (schema) {
        config_context_set_schema(ctx, schema);
        config_schema_apply_defaults(schema, ctx);
    }

    if (enable_hot_reload) {
        config_context_set_hot_reload(ctx, true, 5000);
    }

    if (enable_encryption) {
        config_context_set_encryption(ctx, true);
    }

    return ctx;
}

config_error_t config_service_load(config_context_t *ctx, config_source_t **sources,
                                   size_t source_count)
{
    if (!ctx || !sources || source_count == 0)
        return CONFIG_ERROR_INVALID_ARG;

    config_error_t err = CONFIG_SUCCESS;
    for (size_t i = 0; i < source_count; i++) {
        if (!sources[i])
            continue;
        err = config_source_load(sources[i], ctx);
        if (err != CONFIG_SUCCESS)
            return err;
    }

    return CONFIG_SUCCESS;
}
