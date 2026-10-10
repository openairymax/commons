/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_service.h
 * @brief Unified configuration module: service-layer interface.
 *
 * The service layer provides schema-driven default value application
 * and the config service lifecycle (create/load).
 */

#ifndef AIRY_RT_CONFIG_SERVICE_H
#define AIRY_RT_CONFIG_SERVICE_H

#include "config_source.h"
#include "core_config.h"

#include "airy_abi.h"

AIRY_ABI_BEGIN

/**
 * @brief Configuration schema item
 */
typedef struct {
    const char *key;
    config_value_type_t type;
    bool required;
    const char *description;
    const char *default_value;
} config_schema_item_t;

/**
 * @brief Configuration schema
 */
typedef struct config_schema config_schema_t;


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
 * @brief Apply schema defaults to a configuration context
 * @param schema Schema object
 * @param ctx Configuration context
 * @return Error code
 */
config_error_t config_schema_apply_defaults(config_schema_t *schema, config_context_t *ctx);


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

AIRY_ABI_END

#endif /* AIRY_RT_CONFIG_SERVICE_H */
