/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file config_source_internal.h
 * @brief Unified config module - source adapter internal shared defs.
 *
 * After config_source.c was split by functional domain, this header
 * carries the shared contract between the pieces:
 *   - config_source.c          base class and common API
 *   - config_source_file.c     file config source
 *   - config_source_env.c      environment config source
 *   - config_source_args.c     command line config source
 *   - config_source_memory.c   memory/default/remote config source
 *   - config_source_manager.c  source manager and change monitoring
 */

#ifndef AIRY_RT_CONFIG_SOURCE_INTERNAL_H
#define AIRY_RT_CONFIG_SOURCE_INTERNAL_H

#include "config_source.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "error.h"

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
    char *file_path;
    char *format;
    char *encoding;
    bool auto_reload;
    uint32_t reload_interval_ms;
    uint64_t last_modified;
    FILE *file_handle;
#ifdef __linux__
    int inotify_fd;
    int inotify_wd;
    bool inotify_enabled;
#elif defined(__APPLE__)
    int kqueue_fd;
    bool kqueue_enabled;
#elif defined(_WIN32)
    void *dir_handle;
    uint8_t rdcw_buffer[4096];
    bool rdcw_enabled;
#endif
} file_source_priv_t;

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
    int argc;
    char **argv;
    char *prefix;
    char *assign_char;
    bool allow_positional;
} args_source_priv_t;

typedef struct {
    char *data;
    size_t data_len;
    char *format;
    bool owns_data;
} memory_source_priv_t;

typedef struct {
    char **keys;
    char **vals;
    size_t num_entries;
} defaults_source_priv_t;

/** remote source private data */
typedef struct {
    char *url;
    char *token;
    char *namespace_name;
    uint32_t poll_interval_ms;
    uint64_t last_etag_hash;
    uint64_t last_poll_time_ms;
    char *last_response;
    size_t last_response_len;
} remote_source_priv_t;

struct config_source_manager {

    config_source_t **sources;

    size_t count;

    size_t capacity;

    void (*change_callback)(config_source_t *source, void *user_data);

    void *callback_user_data;

    bool watching;

    airy_mtx_t internal_mutex;

    uint64_t last_notify_time_ms;

    uint64_t debounce_ms;
};

config_source_t *config_source_create_base(config_source_type_t type, const char *name,
                                           const config_source_adapter_t *adapter);
void config_source_free_base(config_source_t *source);

static inline char *duplicate_string(const char *str)
{
    if (!str)
        return NULL;
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

#endif /* AIRY_RT_CONFIG_SOURCE_INTERNAL_H */
