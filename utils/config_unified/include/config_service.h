/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_service.h
 * @brief Unified configuration module: service-layer interface.
 *
 * The service layer provides advanced configuration features:
 * 1. Configuration validation and schema definition
 * 2. Hot reload and change notification
 * 3. Configuration encryption and secure storage
 * 4. Configuration versioning and rollback
 * 5. Configuration templates and variable expansion
 */

#ifndef AIRY_RT_CONFIG_SERVICE_H
#define AIRY_RT_CONFIG_SERVICE_H

#include "config_source.h"
#include "core_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Configuration validator callback function
 * @param key Configuration key
 * @param value Configuration value
 * @param user_data User data
 * @return Validation result (true: valid, false: invalid)
 */
typedef bool (*config_validator_cb_t)(const char *key, const config_value_t *value,
                                      void *user_data);

/**
 * @brief Configuration validator
 */
typedef struct config_validator config_validator_t;

/**
 * @brief Validator types
 */
typedef enum {
    VALIDATOR_TYPE_RANGE = 0,
    VALIDATOR_TYPE_REGEX = 1,
    VALIDATOR_TYPE_ENUM = 2,
    VALIDATOR_TYPE_CUSTOM = 3
} validator_type_t;

/**
 * @brief Validator options
 */
typedef struct {
    validator_type_t type;
    const char *pattern;
    const char **enum_values;
    size_t enum_count;
    config_validator_cb_t custom_cb;
    void *user_data;
} validator_options_t;


/**
 * @brief Configuration schema item
 */
typedef struct {
    const char *key;
    config_value_type_t type;
    bool required;
    const char *description;
    const char *default_value;
    config_validator_t *validator;
} config_schema_item_t;

/**
 * @brief Configuration schema
 */
typedef struct config_schema config_schema_t;


/**
 * @brief Configuration change callback function
 * @param ctx Configuration context
 * @param key Changed configuration key (NULL for all changes)
 * @param old_value Old value (may be NULL)
 * @param new_value New value
 * @param user_data User data
 */
typedef void (*config_change_cb_t)(config_context_t *ctx, const char *key,
                                   const config_value_t *old_value, const config_value_t *new_value,
                                   void *user_data);

/**
 * @brief Hot-reload manager
 */
typedef struct config_hot_reload_manager config_hot_reload_manager_t;


/**
 * @brief Encryption algorithm types
 */
typedef enum {
    ENCRYPTION_NONE = 0,
    ENCRYPTION_AES_256_GCM = 1, /* AES-256-GCM */
    ENCRYPTION_CHACHA20_POLY1305 = 2 /* ChaCha20-Poly1305 */
} encryption_algorithm_t;

/**
 * @brief Encryption configuration
 */
typedef struct {
    encryption_algorithm_t algorithm;
    const char *key;
    size_t key_len;
    const char *iv;
    size_t iv_len;
} encryption_config_t;


/**
 * @brief Configuration version information
 */
typedef struct {
    uint32_t version;
    uint64_t timestamp;
    const char *author;
    const char *description;
    size_t change_count;
} config_version_info_t;

/**
 * @brief Version manager
 */
typedef struct config_version_manager config_version_manager_t;


/**
 * @brief Create a configuration validator
 * @param options Validator options
 * @return Validator object, NULL on failure
 */
config_validator_t *config_validator_create(const validator_options_t *options);

/**
 * @brief Destroy a configuration validator
 * @param validator Validator
 */
void config_validator_destroy(config_validator_t *validator);

/**
 * @brief Validate a configuration value
 * @param validator Validator
 * @param key Configuration key
 * @param value Configuration value
 * @return Validation result
 */
bool config_validator_validate(config_validator_t *validator, const char *key,
                               const config_value_t *value);

/**
 * @brief Create a range validator
 * @param min Minimum value (string form)
 * @param max Maximum value (string form)
 * @return Validator object, NULL on failure
 */
config_validator_t *config_validator_create_range(const char *min, const char *max);

/**
 * @brief Create a regular-expression validator
 * @param pattern Regular expression
 * @return Validator object, NULL on failure
 */
config_validator_t *config_validator_create_regex(const char *pattern);

/**
 * @brief Create an enum-value validator
 * @param values Enum value array
 * @param count Number of enum values
 * @return Validator object, NULL on failure
 */
config_validator_t *config_validator_create_enum(const char **values, size_t count);


/**
 * @brief Create a configuration schema
 * @param name Schema name
 * @return Schema object, NULL on failure
 */
config_schema_t *config_schema_create(const char *name);

/**
 * @brief Destroy a configuration schema
 * @param schema Schema object
 */
void config_schema_destroy(config_schema_t *schema);

/**
 * @brief Add a schema item
 * @param schema Schema object
 * @param item Schema item
 * @return Error code
 */
config_error_t config_schema_add_item(config_schema_t *schema, const config_schema_item_t *item);

/**
 * @brief Validate a configuration context against a schema
 * @param schema Schema object
 * @param ctx Configuration context
 * @param strict Whether strict mode (checks for extra items)
 * @return Validation result (true: valid, false: invalid)
 */
bool config_schema_validate(config_schema_t *schema, const config_context_t *ctx, bool strict);

/**
 * @brief Get a schema validation error message
 * @param schema Schema object
 * @param index Error index
 * @return Error message, NULL if no error
 */
const char *config_schema_get_error(config_schema_t *schema, int index);

/**
 * @brief Apply schema defaults to a configuration context
 * @param schema Schema object
 * @param ctx Configuration context
 * @return Error code
 */
config_error_t config_schema_apply_defaults(config_schema_t *schema, config_context_t *ctx);


/**
 * @brief Create a hot-reload manager
 * @param ctx Configuration context
 * @param source_manager Configuration source manager
 * @return Hot-reload manager, NULL on failure
 */
config_hot_reload_manager_t *config_hot_reload_manager_create(
    config_context_t *ctx, config_source_manager_t *source_manager);

/**
 * @brief Destroy a hot-reload manager
 * @param manager Hot-reload manager
 */
void config_hot_reload_manager_destroy(config_hot_reload_manager_t *manager);

/**
 * @brief Register a configuration change callback
 * @param manager Hot-reload manager
 * @param key Configuration key (NULL listens for all changes)
 * @param callback Callback function
 * @param user_data User data
 * @return Error code
 */
config_error_t config_hot_reload_register_callback(config_hot_reload_manager_t *manager,
                                                   const char *key, config_change_cb_t callback,
                                                   void *user_data);

/**
 * @brief Start monitoring configuration changes
 * @param manager Hot-reload manager
 * @param check_interval_ms Check interval (ms)
 * @return Error code
 */
config_error_t config_hot_reload_start(config_hot_reload_manager_t *manager,
                                       uint32_t check_interval_ms);

/**
 * @brief Stop monitoring configuration changes
 * @param manager Hot-reload manager
 * @return Error code
 */
config_error_t config_hot_reload_stop(config_hot_reload_manager_t *manager);

/**
 * @brief Manually trigger a configuration reload
 * @param manager Hot-reload manager
 * @return Error code
 */
config_error_t config_hot_reload_trigger(config_hot_reload_manager_t *manager);


/**
 * @brief Encrypt a configuration value
 * @param value Configuration value
 * @param manager Encryption configuration
 * @return Encrypted configuration value, NULL on failure
 */
config_value_t *config_encrypt_value(const config_value_t *value,
                                     const encryption_config_t *manager);

/**
 * @brief Decrypt a configuration value
 * @param encrypted_value Encrypted configuration value
 * @param manager Encryption configuration
 * @return Decrypted configuration value, NULL on failure
 */
config_value_t *config_decrypt_value(const config_value_t *encrypted_value,
                                     const encryption_config_t *manager);

/**
 * @brief Create an encrypted configuration source wrapper
 * @param source Original configuration source
 * @param manager Encryption configuration
 * @return Encrypted configuration source, NULL on failure
 */
config_source_t *config_source_create_encrypted(config_source_t *source,
                                                const encryption_config_t *manager);


/**
 * @brief Create a configuration version manager
 * @param ctx Configuration context
 * @param max_versions Maximum retained versions
 * @return Version manager, NULL on failure
 */
config_version_manager_t *config_version_manager_create(config_context_t *ctx, size_t max_versions);

/**
 * @brief Destroy a configuration version manager
 * @param manager Version manager
 */
void config_version_manager_destroy(config_version_manager_t *manager);

/**
 * @brief Create a configuration snapshot (new version)
 * @param manager Version manager
 * @param author Author
 * @param description Description
 * @return Version number, 0 on failure
 */
uint32_t config_version_create_snapshot(config_version_manager_t *manager, const char *author,
                                        const char *description);

/**
 * @brief Roll back to a specific version
 * @param manager Version manager
 * @param version Version number
 * @return Error code
 */
config_error_t config_version_rollback(config_version_manager_t *manager, uint32_t version);

/**
 * @brief Get the version list
 * @param manager Version manager
 * @param versions Version information array (output)
 * @param max_count Maximum count
 * @return Actual number of versions returned
 */
size_t config_version_get_list(config_version_manager_t *manager, config_version_info_t *versions,
                               size_t max_count);

/**
 * @brief Get the diff between two versions
 * @param manager Version manager
 * @param version1 Version 1
 * @param version2 Version 2
 * @param diff Diff output buffer
 * @param diff_size Buffer size
 * @return Diff size
 */
size_t config_version_get_diff(config_version_manager_t *manager, uint32_t version1,
                               uint32_t version2, char *diff, size_t diff_size);


/**
 * @brief Expand configuration template variables
 * @param ctx Configuration context
 * @param template_str Template string
 * @param result Result output buffer
 * @param result_size Buffer size
 * @return Error code
 */
config_error_t config_expand_template(config_context_t *ctx, const char *template_str, char *result,
                                      size_t result_size);

/**
 * @brief Apply a configuration template to a context
 * @param ctx Configuration context
 * @param template_ctx Template configuration context
 * @return Error code
 */
config_error_t config_apply_template(config_context_t *ctx, config_context_t *template_ctx);


/**
 * @brief Create a complete configuration service
 * @param service_name Service name
 * @param schema Configuration schema (may be NULL)
 * @param enable_hot_reload Whether to enable hot reload
 * @param enable_encryption Whether to enable encryption
 * @return Configuration service context, NULL on failure
 */
config_context_t *config_service_create(const char *service_name, config_schema_t *schema,
                                        bool enable_hot_reload, bool enable_encryption);

/**
 * @brief Load a configuration service
 * @param ctx Configuration service context
 * @param sources Configuration source array
 * @param source_count Number of configuration sources
 * @return Error code
 */
config_error_t config_service_load(config_context_t *ctx, config_source_t **sources,
                                   size_t source_count);

/**
 * @brief Save a configuration service
 * @param ctx Configuration service context
 * @param primary_source Primary configuration source
 * @return Error code
 */
config_error_t config_service_save(config_context_t *ctx, config_source_t *primary_source);

/**
 * @brief Get the configuration service status
 * @param ctx Configuration service context
 * @param status_json Status JSON output buffer
 * @param status_size Buffer size
 * @return Error code
 */
config_error_t config_service_get_status(config_context_t *ctx, char *status_json,
                                         size_t status_size);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CONFIG_SERVICE_H */
