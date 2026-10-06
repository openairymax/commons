/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file core_config.h
 * @brief Unified configuration module: core-layer interface.
 *
 * Core layer of the unified configuration module, providing a unified
 * configuration data model and basic interfaces.
 * Design principles:
 * 1. Unified configuration data model supporting multiple data types
 * 2. Type-safe configuration access interfaces
 * 3. Explicit memory ownership, avoiding memory leaks
 * 4. Thread-safe basic operations
 */

#ifndef AIRY_RT_CORE_CONFIG_H
#define AIRY_RT_CORE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    CONFIG_TYPE_NULL = 0,
    CONFIG_TYPE_BOOL = 1,
    CONFIG_TYPE_INT = 2,
    CONFIG_TYPE_INT64 = 3,
    CONFIG_TYPE_DOUBLE = 4,
    CONFIG_TYPE_STRING = 5
} config_value_type_t;


typedef struct config_value config_value_t;


typedef enum {
    CONFIG_SUCCESS = 0,
    CONFIG_ERROR_INVALID_ARG = 1,
    CONFIG_ERROR_NOT_FOUND = 2,
    CONFIG_ERROR_TYPE_MISMATCH = 3,
    CONFIG_ERROR_OUT_OF_MEMORY = 4,
    CONFIG_ERROR_IO = 5,
    CONFIG_ERROR_PARSE = 6,
    CONFIG_ERROR_VALIDATION = 7,
    CONFIG_ERROR_UNSUPPORTED = 8,
    CONFIG_ERROR_THREAD = 9
} config_error_t;


typedef struct config_context config_context_t;


/**
 * @brief Create a null config value
 * @return Config value object, NULL on failure
 */
config_value_t *config_value_create_null(void);

/**
 * @brief Create a boolean config value
 * @param value Boolean value
 * @return Config value object, NULL on failure
 */
config_value_t *config_value_create_bool(bool value);

/**
 * @brief Create an integer config value
 * @param value Integer value
 * @return Config value object, NULL on failure
 */
config_value_t *config_value_create_int(int32_t value);

/**
 * @brief Create a 64-bit integer config value
 * @param value 64-bit integer value
 * @return Config value object, NULL on failure
 */
config_value_t *config_value_create_int64(int64_t value);

/**
 * @brief Create a double config value
 * @param value Double value
 * @return Config value object, NULL on failure
 */
config_value_t *config_value_create_double(double value);

/**
 * @brief Create a string config value
 * @param value String value (copied)
 * @return Config value object, NULL on failure
 */
config_value_t *config_value_create_string(const char *value);

/**
 * @brief Clone a config value
 * @param value Source config value
 * @return New config value copy, NULL on failure
 */
config_value_t *config_value_clone(const config_value_t *value);

/**
 * @brief Destroy a config value
 * @param value Config value object
 */
void config_value_destroy(config_value_t *value);

/**
 * @brief Get a config value's type
 * @param value Config value
 * @return Config value type
 */
config_value_type_t config_value_get_type(const config_value_t *value);

/**
 * @brief Get the boolean value
 * @param value Config value
 * @param default_value Default (returned on type mismatch or NULL)
 * @return Boolean value
 */
bool config_value_get_bool(const config_value_t *value, bool default_value);

/**
 * @brief Get the integer value
 * @param value Config value
 * @param default_value Default (returned on type mismatch or NULL)
 * @return Integer value
 */
int32_t config_value_get_int(const config_value_t *value, int32_t default_value);

/**
 * @brief Get the 64-bit integer value
 * @param value Config value
 * @param default_value Default (returned on type mismatch or NULL)
 * @return 64-bit integer value
 */
int64_t config_value_get_int64(const config_value_t *value, int64_t default_value);

/**
 * @brief Get the double value
 * @param value Config value
 * @param default_value Default (returned on type mismatch or NULL)
 * @return Double value
 */
double config_value_get_double(const config_value_t *value, double default_value);

/**
 * @brief Get the string value
 * @param value Config value
 * @param default_value Default (returned on type mismatch or NULL)
 * @return String pointer (internally owned, do not free)
 */
const char *config_value_get_string(const config_value_t *value, const char *default_value);

/**
 * @brief Create a configuration context
 * @param name Context name (for debugging and logging)
 * @return Configuration context, NULL on failure
 */
config_context_t *config_context_create(const char *name);

/**
 * @brief Destroy a configuration context
 * @param ctx Configuration context
 */
void config_context_destroy(config_context_t *ctx);

/**
 * @brief Set a config value
 * @param ctx Configuration context
 * @param key Configuration key (dot format, e.g. "database.host")
 * @param value Config value (ownership transferred to the context)
 * @return Error code
 */
config_error_t config_context_set(config_context_t *ctx, const char *key, config_value_t *value);

/**
 * @brief Get a config value
 * @param ctx Configuration context
 * @param key Configuration key
 * @return Config value (internally owned, do not free or modify), NULL if
 *         absent
 */
const config_value_t *config_context_get(const config_context_t *ctx, const char *key);

/**
 * @brief Check whether a config entry exists
 * @param ctx Configuration context
 * @param key Configuration key
 * @return Whether it exists
 */
bool config_context_has(const config_context_t *ctx, const char *key);

/**
 * @brief Clear all config entries
 * @param ctx Configuration context
 */
void config_context_clear(config_context_t *ctx);

/**
 * @brief Get the number of config entries
 * @param ctx Configuration context
 * @return Number of config entries
 */
size_t config_context_count(const config_context_t *ctx);

config_context_t *config_context_clone(const config_context_t *ctx);

config_error_t config_context_copy(config_context_t *dst, const config_context_t *src);

typedef struct config_schema config_schema_t;

void config_context_set_schema(config_context_t *ctx, config_schema_t *schema);
void config_context_set_hot_reload(config_context_t *ctx, bool enabled, uint32_t interval_ms);
void config_context_set_encryption(config_context_t *ctx, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CORE_CONFIG_H */
