// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file core_config_strings.c
 * @brief Unified config module - error/type stringification and debug dump.
 *
 * Implements error-code/type-name lookup tables and the recursive debug
 * printer for config values, single responsibility. Split out of
 * core_config.c.
 */

#include "core_config.h"

#include "core_config_internal.h"

#include "logging.h"

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
