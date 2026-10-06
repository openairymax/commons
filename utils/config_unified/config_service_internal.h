/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_service_internal.h
 * @brief Unified config module - schema internal shared defs.
 *
 * Carries the internal schema contract shared between the schema
 * implementation (config_service_validator.c) and its callers.
 */

#ifndef AIRY_RT_CONFIG_SERVICE_INTERNAL_H
#define AIRY_RT_CONFIG_SERVICE_INTERNAL_H

#include "config_service.h"
#include "core_config_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *key;
    config_value_type_t type;
    bool required;
    char *description;
    char *default_value;
} schema_item_internal_t;

struct config_schema {
    char *name;
    schema_item_internal_t *items;
    size_t count;
    size_t capacity;
};

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CONFIG_SERVICE_INTERNAL_H */
