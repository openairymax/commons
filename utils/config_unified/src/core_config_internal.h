/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file core_config_internal.h
 * @brief Unified config module core layer - internal shared definitions.
 *
 * After core_config.c was split by functional domain, this header carries
 * the shared object layouts and cross-file helper declarations:
 *   - core_config.c          config context domain
 *   - core_config_value.c    config value object domain
 *   - core_config_strings.c  error/type stringification and debug dump
 */

#ifndef AIRY_RT_CORE_CONFIG_INTERNAL_H
#define AIRY_RT_CORE_CONFIG_INTERNAL_H

#include "core_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct config_value {

    config_value_type_t type;

    union {
        bool bool_value;
        int32_t int_value;
        int64_t int64_value;
        double double_value;
        struct {
            char *str;
            size_t len;
        } string_value;
        struct {
            config_value_t **items;
            size_t count;
            size_t capacity;
        } array_value;
        struct {
            struct {
                char *key;
                config_value_t *value;
            } *items;
            size_t count;
            size_t capacity;
        } object_value;
        struct {
            void *data;
            size_t size;
        } binary_value;
    } data;
};

struct config_context {
    char *name;

    struct {
        char *key;
        config_value_t *value;
    } *items;

    size_t count;

    size_t capacity;

    bool locked;

    airy_mtx_t mutex;

    config_schema_t *schema;
    bool hot_reload_enabled;
    uint32_t reload_interval_ms;
    bool encryption_enabled;
};

/* Shared helper: string duplicate via AIRY_MALLOC (defined in
 * core_config_value.c). */
char *duplicate_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CORE_CONFIG_INTERNAL_H */
