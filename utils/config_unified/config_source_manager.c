// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_source_manager.c
 * @brief Unified config module - source manager and change monitoring.
 *
 * Implements the config source manager: source register/remove/find/
 * batch load, change watching and debounced poll notification, single
 * responsibility.
 */

#include "config_source.h"

#include "config_source_internal.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"
#include "platform.h" /* airy_time_ms 跨平台单调时钟 */

config_source_manager_t *config_source_manager_create(void)
{
    config_source_manager_t *manager =
        (config_source_manager_t *)AIRY_CALLOC(1, sizeof(config_source_manager_t));
    if (!manager)
        return NULL;

    manager->capacity = 16;
    manager->sources =
        (config_source_t **)AIRY_CALLOC(manager->capacity, sizeof(config_source_t *));
    if (!manager->sources) {
        AIRY_FREE(manager);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    manager->count = 0;
    manager->change_callback = NULL;
    manager->callback_user_data = NULL;
    manager->watching = false;
    manager->last_notify_time_ms = 0;
    manager->debounce_ms = 500;
    airy_mtx_init(&manager->internal_mutex);
    return manager;
}

void config_source_manager_destroy(config_source_manager_t *manager)
{
    if (!manager)
        return;

    for (size_t i = 0; i < manager->count; i++) {
        if (manager->sources[i]) {
            config_source_destroy(manager->sources[i]);
        }
    }

    if (manager->sources)
        AIRY_FREE(manager->sources);
    airy_mtx_destroy(&manager->internal_mutex);
    AIRY_FREE(manager);
}

config_error_t config_source_manager_add(config_source_manager_t *manager, config_source_t *source)
{
    if (!manager || !source)
        return CONFIG_ERROR_INVALID_ARG;

    if (manager->count >= manager->capacity) {
        size_t new_capacity = manager->capacity * 2;
        config_source_t **new_sources =
            (config_source_t **)AIRY_REALLOC(manager->sources,
                                             new_capacity * sizeof(config_source_t *));
        if (!new_sources)
            return CONFIG_ERROR_OUT_OF_MEMORY;

        manager->sources = new_sources;
        manager->capacity = new_capacity;
    }

    manager->sources[manager->count] = source;
    manager->count++;

    if (source->adapter && source->adapter->get_attributes) {
        const config_source_attr_t *attr = source->adapter->get_attributes(source);
        if (attr) {
        }
    }

    return CONFIG_SUCCESS;
}

config_error_t config_source_manager_remove(config_source_manager_t *manager,
                                            config_source_t *source)
{
    if (!manager || !source)
        return CONFIG_ERROR_INVALID_ARG;

    for (size_t i = 0; i < manager->count; i++) {
        if (manager->sources[i] == source) {
            for (size_t j = i; j < manager->count - 1; j++) {
                manager->sources[j] = manager->sources[j + 1];
            }
            manager->count--;
            manager->sources[manager->count] = NULL;
            return CONFIG_SUCCESS;
        }
    }

    return CONFIG_ERROR_NOT_FOUND;
}

config_source_t *config_source_manager_find(config_source_manager_t *manager, const char *name)
{
    if (!manager || !name)
        return NULL;

    for (size_t i = 0; i < manager->count; i++) {
        const config_source_attr_t *attr = config_source_get_attributes(manager->sources[i]);
        if (attr && attr->name && strcmp(attr->name, name) == 0) {
            return manager->sources[i];
        }
    }

    AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
}

config_error_t config_source_manager_load_all(config_source_manager_t *manager,
                                              config_context_t *ctx, int merge_strategy)
{
    if (!manager || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    config_error_t overall_error = CONFIG_SUCCESS;

    for (size_t i = 0; i < manager->count; i++) {
        config_source_t *source = manager->sources[i];
        if (!source)
            continue;

        config_error_t error = config_source_load(source, ctx);
        if (error != CONFIG_SUCCESS) {
            overall_error = error;
            if (merge_strategy == 0) {
                return error;
            }
        }
    }

    return overall_error;
}

config_error_t config_source_manager_watch(config_source_manager_t *manager,
                                           void (*callback)(config_source_t *source,
                                                            void *user_data),
                                           void *user_data)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    manager->change_callback = callback;
    manager->callback_user_data = user_data;
    manager->watching = (callback != NULL);

    return CONFIG_SUCCESS;
}

int config_source_manager_poll_changes(config_source_manager_t *manager)
{
    if (!manager)
        return 0;

    airy_mtx_lock(&manager->internal_mutex);

    if (!manager->watching || !manager->change_callback) {
        airy_mtx_unlock(&manager->internal_mutex);
        return 0;
    }

    uint64_t now_ms = airy_time_ms();

    if (manager->last_notify_time_ms > 0 &&
        now_ms - manager->last_notify_time_ms < manager->debounce_ms) {
        airy_mtx_unlock(&manager->internal_mutex);
        return 0;
    }

    int change_count = 0;
    int first_changed_idx = -1;

    for (size_t i = 0; i < manager->count; i++) {
        config_source_t *source = manager->sources[i];
        if (!source)
            continue;

        const config_source_attr_t *attr = config_source_get_attributes(source);
        if (!attr || !attr->watchable)
            continue;

        if (config_source_has_changed(source)) {
            change_count++;
            if (first_changed_idx < 0) {
                first_changed_idx = (int)i;
            }
        }
    }

    if (change_count > 0) {
        manager->last_notify_time_ms = now_ms;

        void (*cb)(config_source_t *, void *) = manager->change_callback;
        void *ud = manager->callback_user_data;

        airy_mtx_unlock(&manager->internal_mutex);

        for (size_t i = 0; i < manager->count; i++) {
            config_source_t *source = manager->sources[i];
            if (!source)
                continue;
            const config_source_attr_t *attr = config_source_get_attributes(source);
            if (!attr || !attr->watchable)
                continue;
            if (config_source_has_changed(source)) {
                cb(source, ud);
            }
        }

        return change_count;
    }

    airy_mtx_unlock(&manager->internal_mutex);
    return 0;
}
