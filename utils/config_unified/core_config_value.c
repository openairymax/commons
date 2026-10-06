// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file core_config_value.c
 * @brief Unified config module - config value object domain.
 *
 * Implements the typed config value model: scalar creation, cloning,
 * deep destruction and type-safe getters, single responsibility.
 * Split out of core_config.c.
 */

#include "core_config.h"

#include "core_config_internal.h"

#include "airy_memory.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

static config_value_t *config_value_alloc(config_value_type_t type)
{
    config_value_t *value = (config_value_t *)AIRY_CALLOC(1, sizeof(config_value_t));
    if (value) {
        value->type = type;
    }
    return value;
}

char *duplicate_string(const char *str)
{
    if (!str) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    size_t len = strlen(str) + 1;
    char *copy = (char *)AIRY_MALLOC(len);
    if (copy) {
        AIRY_MEMCPY(copy, str, len);
    }
    return copy;
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

    default:
        break;
    }

    AIRY_FREE(value);
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
