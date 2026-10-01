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
 * 2. Source adapter layer: adaptation of environment and memory sources
 * 3. Service layer: schema-driven default application and the config
 *    service lifecycle (create/load)
 *
 * @note Thread safety: all public interfaces are thread-safe
 *
 * Usage example:
 *   config_context_t *ctx = config_context_create("myapp");
 *   config_value_t *val = config_value_create_string("localhost");
 *   config_context_set(ctx, "database.host", val);
 *   // Loading from an in-memory source
 *   config_memory_source_options_t opts = {
 *       .data = yaml_text, .data_len = len, .format = "yaml"};
 *   config_source_t *source = config_source_create_memory(&opts);
 *   config_source_load(source, ctx);
 */

#ifndef AIRY_RT_CONFIG_UNIFIED_H
#define AIRY_RT_CONFIG_UNIFIED_H

#include "atomic_compat.h"


#include "core_config.h"


#include "config_source.h"


#include "config_service.h"


/**
 * @brief Safely get an integer config value
 * @param ctx Configuration context
 * @param key Configuration key
 * @param default_value Default value
 * @return Integer value
 */
#define CONFIG_GET_INT_SAFE(ctx, key, default_value)                    \
    __extension__({                                                     \
        const config_value_t *val = config_context_get(ctx, key);       \
        val ? config_value_get_int(val, default_value) : default_value; \
    })

#endif /* AIRY_RT_CONFIG_UNIFIED_H */
