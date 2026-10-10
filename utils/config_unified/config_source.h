/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_source.h
 * @brief Unified configuration module: source adapter-layer interface.
 *
 * The source adapter layer provides a unified adapter interface for
 * configuration sources. In-tree sources: environment variables and
 * in-memory data.
 */

#ifndef AIRY_RT_CONFIG_SOURCE_H
#define AIRY_RT_CONFIG_SOURCE_H

#include "core_config.h"

#include "airy_abi.h"

AIRY_ABI_BEGIN


typedef enum {
    CONFIG_SOURCE_FILE = 0,
    CONFIG_SOURCE_ENV = 1,
    CONFIG_SOURCE_ARGS = 2,
    CONFIG_SOURCE_MEMORY = 3,
    CONFIG_SOURCE_NETWORK = 4,
    CONFIG_SOURCE_DATABASE = 5,
    CONFIG_SOURCE_DEFAULT = 6
} config_source_type_t;


typedef struct config_source config_source_t;


typedef struct {
    config_source_type_t type;
    const char *name;
    int priority;
    bool read_only;
    bool watchable;
    uint64_t timestamp;
    uint32_t version;
} config_source_attr_t;


typedef struct {
    const char *prefix;
    bool case_sensitive;
    const char *separator;
    bool expand_vars;
} config_env_source_options_t;


typedef struct {
    const char *data;
    size_t data_len;
    const char *format;
} config_memory_source_options_t;


/**
 * @brief Configuration source adapter interface definition
 */
typedef struct {

    config_error_t (*load)(config_source_t *source, config_context_t *ctx);


    config_error_t (*save)(config_source_t *source, const config_context_t *ctx);


    bool (*has_changed)(config_source_t *source);


    const config_source_attr_t *(*get_attributes)(config_source_t *source);


    void (*destroy)(config_source_t *source);
} config_source_adapter_t;


/**
 * @brief Create an environment-variable configuration source
 * @param options Environment-variable configuration source options
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_env(const config_env_source_options_t *options);

/**
 * @brief Create a memory configuration source
 * @param options Memory configuration source options
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_memory(const config_memory_source_options_t *options);


/**
 * @brief Destroy a configuration source
 * @param source Configuration source object
 */
void config_source_destroy(config_source_t *source);

/**
 * @brief Load configuration from a source into a context
 * @param source Configuration source
 * @param ctx Configuration context
 * @return Error code
 */
config_error_t config_source_load(config_source_t *source, config_context_t *ctx);

/**
 * @brief Save a configuration context to a source
 * @param source Configuration source
 * @param ctx Configuration context
 * @return Error code
 */
config_error_t config_source_save(config_source_t *source, const config_context_t *ctx);

/**
 * @brief Check whether a configuration source has changed
 * @param source Configuration source
 * @return Whether it has changed
 */
bool config_source_has_changed(config_source_t *source);

/**
 * @brief Get configuration source attributes
 * @param source Configuration source
 * @return Configuration source attributes
 */
const config_source_attr_t *config_source_get_attributes(config_source_t *source);


/**
 * @brief Get a configuration source type description
 * @param type Configuration source type
 * @return Type description string
 */
const char *config_source_type_to_string(config_source_type_t type);

AIRY_ABI_END

#endif /* AIRY_RT_CONFIG_SOURCE_H */
