// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logging_control.c
 * @brief Unified layered logging system - runtime control.
 *
 * 2026-08-27 域拆分（原 logging.c）：节流（哈希桶 + 每秒上限）、
 * 分级采样、配置热加载（文件键值解析后原子替换）。
 */

#include "logging_internal.h"

/* ==================== Throttle ==================== */

#define THROTTLE_BUCKET_COUNT 256

typedef struct {
    uint64_t hash_key;
    uint64_t last_second;
    uint32_t count;
} throttle_bucket_t;

static throttle_bucket_t g_throttle_buckets[THROTTLE_BUCKET_COUNT];
static atomic_uint g_throttle_enabled = 0;
static atomic_uint g_throttle_max_per_sec = 100;
static airy_mtx_t g_throttle_mutex;
static bool g_throttle_mutex_init = false;

void log_internal_throttle_init_mutex(void)
{
    if (!g_throttle_mutex_init) {
        if (airy_mtx_init(&g_throttle_mutex) == 0) {
            g_throttle_mutex_init = true;
        }
    }
}

void log_internal_throttle_cleanup(void)
{
    if (g_throttle_mutex_init) {
        airy_mtx_destroy(&g_throttle_mutex);
        g_throttle_mutex_init = false;
    }
}

static uint64_t throttle_hash(const char *module, int line, const char *message)
{
    uint64_t h = 14695981039346656037ULL;
    const char *p;

    if (module) {
        for (p = module; *p; p++) {
            h ^= (uint64_t)(unsigned char)*p;
            h *= 1099511628211ULL;
        }
    }

    h ^= (uint64_t)line;
    h *= 1099511628211ULL;

    if (message) {
        for (p = message; *p; p++) {
            h ^= (uint64_t)(unsigned char)*p;
            h *= 1099511628211ULL;
        }
    }

    return h;
}

bool log_internal_throttle_suppress(const char *module, int line, const char *message,
                                    uint64_t now_sec)
{
    if (!g_throttle_enabled)
        return false;
    if (!g_throttle_mutex_init)
        return false;

    uint64_t h = throttle_hash(module, line, message);
    uint32_t bucket_idx = (uint32_t)(h % THROTTLE_BUCKET_COUNT);

    airy_mtx_lock(&g_throttle_mutex);

    throttle_bucket_t *bucket = &g_throttle_buckets[bucket_idx];

    if (bucket->last_second != now_sec) {
        if (bucket->last_second > 0 && bucket->count > g_throttle_max_per_sec) {
            airy_mtx_unlock(&g_throttle_mutex);
            return false;
        }
        bucket->last_second = now_sec;
        bucket->hash_key = h;
        bucket->count = 1;
        airy_mtx_unlock(&g_throttle_mutex);
        return false;
    }

    if (bucket->hash_key == h) {
        if (bucket->count >= g_throttle_max_per_sec) {
            bucket->count++;
            uint32_t suppressed = bucket->count - g_throttle_max_per_sec;
            airy_mtx_unlock(&g_throttle_mutex);

            if (suppressed == 1) {
                /* BAN-70 EXEMPT: logging module - diagnostic throttle notification */
                __builtin_fprintf(stderr,
                                  "[THROTTLE] Suppressing further identical messages: %s:%d\n",
                                  module ? module : "?", line);
            }
            return true;
        }
        bucket->count++;
        airy_mtx_unlock(&g_throttle_mutex);
        return false;
    }

    if (bucket->count > g_throttle_max_per_sec) {
        uint32_t old_suppressed = bucket->count - g_throttle_max_per_sec;
        if (old_suppressed > 0) {
            airy_mtx_unlock(&g_throttle_mutex);
            /* BAN-70 EXEMPT: logging module - diagnostic throttle notification */
            __builtin_fprintf(stderr,
                              "[THROTTLE] Previous bucket flushed: %u messages suppressed\n",
                              old_suppressed);
            airy_mtx_lock(&g_throttle_mutex);
        }
    }
    bucket->hash_key = h;
    bucket->count = 1;
    airy_mtx_unlock(&g_throttle_mutex);
    return false;
}

void log_set_throttle(bool enable, uint32_t max_per_sec)
{
    g_throttle_enabled = enable ? 1 : 0;
    if (max_per_sec > 0) {
        g_throttle_max_per_sec = max_per_sec;
    } else if (max_per_sec == 0 && enable) {
        g_throttle_max_per_sec = 100;
    }
}

/* ==================== Sampling ==================== */

static atomic_uint g_sample_counter_debug = 0;
static atomic_uint g_sample_counter_info = 0;
static atomic_uint g_sample_counter_warn = 0;

bool log_should_sample(log_level_t level)
{
    uint32_t counter;

    switch (level) {
    case LOG_LEVEL_DEBUG: {
        counter = AIRY_ATOMIC_FETCH_ADD(&g_sample_counter_debug, 1);
        return (counter % 1000) == 0; /* 0.1% */
    }
    case LOG_LEVEL_INFO: {
        counter = AIRY_ATOMIC_FETCH_ADD(&g_sample_counter_info, 1);
        return (counter % 100) == 0; /* 1% */
    }
    case LOG_LEVEL_WARN: {
        counter = AIRY_ATOMIC_FETCH_ADD(&g_sample_counter_warn, 1);
        return (counter % 10) == 0; /* 10% */
    }
    case LOG_LEVEL_ERROR:
    case LOG_LEVEL_FATAL:
        return true; /* 100% */
    default:
        return true;
    }
}

/* ==================== Config hot reload ==================== */

int log_reload_config(const char *config_path)
{
    if (!config_path) {
        return AIRY_EINVAL;
    }

    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        return AIRY_ENOENT;
    }

    logging_state_t *st = log_internal_state();

    char line[512];
    airy_mtx_lock(&st->mutex);
    log_config_t new_config = st->manager;
    airy_mtx_unlock(&st->mutex);

    int changes = 0;

    while (fgets(line, sizeof(line), fp)) {
        char key[128], value[256];
        char *saveptr = NULL;
        char *key_tok = strtok_r(line, " =\r\n", &saveptr);
        if (!key_tok)
            continue;
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        char *val_tok = eq + 1;
        while (*val_tok == ' ')
            val_tok++;
        size_t key_len = strlen(key_tok);
        if (key_len >= sizeof(key))
            key_len = sizeof(key) - 1;
        __builtin_memcpy(key, key_tok, key_len);
        key[key_len] = '\0';
        size_t val_len = strlen(val_tok);
        if (val_len >= sizeof(value))
            val_len = sizeof(value) - 1;
        __builtin_memcpy(value, val_tok, val_len);
        value[val_len] = '\0';
        {
            if (strcmp(key, "level") == 0) {
                log_level_t lvl = log_level_from_string(value);
                if ((int)lvl >= 0 && lvl < LOG_LEVEL_COUNT) {
                    new_config.level = lvl;
                    changes++;
                }
            } else if (strcmp(key, "output") == 0) {
                if (strstr(value, "file"))
                    new_config.outputs |= LOG_OUTPUT_FILE;
                if (strstr(value, "console"))
                    new_config.outputs |= LOG_OUTPUT_CONSOLE;
                if (strstr(value, "syslog"))
                    new_config.outputs |= LOG_OUTPUT_SYSLOG;
                changes++;
            } else if (strcmp(key, "format") == 0) {
                if (strcmp(value, "json") == 0)
                    new_config.format = LOG_FORMAT_JSON;
                else if (strcmp(value, "text") == 0)
                    new_config.format = LOG_FORMAT_TEXT;
                changes++;
            }
        }
    }

    fclose(fp);

    airy_mtx_lock(&st->mutex);
    st->manager = new_config;
    airy_mtx_unlock(&st->mutex);

    if (changes > 0) {
        /* BAN-70 EXEMPT: logging module - diagnostic config reload notification */
        __builtin_fprintf(stderr, "[LOGGING] Config reloaded from '%s' (%d changes applied)\n",
                          config_path, changes);
    }

    return changes > 0 ? 0 : AIRY_ENOENT;
}
