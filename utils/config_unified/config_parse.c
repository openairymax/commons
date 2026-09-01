// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse.c
 * @brief Unified config module - INI format parsing.
 *
 * 2026-08-27 域拆分：原 config_parse.c（994 行）按格式拆分，本文件
 * 保留 INI 解析；JSON 见 config_parse_json.c，YAML 见
 * config_parse_yaml.c / config_parse_yaml_scalar.c。
 * 解析结果写入 config_context_t（点分键 -> config_value_t）。
 */

#include "config_parse_internal.h"

/* ==================== INI parsing ==================== */

static config_error_t parse_ini_simple(const char *data, size_t data_len, config_context_t *ctx)
{
    if (!data || data_len == 0 || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    char section[256] = "";
    const char *p = data;
    const char *end = data + data_len;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p >= end)
            break;
        if (*p == '#' || *p == ';') {
            while (p < end && *p != '\n')
                p++;
            if (p < end)
                p++;
            continue;
        }
        if (*p == '[') {
            p++;
            const char *sec_start = p;
            while (p < end && *p != ']')
                p++;
            size_t sec_len = (size_t)(p - sec_start);
            if (sec_len >= sizeof(section))
                sec_len = sizeof(section) - 1;
            __builtin_memcpy(section, sec_start, sec_len);
            section[sec_len] = '\0';
            while (p < end && *p != '\n')
                p++;
            if (p < end)
                p++;
            continue;
        }
        const char *line_start = p;
        const char *eq = NULL;
        while (p < end && *p != '\n') {
            if (*p == '=' && !eq)
                eq = p;
            p++;
        }
        if (eq && eq > line_start) {
            size_t key_len = (size_t)(eq - line_start);
            char key_buf[512];
            const char *val_start = eq + 1;
            const char *val_end = p;
            while (val_end > val_start &&
                   (*(val_end - 1) == '\r' || *(val_end - 1) == '\n' || *(val_end - 1) == ' '))
                val_end--;
            size_t val_len = (size_t)(val_end - val_start);
            while (key_len > 0 &&
                   (*(line_start + key_len - 1) == ' ' || *(line_start + key_len - 1) == '\t'))
                key_len--;
            if (section[0]) {
                snprintf(key_buf, sizeof(key_buf), "%s.", section);
                size_t sl = strlen(key_buf);
                if (sl + key_len < sizeof(key_buf)) {
                    __builtin_memcpy(key_buf + sl, line_start, key_len);
                    key_buf[sl + key_len] = '\0';
                }
            } else {
                if (key_len >= sizeof(key_buf))
                    key_len = sizeof(key_buf) - 1;
                __builtin_memcpy(key_buf, line_start, key_len);
                key_buf[key_len] = '\0';
            }
            char val_buf[1024];
            if (val_len >= sizeof(val_buf))
                val_len = sizeof(val_buf) - 1;
            __builtin_memcpy(val_buf, val_start, val_len);
            val_buf[val_len] = '\0';
            config_value_t *cv = config_value_create_string(val_buf);
            if (cv)
                config_context_set(ctx, key_buf, cv);
        }
        if (p < end)
            p++;
    }
    return CONFIG_SUCCESS;
}

/* ==================== Public entry point ==================== */

config_error_t config_parse_ini(const char *data, size_t data_len, config_context_t *ctx)
{
    return parse_ini_simple(data, data_len, ctx);
}
