// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_source_file.c
 * @brief Unified config module - file config source.
 *
 * Implements the file config source: file load/save, format detection,
 * change detection (inotify/kqueue/ReadDirectoryChangesW with an mtime
 * fallback), single responsibility.
 */

#include "config_source.h"

#include "config_source_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/inotify.h>
#endif

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"

static bool check_file_modified(const char *file_path, uint64_t last_modified)
{
    if (!file_path)
        return false;
    struct stat st;
    if (stat(file_path, &st) != 0)
        return false;
    uint64_t mod_time = (uint64_t)st.st_mtime;
    return mod_time > last_modified;
}

#ifdef __linux__
static int file_source_init_inotify(file_source_priv_t *priv)
{
    if (!priv || !priv->file_path)
        return AIRY_EINVAL;
    priv->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (priv->inotify_fd < 0) {
        priv->inotify_enabled = false;
        return AIRY_EINVAL;
    }
    priv->inotify_wd =
        inotify_add_watch(priv->inotify_fd, priv->file_path,
                          IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_DELETE_SELF);
    if (priv->inotify_wd < 0) {
        close(priv->inotify_fd);
        priv->inotify_fd = -1;
        priv->inotify_enabled = false;
        return AIRY_EINVAL;
    }
    priv->inotify_enabled = true;
    return 0;
}

static bool file_source_check_inotify(file_source_priv_t *priv)
{
    if (!priv || !priv->inotify_enabled || priv->inotify_fd < 0)
        return false;
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t len = read(priv->inotify_fd, buf, sizeof(buf));
    if (len > 0) {
        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *)ptr;
            if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                return true;
            }
            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                inotify_rm_watch(priv->inotify_fd, priv->inotify_wd);
                close(priv->inotify_fd);
                priv->inotify_enabled = false;
                return true;
            }
        }
    }
    return false;
}

static void file_source_close_inotify(file_source_priv_t *priv)
{
    if (!priv)
        return;
    if (priv->inotify_enabled) {
        if (priv->inotify_wd >= 0)
            inotify_rm_watch(priv->inotify_fd, priv->inotify_wd);
        if (priv->inotify_fd >= 0)
            close(priv->inotify_fd);
        priv->inotify_fd = -1;
        priv->inotify_wd = -1;
        priv->inotify_enabled = false;
    }
}
#endif

#ifdef __APPLE__
#include <fcntl.h>
#include <sys/event.h>

static int file_source_init_kqueue(file_source_priv_t *priv)
{
    if (!priv || !priv->file_path)
        return AIRY_EINVAL;
    priv->kqueue_fd = kqueue();
    if (priv->kqueue_fd < 0) {
        priv->kqueue_enabled = false;
        return AIRY_EINVAL;
    }

    int fd = open(priv->file_path, O_RDONLY);
    if (fd < 0) {
        close(priv->kqueue_fd);
        priv->kqueue_fd = -1;
        priv->kqueue_enabled = false;
        return AIRY_EINVAL;
    }

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME, 0, NULL);
    if (kevent(priv->kqueue_fd, &ev, 1, NULL, 0, NULL) < 0) {
        close(fd);
        close(priv->kqueue_fd);
        priv->kqueue_fd = -1;
        priv->kqueue_enabled = false;
        return AIRY_EINVAL;
    }

    close(fd);
    priv->kqueue_enabled = true;
    return 0;
}

static bool file_source_check_kqueue(file_source_priv_t *priv)
{
    if (!priv || !priv->kqueue_enabled || priv->kqueue_fd < 0)
        return false;
    struct kevent ev;
    struct timespec ts = {0, 0};
    int n = kevent(priv->kqueue_fd, NULL, 0, &ev, 1, &ts);
    if (n > 0) {
        if (ev.fflags & (NOTE_DELETE | NOTE_RENAME)) {
            close(priv->kqueue_fd);
            priv->kqueue_fd = -1;
            priv->kqueue_enabled = false;
        }
        return true;
    }
    return false;
}

static void file_source_close_kqueue(file_source_priv_t *priv)
{
    if (!priv)
        return;
    if (priv->kqueue_enabled && priv->kqueue_fd >= 0) {
        close(priv->kqueue_fd);
        priv->kqueue_fd = -1;
        priv->kqueue_enabled = false;
    }
}
#endif

#ifdef _WIN32
#include <windows.h>

static int file_source_init_rdcw(file_source_priv_t *priv)
{
    if (!priv || !priv->file_path)
        return AIRY_EINVAL;

    char dir_path[MAX_PATH];
    size_t len = strlen(priv->file_path);
    if (len >= MAX_PATH)
        return AIRY_EINVAL;
    __builtin_memcpy(dir_path, priv->file_path, len + 1);

    char *last_sep = strrchr(dir_path, '\\');
    if (!last_sep)
        last_sep = strrchr(dir_path, '/');
    if (last_sep) {
        *last_sep = '\0';
        priv->dir_handle =
            CreateFileA(dir_path, FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    } else {
        priv->dir_handle =
            CreateFileA(".", FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    }

    if (priv->dir_handle == INVALID_HANDLE_VALUE) {
        priv->dir_handle = NULL;
        priv->rdcw_enabled = false;
        return AIRY_EINVAL;
    }

    priv->rdcw_enabled = true;
    return 0;
}

static bool file_source_check_rdcw(file_source_priv_t *priv)
{
    if (!priv || !priv->rdcw_enabled || !priv->dir_handle)
        return false;
    DWORD bytes_returned = 0;
    uint8_t buf[4096];
    BOOL success =
        ReadDirectoryChangesW(priv->dir_handle, buf, sizeof(buf), FALSE,
                              FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
                              &bytes_returned, NULL, NULL);
    if (success && bytes_returned > 0) {
        return true;
    }
    return false;
}

static void file_source_close_rdcw(file_source_priv_t *priv)
{
    if (!priv)
        return;
    if (priv->rdcw_enabled && priv->dir_handle) {
        CloseHandle(priv->dir_handle);
        priv->dir_handle = NULL;
        priv->rdcw_enabled = false;
    }
}
#endif

static config_error_t file_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    file_source_priv_t *priv = (file_source_priv_t *)source->priv_data;
    if (!priv || !priv->file_path)
        return CONFIG_ERROR_INVALID_ARG;

    FILE *file = fopen(priv->file_path, "r");
    if (!file)
        return CONFIG_ERROR_IO;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return CONFIG_ERROR_IO;
    }

    char *buffer = (char *)AIRY_MALLOC(file_size + 1);
    if (!buffer) {
        fclose(file);
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        AIRY_FREE(buffer);
        return CONFIG_ERROR_IO;
    }

    buffer[file_size] = '\0';

    config_error_t error = CONFIG_SUCCESS;
    if (priv->format && strcmp(priv->format, "json") == 0) {
        error = config_parse_json(buffer, file_size, ctx);
    } else if (priv->format && strcmp(priv->format, "yaml") == 0) {
        error = config_parse_yaml(buffer, file_size, ctx);
    } else if (priv->format && strcmp(priv->format, "ini") == 0) {
        error = config_parse_ini(buffer, file_size, ctx);
    } else {
        error = config_parse_json(buffer, file_size, ctx);
        if (error != CONFIG_SUCCESS) {
            error = config_parse_yaml(buffer, file_size, ctx);
        }
    }

    AIRY_FREE(buffer);
    return error;
}

static config_error_t file_source_save(config_source_t *source, const config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    file_source_priv_t *priv = (file_source_priv_t *)source->priv_data;
    if (!priv || !priv->file_path)
        return CONFIG_ERROR_INVALID_ARG;

    FILE *f = fopen(priv->file_path, "w");
    if (!f)
        return CONFIG_ERROR_IO;

    const char *ext = strrchr(priv->file_path, '.');
    bool is_json = (ext && (strcmp(ext, ".json") == 0 || strcmp(ext, ".JSON") == 0));

    if (is_json) {
        fputs("{\n", f);
        size_t count = config_context_count(ctx);
        for (size_t i = 0; i < count; i++) {
            const char *key = NULL;
            const config_value_t *val = config_context_get(ctx, key);
            if (!key || !val)
                continue;
            if (i > 0)
                fputs(",\n", f);
            const char *str_val = config_value_get_string(val, "");
            char line_buf[4096];
            snprintf(line_buf, sizeof(line_buf), "  \"%s\": \"%s\"", key, str_val ? str_val : "");
            fputs(line_buf, f);
        }
        fputs("\n}\n", f);
    } else {
        size_t count = config_context_count(ctx);
        for (size_t i = 0; i < count; i++) {
            const char *key = NULL;
            const config_value_t *val = config_context_get(ctx, key);
            if (!key || !val)
                continue;
            const char *str_val = config_value_get_string(val, "");
            char line_buf[4096];
            snprintf(line_buf, sizeof(line_buf), "%s=%s\n", key, str_val ? str_val : "");
            fputs(line_buf, f);
        }
    }

    fclose(f);
    return CONFIG_SUCCESS;
}

static bool file_source_has_changed(config_source_t *source)
{
    if (!source)
        return false;

    file_source_priv_t *priv = (file_source_priv_t *)source->priv_data;
    if (!priv || !priv->file_path)
        return false;

#ifdef __linux__
    if (priv->inotify_enabled) {
        return file_source_check_inotify(priv);
    }
#elif defined(__APPLE__)
    if (priv->kqueue_enabled) {
        return file_source_check_kqueue(priv);
    }
#elif defined(_WIN32)
    if (priv->rdcw_enabled) {
        return file_source_check_rdcw(priv);
    }
#endif
    return check_file_modified(priv->file_path, priv->last_modified);
}

static const config_source_attr_t *file_source_get_attributes(config_source_t *source)
{
    if (!source)
        return NULL;
    return &source->attributes;
}

static void file_source_destroy(config_source_t *source)
{
    if (!source)
        return;

    file_source_priv_t *priv = (file_source_priv_t *)source->priv_data;
    if (priv) {
#ifdef __linux__
        file_source_close_inotify(priv);
#elif defined(__APPLE__)
        file_source_close_kqueue(priv);
#elif defined(_WIN32)
        file_source_close_rdcw(priv);
#endif
        if (priv->file_path)
            AIRY_FREE(priv->file_path);
        if (priv->format)
            AIRY_FREE(priv->format);
        if (priv->encoding)
            AIRY_FREE(priv->encoding);
        if (priv->file_handle)
            fclose(priv->file_handle);
        AIRY_FREE(priv);
    }

    config_source_free_base(source);
}

static const config_source_adapter_t file_source_adapter = {.load = file_source_load,
                                                            .save = file_source_save,
                                                            .has_changed = file_source_has_changed,
                                                            .get_attributes =
                                                                file_source_get_attributes,
                                                            .destroy = file_source_destroy};

config_source_t *config_source_create_file(const config_file_source_options_t *options)
{
    if (!options || !options->file_path)
        return NULL;

    config_source_t *source =
        config_source_create_base(CONFIG_SOURCE_FILE, options->file_path, &file_source_adapter);
    if (!source)
        return NULL;

    file_source_priv_t *priv = (file_source_priv_t *)AIRY_CALLOC(1, sizeof(file_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    priv->file_path = duplicate_string(options->file_path);
    priv->format = options->format ? duplicate_string(options->format) : duplicate_string("json");
    priv->encoding =
        options->encoding ? duplicate_string(options->encoding) : duplicate_string("utf-8");
    priv->auto_reload = options->auto_reload;
    priv->reload_interval_ms = options->reload_interval_ms;
    priv->last_modified = 0;
    priv->file_handle = NULL;

    if (!priv->file_path || !priv->format || !priv->encoding) {
        if (priv->file_path)
            AIRY_FREE(priv->file_path);
        if (priv->format)
            AIRY_FREE(priv->format);
        if (priv->encoding)
            AIRY_FREE(priv->encoding);
        AIRY_FREE(priv);
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    source->priv_data = priv;
    source->attributes.watchable = options->auto_reload;
    source->attributes.read_only = false;

#ifdef __linux__
    if (options->auto_reload) {
        file_source_init_inotify(priv);
    }
#elif defined(__APPLE__)
    if (options->auto_reload) {
        file_source_init_kqueue(priv);
    }
#elif defined(_WIN32)
    if (options->auto_reload) {
        file_source_init_rdcw(priv);
    }
#endif

    return source;
}
