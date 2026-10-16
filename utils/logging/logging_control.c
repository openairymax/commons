// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logging_control.c
 * @brief Unified layered logging system - runtime control.
 *
 * 2026-08-27 域拆分（原 logging.c）：配置热加载（文件键值解析后原子替换）。
 * 0.1.19 §3.6 D7：移除无调用方的节流/采样机制（死代码净减，体积守恒）。
 */

#include "logging_internal.h"

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
