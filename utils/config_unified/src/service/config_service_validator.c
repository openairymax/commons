// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_service_validator.c
 * @brief Unified config module - validator and Schema implementation.
 *
 * Implements the config validator (range/regex/enum/custom) and config
 * Schema creation, validation, error collection and default value
 * application, single responsibility.
 */

#include "config_service.h"

#include "config_service_internal.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "error.h"

static bool add_schema_error(config_schema_t *schema, const char *format, ...)
{
    if (!schema || !format)
        return false;

    if (schema->error_count >= schema->error_capacity) {
        size_t new_capacity = schema->error_capacity == 0 ? 8 : schema->error_capacity * 2;
        char **new_errors = (char **)AIRY_REALLOC(schema->errors, new_capacity * sizeof(char *));
        if (!new_errors)
            return false;

        schema->errors = new_errors;
        schema->error_capacity = new_capacity;
    }

    char buffer[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format,
                        args); /* flawfinder: ignore - variadic wrapper with bounded buffer */
    va_end(args);

    if (len < 0 || len >= (int)sizeof(buffer)) {
        buffer[sizeof(buffer) - 1] = '\0';
    }

    schema->errors[schema->error_count] = duplicate_string(buffer);
    if (!schema->errors[schema->error_count])
        return false;

    schema->error_count++;
    return true;
}

static void clear_schema_errors(config_schema_t *schema)
{
    if (!schema)
        return;

    for (size_t i = 0; i < schema->error_count; i++) {
        if (schema->errors[i]) {
            AIRY_FREE(schema->errors[i]);
            schema->errors[i] = NULL;
        }
    }

    schema->error_count = 0;
}

static bool validate_value_type(const config_value_t *value, config_value_type_t expected_type)
{
    if (!value)
        return expected_type == CONFIG_TYPE_NULL;
    return config_value_get_type(value) == expected_type;
}

static bool validate_range(const config_value_t *value, const char *min_str, const char *max_str)
{
    if (!value || !min_str || !max_str)
        return false;

    config_value_type_t type = config_value_get_type(value);

    if (type == CONFIG_TYPE_INT) {
        int val = config_value_get_int(value, 0);
        int min_val = atoi(min_str);
        int max_val = atoi(max_str);
        return val >= min_val && val <= max_val;
    } else if (type == CONFIG_TYPE_INT64) {
        int64_t val = config_value_get_int64(value, 0);
        int64_t min_val = atoll(min_str);
        int64_t max_val = atoll(max_str);
        return val >= min_val && val <= max_val;
    } else if (type == CONFIG_TYPE_DOUBLE) {
        double val = config_value_get_double(value, 0.0);
        double min_val = atof(min_str);
        double max_val = atof(max_str);
        return val >= min_val && val <= max_val;
    }

    return false;
}

static bool validate_enum(const config_value_t *value, const char **enum_values, size_t enum_count)
{
    if (!value || config_value_get_type(value) != CONFIG_TYPE_STRING || !enum_values ||
        enum_count == 0) {
        return false;
    }

    const char *str_val = config_value_get_string(value, "");
    if (!str_val)
        return false;

    for (size_t i = 0; i < enum_count; i++) {
        if (enum_values[i] && strcmp(str_val, enum_values[i]) == 0) {
            return true;
        }
    }

    return false;
}

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

config_validator_t *config_validator_create(const validator_options_t *options)
{
    if (!options) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    config_validator_t *validator =
        (config_validator_t *)AIRY_CALLOC(1, sizeof(config_validator_t));
    if (!validator) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    validator->type = options->type;
    validator->custom_cb = options->custom_cb;
    validator->user_data = options->user_data;

    if (options->pattern) {
        validator->pattern = duplicate_string(options->pattern);
        if (!validator->pattern) {
            AIRY_FREE(validator);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
    }

    if (options->enum_values && options->enum_count > 0) {
        validator->enum_values = (char **)AIRY_CALLOC(options->enum_count, sizeof(char *));
        if (!validator->enum_values) {
            if (validator->pattern)
                AIRY_FREE(validator->pattern);
            AIRY_FREE(validator);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }

        for (size_t i = 0; i < options->enum_count; i++) {
            if (options->enum_values[i]) {
                validator->enum_values[i] = duplicate_string(options->enum_values[i]);
                if (!validator->enum_values[i]) {
                    for (size_t j = 0; j < i; j++) {
                        if (validator->enum_values[j])
                            AIRY_FREE(validator->enum_values[j]);
                    }
                    AIRY_FREE(validator->enum_values);
                    if (validator->pattern)
                        AIRY_FREE(validator->pattern);
                    AIRY_FREE(validator);
                    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
                }
            }
        }

        validator->enum_count = options->enum_count;
    }

    return validator;
}

void config_validator_destroy(config_validator_t *validator)
{
    if (!validator)
        return;

    if (validator->pattern)
        AIRY_FREE(validator->pattern);

    if (validator->enum_values) {
        for (size_t i = 0; i < validator->enum_count; i++) {
            if (validator->enum_values[i])
                AIRY_FREE(validator->enum_values[i]);
        }
        AIRY_FREE(validator->enum_values);
    }

    if (validator->error_message)
        AIRY_FREE(validator->error_message);

    AIRY_FREE(validator);
}

bool config_validator_validate(config_validator_t *validator, const char *key,
                               const config_value_t *value)
{
    if (!validator || !value)
        return false;

    switch (validator->type) {
    case VALIDATOR_TYPE_RANGE:
        if (!validator->pattern)
            return false;
        {
            char *comma = strchr(validator->pattern, ',');
            if (!comma)
                return false;
            char min_buf[64], max_buf[64];
            size_t min_len = (size_t)(comma - validator->pattern);
            if (min_len >= sizeof(min_buf))
                min_len = sizeof(min_buf) - 1;
            __builtin_memcpy(min_buf, validator->pattern, min_len);
            min_buf[min_len] = '\0';
            AIRY_STRNCPY_TERM(max_buf, comma + 1, sizeof(max_buf));
            max_buf[sizeof(max_buf) - 1] = '\0';
            return validate_range(value, min_buf, max_buf);
        }

    case VALIDATOR_TYPE_REGEX:
        if (!validator->pattern)
            return false;
        {
            const char *str_val = config_value_get_string(value, "");
            if (!str_val)
                return false;
            size_t pat_len = strlen(validator->pattern);
            size_t val_len = strlen(str_val);
            if (pat_len == 0)
                return true;
            if (pat_len == 1 && validator->pattern[0] == '*')
                return true;
            if (strstr(str_val, validator->pattern) != NULL)
                return true;
            if (pat_len > 1 && validator->pattern[0] == '^' &&
                validator->pattern[pat_len - 1] == '$') {
                char inner[256];
                if (pat_len - 2 < sizeof(inner)) {
                    __builtin_memcpy(inner, validator->pattern + 1, pat_len - 2);
                    inner[pat_len - 2] = '\0';
                    return strcmp(str_val, inner) == 0;
                }
            }
            return val_len > 0;
        }

    case VALIDATOR_TYPE_ENUM:
        return validate_enum(value, (const char **)validator->enum_values, validator->enum_count);

    case VALIDATOR_TYPE_CUSTOM:
        if (validator->custom_cb) {
            return validator->custom_cb(key, value, validator->user_data);
        }
        return false;

    default:
        return false;
    }
}

config_validator_t *config_validator_create_range(const char *min, const char *max)
{
    if (!min || !max) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    validator_options_t options = {.type = VALIDATOR_TYPE_RANGE,
                                   .pattern = NULL,
                                   .enum_values = NULL,
                                   .enum_count = 0,
                                   .custom_cb = NULL,
                                   .user_data = NULL};

    size_t pattern_len = strlen(min) + strlen(max) + 2;
    char *pattern = (char *)AIRY_MALLOC(pattern_len);
    if (!pattern) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    snprintf(pattern, pattern_len, "%s,%s", min, max);
    options.pattern = pattern;

    config_validator_t *validator = config_validator_create(&options);

    AIRY_FREE(pattern);
    return validator;
}

config_validator_t *config_validator_create_regex(const char *pattern)
{
    if (!pattern) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    validator_options_t options = {.type = VALIDATOR_TYPE_REGEX,
                                   .pattern = pattern,
                                   .enum_values = NULL,
                                   .enum_count = 0,
                                   .custom_cb = NULL,
                                   .user_data = NULL};

    return config_validator_create(&options);
}

config_validator_t *config_validator_create_enum(const char **values, size_t count)
{
    if (!values || count == 0) {
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    validator_options_t options = {.type = VALIDATOR_TYPE_ENUM,
                                   .pattern = NULL,
                                   .enum_values = values,
                                   .enum_count = count,
                                   .custom_cb = NULL,
                                   .user_data = NULL};

    return config_validator_create(&options);
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
    schema->error_capacity = 8;
    schema->errors = (char **)AIRY_CALLOC(schema->error_capacity, sizeof(char *));
    if (!schema->errors) {
        AIRY_FREE(schema->items);
        AIRY_FREE(schema->name);
        AIRY_FREE(schema);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    schema->error_count = 0;

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
        if (item->validator)
            config_validator_destroy(item->validator);
    }

    if (schema->items)
        AIRY_FREE(schema->items);

    clear_schema_errors(schema);
    if (schema->errors)
        AIRY_FREE(schema->errors);

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

    new_item->validator = item->validator;

    schema->count++;
    return CONFIG_SUCCESS;
}

bool config_schema_validate(config_schema_t *schema, const config_context_t *ctx, bool strict)
{
    if (!schema || !ctx)
        return false;

    clear_schema_errors(schema);
    bool valid = true;

    for (size_t i = 0; i < schema->count; i++) {
        schema_item_internal_t *item = &schema->items[i];

        const config_value_t *value = config_context_get(ctx, item->key);

        if (item->required && !value) {
            add_schema_error(schema, "Required configuration item '%s' is missing", item->key);
            valid = false;
            continue;
        }

        if (value) {
            if (!validate_value_type(value, item->type)) {
                add_schema_error(schema, "Configuration item '%s' has wrong type", item->key);
                valid = false;
            }

            if (item->validator && !config_validator_validate(item->validator, item->key, value)) {
                add_schema_error(schema, "Configuration item '%s' failed validation", item->key);
                valid = false;
            }
        }
    }

    if (strict) {
        const config_iterator_t *it = config_context_iterator(ctx);
        if (it) {
            config_iterator_reset(it);
            while (config_iterator_has_next(it)) {
                const char *key = config_iterator_next_key(it);
                bool found = false;
                for (size_t j = 0; j < schema->count; j++) {
                    if (strcmp(schema->items[j].key, key) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    add_schema_error(schema, "Configuration key '%s' is not defined in schema",
                                     key);
                    valid = false;
                }
            }
        }
    }

    return valid;
}

const char *config_schema_get_error(config_schema_t *schema, int index)
{
    if (!schema || index < 0 || (size_t)index >= schema->error_count) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    return schema->errors[index];
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
                    config_value_destroy(default_value);
                }
            }
        }
    }

    return CONFIG_SUCCESS;
}
