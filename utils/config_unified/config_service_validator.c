// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_service_validator.c
 * @brief Unified config module - Schema implementation.
 *
 * Implements config Schema creation, item registration, key lookup
 * and default value application.
 */

#include "config_service.h"

#include "config_service_internal.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "error.h"

static int find_schema_item(const config_schema_t *schema, const char *key)
{
    if (!schema || !key)
        return INDEX_NOT_FOUND;

    for (size_t i = 0; i < schema->count; i++) {
        if (schema->items[i].key && strcmp(schema->items[i].key, key) == 0) {
            return (int)i;
        }
    }

    return INDEX_NOT_FOUND;
}

config_schema_t *config_schema_create(const char *name)
{
    if (!name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    config_schema_t *schema = (config_schema_t *)AIRY_CALLOC(1, sizeof(config_schema_t));
    if (!schema) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    schema->name = duplicate_string(name);
    if (!schema->name) {
        AIRY_FREE(schema);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    schema->capacity = 16;
    schema->items =
        (schema_item_internal_t *)AIRY_CALLOC(schema->capacity, sizeof(schema_item_internal_t));
    if (!schema->items) {
        AIRY_FREE(schema->name);
        AIRY_FREE(schema);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    schema->count = 0;

    return schema;
}

void config_schema_destroy(config_schema_t *schema)
{
    if (!schema)
        return;

    if (schema->name)
        AIRY_FREE(schema->name);

    for (size_t i = 0; i < schema->count; i++) {
        schema_item_internal_t *item = &schema->items[i];
        if (item->key)
            AIRY_FREE(item->key);
        if (item->description)
            AIRY_FREE(item->description);
        if (item->default_value)
            AIRY_FREE(item->default_value);
    }

    if (schema->items)
        AIRY_FREE(schema->items);

    AIRY_FREE(schema);
}

config_error_t config_schema_add_item(config_schema_t *schema, const config_schema_item_t *item)
{
    if (!schema || !item || !item->key)
        return CONFIG_ERROR_INVALID_ARG;

    if (find_schema_item(schema, item->key) >= 0) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    if (schema->count >= schema->capacity) {
        size_t new_capacity = schema->capacity * 2;
        schema_item_internal_t *new_items =
            (schema_item_internal_t *)AIRY_REALLOC(schema->items,
                                                   new_capacity * sizeof(schema_item_internal_t));
        if (!new_items)
            return CONFIG_ERROR_OUT_OF_MEMORY;

        schema->items = new_items;
        schema->capacity = new_capacity;
    }

    schema_item_internal_t *new_item = &schema->items[schema->count];
    AIRY_MEMSET(new_item, 0, sizeof(schema_item_internal_t));

    new_item->key = duplicate_string(item->key);
    if (!new_item->key)
        return CONFIG_ERROR_OUT_OF_MEMORY;

    new_item->type = item->type;
    new_item->required = item->required;

    if (item->description) {
        new_item->description = duplicate_string(item->description);
        if (!new_item->description) {
            AIRY_FREE(new_item->key);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
    }

    if (item->default_value) {
        new_item->default_value = duplicate_string(item->default_value);
        if (!new_item->default_value) {
            if (new_item->description)
                AIRY_FREE(new_item->description);
            AIRY_FREE(new_item->key);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
    }

    schema->count++;
    return CONFIG_SUCCESS;
}

config_error_t config_schema_apply_defaults(config_schema_t *schema, config_context_t *ctx)
{
    if (!schema || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    for (size_t i = 0; i < schema->count; i++) {
        schema_item_internal_t *item = &schema->items[i];

        if (item->default_value) {
            bool has_key = config_context_has(ctx, item->key);

            if (!has_key) {
                config_value_t *default_value = NULL;

                switch (item->type) {
                case CONFIG_TYPE_BOOL:
                    default_value =
                        config_value_create_bool(strcasecmp(item->default_value, "true") == 0);
                    break;

                case CONFIG_TYPE_INT:
                    default_value =
                        config_value_create_int((int)strtol(item->default_value, NULL, 10));
                    break;

                case CONFIG_TYPE_INT64:
                    default_value = config_value_create_int64(atoll(item->default_value));
                    break;

                case CONFIG_TYPE_DOUBLE:
                    default_value = config_value_create_double(atof(item->default_value));
                    break;

                case CONFIG_TYPE_STRING:
                    default_value = config_value_create_string(item->default_value);
                    break;

                default:
                    continue;
                }

                if (default_value) {
                    config_error_t err = config_context_set(ctx, item->key, default_value);
                    if (err != CONFIG_SUCCESS)
                        return err;
                }
            }
        }
    }

    return CONFIG_SUCCESS;
}
