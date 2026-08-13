// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_service_version.c
 * @brief Unified config module - version management and rollback.
 *
 * Implements the config version manager: snapshot creation, rollback,
 * version listing and version diffing, single responsibility.
 */

#include "config_service.h"

#include "config_service_internal.h"

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "error.h"

config_version_manager_t *config_version_manager_create(config_context_t *ctx, size_t max_versions)
{
    if (!ctx) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    config_version_manager_t *manager =
        (config_version_manager_t *)AIRY_CALLOC(1, sizeof(config_version_manager_t));
    if (!manager) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    manager->ctx = ctx;
    manager->max_versions = max_versions > 0 ? max_versions : 10;
    manager->next_version = 1;

    manager->capacity = 8;
    manager->versions =
        (config_version_item_t *)AIRY_CALLOC(manager->capacity, sizeof(config_version_item_t));
    if (!manager->versions) {
        AIRY_FREE(manager);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    manager->count = 0;

    return manager;
}

void config_version_manager_destroy(config_version_manager_t *manager)
{
    if (!manager)
        return;

    for (size_t i = 0; i < manager->count; i++) {
        config_version_item_t *version = &manager->versions[i];
        if (version->author)
            AIRY_FREE(version->author);
        if (version->description)
            AIRY_FREE(version->description);
        if (version->snapshot) {
            config_context_destroy(version->snapshot);
        }
    }

    if (manager->versions)
        AIRY_FREE(manager->versions);
    AIRY_FREE(manager);
}

uint32_t config_version_create_snapshot(config_version_manager_t *manager, const char *author,
                                        const char *description)
{
    if (!manager)
        return 0;

    if (manager->count >= manager->capacity) {
        size_t new_capacity = manager->capacity * 2;
        config_version_item_t *new_versions =
            (config_version_item_t *)AIRY_REALLOC(manager->versions,
                                                  new_capacity * sizeof(config_version_item_t));
        if (!new_versions)
            return 0;

        manager->versions = new_versions;
        manager->capacity = new_capacity;
    }

    config_version_item_t *version = &manager->versions[manager->count];
    AIRY_MEMSET(version, 0, sizeof(config_version_item_t));

    version->version = manager->next_version++;
    version->timestamp = (uint64_t)time(NULL);

    if (author) {
        version->author = duplicate_string(author);
        if (!version->author)
            return 0;
    }

    if (description) {
        version->description = duplicate_string(description);
        if (!version->description) {
            if (version->author)
                AIRY_FREE(version->author);
            return 0;
        }
    }

    version->snapshot = config_context_clone(manager->ctx);
    if (!version->snapshot) {
        if (version->author)
            AIRY_FREE(version->author);
        if (version->description)
            AIRY_FREE(version->description);
        return 0;
    }

    manager->count++;

    if (manager->count > manager->max_versions) {
        config_version_item_t *oldest = &manager->versions[0];
        if (oldest->author)
            AIRY_FREE(oldest->author);
        if (oldest->description)
            AIRY_FREE(oldest->description);
        if (oldest->snapshot) {
            config_context_destroy(oldest->snapshot);
        }

        for (size_t i = 1; i < manager->count; i++) {
            manager->versions[i - 1] = manager->versions[i];
        }

        manager->count--;
    }

    return version->version;
}

config_error_t config_version_rollback(config_version_manager_t *manager, uint32_t version)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    for (size_t i = 0; i < manager->count; i++) {
        if (manager->versions[i].version == version) {
            if (!manager->versions[i].snapshot)
                return CONFIG_ERROR_UNSUPPORTED;

            config_error_t err = config_context_copy(manager->ctx, manager->versions[i].snapshot);
            if (err != CONFIG_SUCCESS)
                return err;

            for (size_t j = i + 1; j < manager->count; j++) {
                if (manager->versions[j].author)
                    AIRY_FREE(manager->versions[j].author);
                if (manager->versions[j].description)
                    AIRY_FREE(manager->versions[j].description);
                if (manager->versions[j].snapshot)
                    config_context_destroy(manager->versions[j].snapshot);
            }
            manager->count = i + 1;
            return CONFIG_SUCCESS;
        }
    }

    return CONFIG_ERROR_NOT_FOUND;
}

size_t config_version_get_list(config_version_manager_t *manager, config_version_info_t *versions,
                               size_t max_count)
{
    if (!manager || !versions || max_count == 0)
        return 0;

    size_t count = manager->count;
    if (count > max_count)
        count = max_count;

    for (size_t i = 0; i < count; i++) {
        config_version_item_t *src = &manager->versions[manager->count - 1 - i];
        config_version_info_t *dst = &versions[i];

        dst->version = src->version;
        dst->timestamp = src->timestamp;
        dst->author = src->author;
        dst->description = src->description;

        dst->change_count = 0;
        if (src->snapshot) {
            dst->change_count = config_context_count(src->snapshot);
        }
    }

    return count;
}

static const char *value_to_summary(const config_value_t *val, char *buf, size_t buf_size)
{
    if (!val) {
        snprintf(buf, buf_size, "<null>");
        return buf;
    }
    switch (config_value_get_type(val)) {
    case CONFIG_TYPE_BOOL:
        snprintf(buf, buf_size, "%s", config_value_get_bool(val, false) ? "true" : "false");
        break;
    case CONFIG_TYPE_INT:
        snprintf(buf, buf_size, "%d", config_value_get_int(val, 0));
        break;
    case CONFIG_TYPE_INT64:
        snprintf(buf, buf_size, "%lld", (long long)config_value_get_int64(val, 0));
        break;
    case CONFIG_TYPE_DOUBLE:
        snprintf(buf, buf_size, "%g", config_value_get_double(val, 0.0));
        break;
    case CONFIG_TYPE_STRING:
        snprintf(buf, buf_size, "\"%s\"", config_value_get_string(val, ""));
        break;
    case CONFIG_TYPE_NULL:
        snprintf(buf, buf_size, "null");
        break;
    case CONFIG_TYPE_ARRAY:
        snprintf(buf, buf_size, "[array]");
        break;
    case CONFIG_TYPE_OBJECT:
        snprintf(buf, buf_size, "{object}");
        break;
    case CONFIG_TYPE_BINARY:
        snprintf(buf, buf_size, "<binary>");
        break;
    default:
        snprintf(buf, buf_size, "<unknown>");
        break;
    }
    return buf;
}

static bool values_equal(const config_value_t *a, const config_value_t *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    config_value_type_t ta = config_value_get_type(a);
    config_value_type_t tb = config_value_get_type(b);
    if (ta != tb)
        return false;
    switch (ta) {
    case CONFIG_TYPE_NULL:
        return true;
    case CONFIG_TYPE_BOOL:
        return config_value_get_bool(a, false) == config_value_get_bool(b, false);
    case CONFIG_TYPE_INT:
        return config_value_get_int(a, 0) == config_value_get_int(b, 0);
    case CONFIG_TYPE_INT64:
        return config_value_get_int64(a, 0) == config_value_get_int64(b, 0);
    case CONFIG_TYPE_DOUBLE:
        return config_value_get_double(a, 0.0) == config_value_get_double(b, 0.0);
    case CONFIG_TYPE_STRING: {
        const char *sa = config_value_get_string(a, NULL);
        const char *sb = config_value_get_string(b, NULL);
        if (!sa && !sb)
            return true;
        if (!sa || !sb)
            return false;
        return strcmp(sa, sb) == 0;
    }
    default:
        return false;
    }
}

size_t config_version_get_diff(config_version_manager_t *manager, uint32_t version1,
                               uint32_t version2, char *diff, size_t diff_size)
{
    if (!manager || !diff || diff_size == 0)
        return 0;

    config_version_item_t *v1 = NULL;
    config_version_item_t *v2 = NULL;
    for (size_t i = 0; i < manager->count; i++) {
        if (manager->versions[i].version == version1)
            v1 = &manager->versions[i];
        if (manager->versions[i].version == version2)
            v2 = &manager->versions[i];
    }
    if (!v1 || !v2) {
        size_t w = snprintf(diff, diff_size, "Version %u or %u not found", version1, version2);
        return w < diff_size ? w : diff_size - 1;
    }
    if (!v1->snapshot || !v2->snapshot) {
        size_t w =
            snprintf(diff, diff_size, "Version %u or %u has no snapshot", version1, version2);
        return w < diff_size ? w : diff_size - 1;
    }

    size_t pos = 0;
    int change_count = 0;

    pos += snprintf(diff + pos, diff_size - pos, "Diff v%u -> v%u:\n", version1, version2);
    if (pos >= diff_size)
        return diff_size - 1;

    for (size_t i = 0; i < config_context_count(v2->snapshot); i++) {
        const char *key = config_context_get_key_at(v2->snapshot, i);
        const config_value_t *new_val = config_context_get_value_at(v2->snapshot, i);
        const config_value_t *old_val = config_context_get(v1->snapshot, key);

        char old_buf[128], new_buf[128];
        if (!old_val) {
            pos += snprintf(diff + pos, diff_size - pos, "  + %s = %s\n", key,
                            value_to_summary(new_val, new_buf, sizeof(new_buf)));
            change_count++;
        } else if (!values_equal(old_val, new_val)) {
            pos += snprintf(diff + pos, diff_size - pos, "  ~ %s: %s -> %s\n", key,
                            value_to_summary(old_val, old_buf, sizeof(old_buf)),
                            value_to_summary(new_val, new_buf, sizeof(new_buf)));
            change_count++;
        }
        if (pos >= diff_size - 1) {
            pos = diff_size - 1;
            break;
        }
    }

    for (size_t i = 0; i < config_context_count(v1->snapshot); i++) {
        const char *key = config_context_get_key_at(v1->snapshot, i);
        if (!config_context_has(v2->snapshot, key)) {
            const config_value_t *old_val = config_context_get_value_at(v1->snapshot, i);
            char old_buf[128];
            pos += snprintf(diff + pos, diff_size - pos, "  - %s = %s\n", key,
                            value_to_summary(old_val, old_buf, sizeof(old_buf)));
            change_count++;
            if (pos >= diff_size - 1) {
                pos = diff_size - 1;
                break;
            }
        }
    }

    if (change_count == 0 && pos < diff_size - 1) {
        pos += snprintf(diff + pos, diff_size - pos, "  (no changes)\n");
    }

    return pos < diff_size ? pos : diff_size - 1;
}
