// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_service.c
 * @brief Unified config module - service layer main entry.
 *
 * Keeps the service layer entry and core state machine, providing:
 * 1. Config template and variable expansion
 * 2. Config service lifecycle management (create/load/save/status query)
 *
 * The validator/Schema, hot reload, crypto and version management are
 * split into config_service_validator.c / config_service_hotreload.c /
 * config_service_crypto.c / config_service_version.c.
 */

#include "config_service.h"

#include "config_source.h"
#include "core_config.h"

#include <platform.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>
#include "error.h"

config_error_t config_expand_template(config_context_t *ctx, const char *template_str, char *result,
                                      size_t result_size)
{
    if (!ctx || !template_str || !result || result_size == 0) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    size_t out_pos = 0;
    const char *p = template_str;

    while (*p && out_pos < result_size - 1) {
        if (*p == '$' && *(p + 1) == '{') {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (end && end > start) {
                size_t key_len = (size_t)(end - start);
                char key_buf[256];
                if (key_len >= sizeof(key_buf))
                    key_len = sizeof(key_buf) - 1;
                __builtin_memcpy(key_buf, start, key_len);
                key_buf[key_len] = '\0';

                const config_value_t *val = config_context_get(ctx, key_buf);
                if (val) {
                    const char *str_val = config_value_get_string(val, "");
                    if (str_val) {
                        size_t vlen = strlen(str_val);
                        if (out_pos + vlen >= result_size)
                            vlen = result_size - out_pos - 1;
                        __builtin_memcpy(result + out_pos, str_val, vlen);
                        out_pos += vlen;
                    }
                } else {
                    if (out_pos + key_len + 3 < result_size) {
                        result[out_pos++] = '$';
                        result[out_pos++] = '{';
                        __builtin_memcpy(result + out_pos, key_buf, key_len);
                        out_pos += key_len;
                        result[out_pos++] = '}';
                    }
                }
                p = end + 1;
            } else {
                result[out_pos++] = *p++;
            }
        } else {
            result[out_pos++] = *p++;
        }
    }

    result[out_pos] = '\0';
    return CONFIG_SUCCESS;
}

config_error_t config_apply_template(config_context_t *ctx, config_context_t *template_ctx)
{
    if (!ctx || !template_ctx)
        return CONFIG_ERROR_INVALID_ARG;

    const config_iterator_t *it = config_context_iterator(template_ctx);
    if (!it)
        return CONFIG_SUCCESS;

    config_iterator_reset(it);
    while (config_iterator_has_next(it)) {
        const char *key = config_iterator_next_key(it);
        const config_value_t *val = config_context_get(template_ctx, key);
        if (val && !config_context_has(ctx, key)) {
            config_value_t *cloned = config_value_clone(val);
            if (cloned) {
                config_context_set(ctx, key, cloned);
            }
        }
    }

    return CONFIG_SUCCESS;
}

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

config_error_t config_service_save(config_context_t *ctx, config_source_t *primary_source)
{
    if (!ctx || !primary_source)
        return CONFIG_ERROR_INVALID_ARG;

    return config_source_save(primary_source, ctx);
}

config_error_t config_service_get_status(config_context_t *ctx, char *status_json,
                                         size_t status_size)
{
    if (!ctx || !status_json || status_size == 0)
        return CONFIG_ERROR_INVALID_ARG;

    snprintf(status_json, status_size, "{\"status\":\"ok\",\"service\":\"%s\"}", "config_service");

    return CONFIG_SUCCESS;
}
