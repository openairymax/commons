/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_source.h
 * @brief Unified configuration module: source adapter-layer interface.
 *
 * The source adapter layer provides a unified adapter interface for
 * different configuration sources. Supports multiple sources: file,
 * environment variables, command-line arguments, memory, network, etc.
 */

#ifndef AIRY_RT_CONFIG_SOURCE_H
#define AIRY_RT_CONFIG_SOURCE_H

#include "core_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


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
    const char *file_path;
    const char *format;
    const char *encoding;
    bool auto_reload;
    uint32_t reload_interval_ms;
} config_file_source_options_t;


typedef struct {
    const char *prefix;
    bool case_sensitive;
    const char *separator;
    bool expand_vars;
} config_env_source_options_t;


typedef struct {
    int argc;
    char **argv;
    const char *prefix;
    const char *assign_char;
    bool allow_positional;
} config_args_source_options_t;


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
 * @brief Create a file configuration source
 * @param options File configuration source options
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_file(const config_file_source_options_t *options);

/**
 * @brief Create an environment-variable configuration source
 * @param options Environment-variable configuration source options
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_env(const config_env_source_options_t *options);

/**
 * @brief Create a command-line configuration source
 * @param options Command-line configuration source options
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_args(const config_args_source_options_t *options);

/**
 * @brief Create a memory configuration source
 * @param options Memory configuration source options
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_memory(const config_memory_source_options_t *options);

/**
 * @brief Create a default-value configuration source
 * @param default_values Default value map (key-value array)
 * @param count Number of key-value pairs
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_defaults(const char *const *default_values, size_t count);

/**
 * @brief Create a remote configuration source
 * @param url Configuration center URL
 * @param token Authentication token (may be NULL)
 * @param ns Namespace (may be NULL)
 * @param poll_interval_ms Poll interval in ms (0 for the default 30000ms)
 * @return Configuration source object, NULL on failure
 */
config_source_t *config_source_create_remote(const char *url, const char *token, const char *ns,
                                             uint32_t poll_interval_ms);


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
 * @brief Get the configuration source type
 * @param source Configuration source
 * @return Configuration source type
 */
config_source_type_t config_source_get_type(config_source_t *source);


typedef struct config_source_manager config_source_manager_t;

/**
 * @brief Create a configuration source manager
 * @return Configuration source manager, NULL on failure
 */
config_source_manager_t *config_source_manager_create(void);

/**
 * @brief Destroy a configuration source manager
 * @param manager Configuration source manager
 */
void config_source_manager_destroy(config_source_manager_t *manager);

/**
 * @brief Add a configuration source to the manager
 * @param manager Configuration source manager
 * @param source Configuration source
 * @return Error code
 */
config_error_t config_source_manager_add(config_source_manager_t *manager, config_source_t *source);

/**
 * @brief Remove a configuration source from the manager
 * @param manager Configuration source manager
 * @param source Configuration source
 * @return Error code
 */
config_error_t config_source_manager_remove(config_source_manager_t *manager,
                                            config_source_t *source);

/**
 * @brief Find a configuration source by name
 * @param manager Configuration source manager
 * @param name Configuration source name
 * @return Configuration source, NULL if not found
 */
config_source_t *config_source_manager_find(config_source_manager_t *manager, const char *name);

/**
 * @brief Load configuration from all sources
 * @param manager Configuration source manager
 * @param ctx Configuration context
 * @param merge_strategy Merge strategy (0: overwrite, 1: merge,
 *                      2: smart merge)
 * @return Error code
 */
config_error_t config_source_manager_load_all(config_source_manager_t *manager,
                                              config_context_t *ctx, int merge_strategy);

/**
 * @brief Watch for configuration source changes
 * @param manager Configuration source manager
 * @param callback Change callback function
 * @param user_data User data
 * @return Error code
 */
config_error_t config_source_manager_watch(config_source_manager_t *manager,
                                           void (*callback)(config_source_t *source,
                                                            void *user_data),
                                           void *user_data);

/**
 * @brief Poll all watchable sources for changes and notify callbacks
 *
 * Checks whether any watchable source has changed. If a change is
 * detected, invokes the registered callbacks for each changed source
 * after the debounce interval. The default debounce is 500ms, i.e.
 * multiple changes within 500ms are merged into one notification.
 *
 * @param manager Configuration source manager
 * @return Number of changed sources, 0 for no changes, -1 on error
 */
int config_source_manager_poll_changes(config_source_manager_t *manager);


/**
 * @brief Get a configuration source type description
 * @param type Configuration source type
 * @return Type description string
 */
const char *config_source_type_to_string(config_source_type_t type);

/**
 * @brief Parse a configuration file format
 * @param file_path File path
 * @return File format string, "unknown" for unknown formats
 */
const char *config_parse_file_format(const char *file_path);

/**
 * @brief Create a configuration source name
 * @param type Configuration source type
 * @param identifier Identifier (e.g. file path, env prefix, etc.)
 * @return Configuration source name string (caller frees)
 */
char *config_source_create_name(config_source_type_t type, const char *identifier);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CONFIG_SOURCE_H */
