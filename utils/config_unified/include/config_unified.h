/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file config_unified.h
 * @brief Unified configuration module: main header.
 *
 * Main header of the unified configuration module; includes the headers
 * of all submodules. Provides unified layered configuration management:
 * 1. Core layer: unified configuration data model and basic interfaces
 * 2. Source adapter layer: unified adaptation of multiple configuration
 *    sources
 * 3. Service layer: advanced features (validation, hot reload,
 *    encryption, etc.)
 * 4. Compatibility layer: backward compatibility with existing
 *    configuration APIs
 *
 * @note Thread safety: all public interfaces are thread-safe
 * @see ARCHITECTURAL_PRINCIPLES.md E-2 observability principle
 *
 * Usage example:
 *   config_context_t *ctx = config_context_create("myapp");
 *   config_value_t *val = config_value_create_string("localhost");
 *   config_context_set(ctx, "database.host", val);
 *   // Using a source adapter
 *   config_file_source_options_t file_opts = {.file_path = "manager.yaml", .format = "yaml"};
 *   config_source_t *source = config_source_create_file(&file_opts);
 *   config_source_load(source, ctx);
 *
 *   config_hot_reload_manager_t *hot_reload = config_hot_reload_manager_create(ctx, NULL);
 *   config_hot_reload_start(hot_reload, 5000);
 */

#ifndef AIRY_RT_CONFIG_UNIFIED_H
#define AIRY_RT_CONFIG_UNIFIED_H

#include "atomic_compat.h"


#include "core_config.h"


#include "config_source.h"


#include "config_service.h"


/**
 * @brief Simplified macro for creating a string config value
 * @param value String value
 * @return Config value object
 */
#define CONFIG_STRING(value) config_value_create_string(value)

/**
 * @brief Simplified macro for creating an integer config value
 * @param value Integer value
 * @return Config value object
 */
#define CONFIG_INT(value) config_value_create_int(value)

/**
 * @brief Simplified macro for creating a boolean config value
 * @param value Boolean value
 * @return Config value object
 */
#define CONFIG_BOOL(value) config_value_create_bool(value)

/**
 * @brief Simplified macro for creating a double config value
 * @param value Double value
 * @return Config value object
 */
#define CONFIG_DOUBLE(value) config_value_create_double(value)

/**
 * @brief Safely set a config value (auto-destroys the old value)
 * @param ctx Configuration context
 * @param key Configuration key
 * @param value Config value (ownership transferred)
 */
#define CONFIG_SET_SAFE(ctx, key, value)                          \
    do {                                                          \
        const config_value_t *old = config_context_get(ctx, key); \
        if (old) {                                                \
            config_value_t *old_mutable = (config_value_t *)old;  \
            config_context_delete(ctx, key);                      \
            config_value_destroy(old_mutable);                    \
        }                                                         \
        config_context_set(ctx, key, value);                      \
    } while (0)

/**
 * @brief Safely get a string config value
 * @param ctx Configuration context
 * @param key Configuration key
 * @param default_value Default value
 * @return Config value
 */
#define CONFIG_GET_STRING_SAFE(ctx, key, default_value)                    \
    __extension__({                                                        \
        const config_value_t *val = config_context_get(ctx, key);          \
        val ? config_value_get_string(val, default_value) : default_value; \
    })

/**
 * @brief Safely get an integer config value
 * @param ctx Configuration context
 * @param key Configuration key
 * @param default_value Default value
 * @return Config value
 */
#define CONFIG_GET_INT_SAFE(ctx, key, default_value)                    \
    __extension__({                                                     \
        const config_value_t *val = config_context_get(ctx, key);       \
        val ? config_value_get_int(val, default_value) : default_value; \
    })

/**
 * @brief Safely get a boolean config value
 * @param ctx Configuration context
 * @param key Configuration key
 * @param default_value Default value
 * @return Config value
 */
#define CONFIG_GET_BOOL_SAFE(ctx, key, default_value)                    \
    __extension__({                                                      \
        const config_value_t *val = config_context_get(ctx, key);        \
        val ? config_value_get_bool(val, default_value) : default_value; \
    })

/**
 * @brief Safely get a double config value
 * @param ctx Configuration context
 * @param key Configuration key
 * @param default_value Default value
 * @return Config value
 */
#define CONFIG_GET_DOUBLE_SAFE(ctx, key, default_value)                    \
    __extension__({                                                        \
        const config_value_t *val = config_context_get(ctx, key);          \
        val ? config_value_get_double(val, default_value) : default_value; \
    })


/**
 * @brief Build a configuration path
 * @param base Base path
 * @param key Sub-key
 * @return Full path string
 */
#define CONFIG_PATH(base, key) (base "." key)

/**
 * @brief Build a database configuration path
 * @param key Sub-key
 * @return Full path string
 */
#define CONFIG_DB_PATH(key) CONFIG_PATH("database", key)

/**
 * @brief Build a logging configuration path
 * @param key Sub-key
 * @return Full path string
 */
#define CONFIG_LOG_PATH(key) CONFIG_PATH("logging", key)

/**
 * @brief Build a network configuration path
 * @param key Sub-key
 * @return Full path string
 */
#define CONFIG_NETWORK_PATH(key) CONFIG_PATH("network", key)

/**
 * @brief Build a security configuration path
 * @param key Sub-key
 * @return Full path string
 */
#define CONFIG_SECURITY_PATH(key) CONFIG_PATH("security", key)


/**
 * @brief Validate that a config value is a valid string
 * @param value Config value
 * @return Whether valid
 */
#define CONFIG_VALID_STRING(value) (value && config_value_get_type(value) == CONFIG_TYPE_STRING)

/**
 * @brief Validate that a config value is a valid integer
 * @param value Config value
 * @return Whether valid
 */
#define CONFIG_VALID_INT(value) (value && config_value_get_type(value) == CONFIG_TYPE_INT)

/**
 * @brief Validate that a config value is a valid boolean
 * @param value Config value
 * @return Whether valid
 */
#define CONFIG_VALID_BOOL(value) (value && config_value_get_type(value) == CONFIG_TYPE_BOOL)

/**
 * @brief Validate that a config value is a valid double
 * @param value Config value
 * @return Whether valid
 */
#define CONFIG_VALID_DOUBLE(value) (value && config_value_get_type(value) == CONFIG_TYPE_DOUBLE)


/**
 * @brief Check whether a config operation succeeded
 * @param error Error code
 * @return Whether successful
 */
#define CONFIG_SUCCESS(error) ((error) == CONFIG_SUCCESS)

/**
 * @brief Check whether a config operation failed
 * @param error Error code
 * @return Whether failed
 */
#define CONFIG_FAILED(error) ((error) != CONFIG_SUCCESS)

/**
 * @brief Return if a config operation failed
 * @param error Error code
 */
#define CONFIG_RETURN_IF_FAILED(error) \
    do {                               \
        if (CONFIG_FAILED(error)) {    \
            return error;              \
        }                              \
    } while (0)

/**
 * @brief Jump to a label if a config operation failed
 * @param error Error code
 * @param label Jump label
 */
#define CONFIG_GOTO_IF_FAILED(error, label) \
    do {                                    \
        if (CONFIG_FAILED(error)) {         \
            goto label;                     \
        }                                   \
    } while (0)


/**
 * @brief Initialize a config context and set defaults
 * @param ctx_name Context name
 * @param defaults Default value array
 * @param count Number of defaults
 * @return Configuration context
 */
#define CONFIG_INIT_WITH_DEFAULTS(ctx_name, defaults, count)                         \
    __extension__({                                                                  \
        config_context_t *ctx = config_context_create(ctx_name);                     \
        if (ctx) {                                                                   \
            for (size_t i = 0; i < (count); i += 2) {                                \
                config_value_t *val = config_value_create_string((defaults)[i + 1]); \
                if (val) {                                                           \
                    config_context_set(ctx, (defaults)[i], val);                     \
                }                                                                    \
            }                                                                        \
        }                                                                            \
        ctx;                                                                         \
    })


/**
 * @brief Safely convert a config value to a string
 * @param value Config value
 * @param default_value Default value
 * @return String value
 */
#define CONFIG_AS_STRING(value, default_value)            \
    (config_value_get_type(value) == CONFIG_TYPE_STRING ? \
         config_value_get_string(value, default_value) :  \
         default_value)

/**
 * @brief Safely convert a config value to an integer
 * @param value Config value
 * @param default_value Default value
 * @return Integer value
 */
#define CONFIG_AS_INT(value, default_value)            \
    (config_value_get_type(value) == CONFIG_TYPE_INT ? \
         config_value_get_int(value, default_value) :  \
         default_value)

/**
 * @brief Safely convert a config value to a boolean
 * @param value Config value
 * @param default_value Default value
 * @return Boolean value
 */
#define CONFIG_AS_BOOL(value, default_value)            \
    (config_value_get_type(value) == CONFIG_TYPE_BOOL ? \
         config_value_get_bool(value, default_value) :  \
         default_value)

/**
 * @brief Safely convert a config value to a double
 * @param value Config value
 * @param default_value Default value
 * @return Double value
 */
#define CONFIG_AS_DOUBLE(value, default_value)            \
    (config_value_get_type(value) == CONFIG_TYPE_DOUBLE ? \
         config_value_get_double(value, default_value) :  \
         default_value)

#endif /* AIRY_RT_CONFIG_UNIFIED_H */
