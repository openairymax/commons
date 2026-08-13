// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_service_hotreload.c
 * @brief Unified config module - hot reload and change notification.
 *
 * Implements the config hot-reload manager: background monitoring thread,
 * change callback registration, debounced triggering and resource
 * management, single responsibility.
 */

#include "config_service.h"

#include "config_service_internal.h"

#include <platform.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <string.h>
#include <time.h>
#include "error.h"

config_hot_reload_manager_t *config_hot_reload_manager_create(
    config_context_t *ctx, config_source_manager_t *source_manager)
{
    if (!ctx || !source_manager) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    config_hot_reload_manager_t *manager =
        (config_hot_reload_manager_t *)AIRY_CALLOC(1, sizeof(config_hot_reload_manager_t));
    if (!manager) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    manager->ctx = ctx;
    manager->source_manager = source_manager;
    manager->running = false;
    manager->check_interval_ms = 5000;
    manager->debounce_ms = 500;
    manager->last_trigger_time_ms = 0;

    manager->callback_capacity = 8;
    manager->callbacks = (change_callback_item_t *)AIRY_CALLOC(manager->callback_capacity,
                                                               sizeof(change_callback_item_t));
    if (!manager->callbacks) {
        AIRY_FREE(manager);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    manager->callback_count = 0;
    manager->thread_handle = NULL;
    manager->lock = (void *)AIRY_CALLOC(1, sizeof(airy_mtx_t));
    if (!manager->lock) {
        AIRY_FREE(manager->callbacks);
        AIRY_FREE(manager);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    airy_mtx_init((airy_mtx_t *)manager->lock);

    return manager;
}

void config_hot_reload_manager_destroy(config_hot_reload_manager_t *manager)
{
    if (!manager)
        return;

    config_hot_reload_stop(manager);

    for (size_t i = 0; i < manager->callback_count; i++) {
        change_callback_item_t *cb = &manager->callbacks[i];
        if (cb->key)
            AIRY_FREE(cb->key);
    }

    if (manager->callbacks)
        AIRY_FREE(manager->callbacks);
    if (manager->lock) {
        airy_mtx_destroy((airy_mtx_t *)manager->lock);
        AIRY_FREE(manager->lock);
    }
    AIRY_FREE(manager);
}

config_error_t config_hot_reload_register_callback(config_hot_reload_manager_t *manager,
                                                   const char *key, config_change_cb_t callback,
                                                   void *user_data)
{
    if (!manager || !callback)
        return CONFIG_ERROR_INVALID_ARG;

    if (manager->callback_count >= manager->callback_capacity) {
        size_t new_capacity = manager->callback_capacity * 2;
        change_callback_item_t *new_callbacks =
            (change_callback_item_t *)AIRY_REALLOC(manager->callbacks,
                                                   new_capacity * sizeof(change_callback_item_t));
        if (!new_callbacks)
            return CONFIG_ERROR_OUT_OF_MEMORY;

        manager->callbacks = new_callbacks;
        manager->callback_capacity = new_capacity;
    }

    change_callback_item_t *cb = &manager->callbacks[manager->callback_count];
    cb->key = key ? duplicate_string(key) : NULL;
    cb->callback = callback;
    cb->user_data = user_data;

    if (key && !cb->key) {
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }

    manager->callback_count++;
    return CONFIG_SUCCESS;
}

static void *config_hot_reload_thread_func(void *arg);

config_error_t config_hot_reload_start(config_hot_reload_manager_t *manager,
                                       uint32_t check_interval_ms)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    if (manager->running)
        return CONFIG_SUCCESS;

    manager->check_interval_ms = check_interval_ms > 0 ? check_interval_ms : 5000;
    manager->running = true;

    if (manager->thread_handle == NULL) {
        airy_thread_t *thread = AIRY_MALLOC(sizeof(airy_thread_t));
        if (!thread) {
            manager->running = false;
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
        int rc = airy_thread_create(thread, config_hot_reload_thread_func, manager);
        if (rc != 0) {
            AIRY_FREE(thread);
            manager->running = false;
            return CONFIG_ERROR_THREAD;
        }
        manager->thread_handle = thread;
    }

    return CONFIG_SUCCESS;
}

config_error_t config_hot_reload_stop(config_hot_reload_manager_t *manager)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    if (!manager->running)
        return CONFIG_SUCCESS;

    manager->running = false;

    if (manager->thread_handle) {
        airy_thread_t *thread = (airy_thread_t *)manager->thread_handle;
        airy_thread_join(*thread, NULL);
        AIRY_FREE(thread);
        manager->thread_handle = NULL;
    }

    return CONFIG_SUCCESS;
}

static void *config_hot_reload_thread_func(void *arg)
{
    config_hot_reload_manager_t *manager = (config_hot_reload_manager_t *)arg;
    if (!manager) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    while (manager->running) {
        uint32_t interval = manager->check_interval_ms > 0 ? manager->check_interval_ms : 5000;
        struct timespec ts;
        ts.tv_sec = interval / 1000;
        ts.tv_nsec = (interval % 1000) * 1000000L;
        nanosleep(&ts, NULL);

        if (!manager->running)
            break;

        config_error_t err = config_hot_reload_trigger(manager);
        if (err != CONFIG_SUCCESS) {
            continue;
        }
    }

    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
}

config_error_t config_hot_reload_trigger(config_hot_reload_manager_t *manager)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    if (!manager->source_manager)
        return CONFIG_ERROR_INVALID_ARG;

    airy_mtx_lock((airy_mtx_t *)manager->lock);

    uint32_t debounce = manager->debounce_ms > 0 ? manager->debounce_ms : 500;
    uint64_t now_ms = (uint64_t)(time(NULL) * 1000);
    if (manager->last_trigger_time_ms > 0 && (now_ms - manager->last_trigger_time_ms) < debounce) {
        airy_mtx_unlock((airy_mtx_t *)manager->lock);
        return CONFIG_SUCCESS;
    }

    size_t source_count = 0;
    bool changed = false;

    if (manager->source_manager) {
        for (size_t i = 0; i < source_count; i++) {
            config_source_t *source = NULL;
            if (source && config_source_has_changed(source)) {
                changed = true;
                config_error_t err = config_source_load(source, manager->ctx);
                if (err != CONFIG_SUCCESS) {
                    airy_mtx_unlock((airy_mtx_t *)manager->lock);
                    return err;
                }
            }
        }
    }

    if (changed) {
        manager->last_trigger_time_ms = now_ms;
        for (size_t i = 0; i < manager->callback_count; i++) {
            change_callback_item_t *cb = &manager->callbacks[i];
            if (cb->callback) {
                cb->callback(manager->ctx, cb->key, NULL, NULL, cb->user_data);
            }
        }
    }

    airy_mtx_unlock((airy_mtx_t *)manager->lock);
    return CONFIG_SUCCESS;
}
