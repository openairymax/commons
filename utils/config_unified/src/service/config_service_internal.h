/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_service_internal.h
 * @brief Unified config module - service layer internal shared defs.
 *
 * After config_service.c was split by functional domain, this header
 * carries the shared contract between the pieces:
 *   - config_service.c           template expansion and service lifecycle
 *   - config_service_validator.c validator and Schema definitions
 *   - config_service_hotreload.c hot reload and change notification
 *   - config_service_crypto.c    encryption and secure storage
 *   - config_service_version.c   version management and rollback
 */

#ifndef AIRY_RT_CONFIG_SERVICE_INTERNAL_H
#define AIRY_RT_CONFIG_SERVICE_INTERNAL_H

#include "config_service.h"

#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INDEX_NOT_FOUND (-1)

struct config_validator {
    validator_type_t type;
    char *pattern;
    char **enum_values;
    size_t enum_count;
    config_validator_cb_t custom_cb;
    void *user_data;
    char *error_message;
};

typedef struct {
    char *key;
    config_value_type_t type;
    bool required;
    char *description;
    char *default_value;
    config_validator_t *validator;
} schema_item_internal_t;

struct config_schema {
    char *name;
    schema_item_internal_t *items;
    size_t count;
    size_t capacity;
    char **errors;
    size_t error_count;
    size_t error_capacity;
};

typedef struct {
    char *key;
    config_change_cb_t callback;
    void *user_data;
} change_callback_item_t;

struct config_hot_reload_manager {
    config_context_t *ctx;
    config_source_manager_t *source_manager;
    change_callback_item_t *callbacks;
    size_t callback_count;
    size_t callback_capacity;
    volatile bool running;
    uint32_t check_interval_ms;
    uint32_t debounce_ms;
    uint64_t last_trigger_time_ms;
    void *thread_handle;
    void *lock;
};

typedef struct {
    uint32_t version;
    uint64_t timestamp;
    char *author;
    char *description;
    config_context_t *snapshot;
} config_version_item_t;

struct config_version_manager {
    config_context_t *ctx;
    config_version_item_t *versions;
    size_t count;
    size_t capacity;
    size_t max_versions;
    uint32_t next_version;
};

static inline char *duplicate_string(const char *str)
{
    if (!str) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    size_t len = strlen(str);
    char *copy = (char *)AIRY_MALLOC(len + 1);
    if (copy) {
        __builtin_memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CONFIG_SERVICE_INTERNAL_H */
