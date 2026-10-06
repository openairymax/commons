/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_source_internal.h
 * @brief Unified config module - source adapter internal shared defs.
 *
 * Carries the shared contract between the source pieces:
 *   - config_source.c          base class and common API
 *   - config_source_env.c      environment config source
 *   - config_source_memory.c   memory/default config source
 */

#ifndef AIRY_RT_CONFIG_SOURCE_INTERNAL_H
#define AIRY_RT_CONFIG_SOURCE_INTERNAL_H

#include "config_source.h"
#include "core_config_internal.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Format parsing layer (config_parse.c): pure string parsing of JSON/INI/YAML */
config_error_t config_parse_json(const char *data, size_t data_len, config_context_t *ctx);
config_error_t config_parse_ini(const char *data, size_t data_len, config_context_t *ctx);
config_error_t config_parse_yaml(const char *data, size_t data_len, config_context_t *ctx);

struct config_source {

    const config_source_adapter_t *adapter;

    void *priv_data;

    config_source_attr_t attributes;
};

typedef struct {
    char *prefix;
    bool case_sensitive;
    char *separator;
    bool expand_vars;
    char **env_keys;
    size_t env_count;
    uint64_t env_hash;
} env_source_priv_t;

typedef struct {
    char *data;
    size_t data_len;
    char *format;
    bool owns_data;
} memory_source_priv_t;

config_source_t *config_source_create_base(config_source_type_t type, const char *name,
                                           const config_source_adapter_t *adapter);
void config_source_free_base(config_source_t *source);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CONFIG_SOURCE_INTERNAL_H */
