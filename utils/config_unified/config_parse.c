// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse.c
 * @brief Unified config module - INI format parsing.
 *
 * 2026-08-27 域拆分：原 config_parse.c（994 行）按格式拆分，本文件
 * 保留 INI 解析；JSON 见 config_parse_json.c，YAML 见
 * config_parse_yaml.c。
 * 解析结果写入 config_context_t（点分键 -> config_value_t）。
 */

#include "config_parse_internal.h"

/* ==================== INI parsing ==================== */

/*
 * 注释行与节头行都要跳到下一行起点，原实现在两处各写了一遍同样的四
 * 行游标推进；统一到这里后两处只保留一次语义，且 `[` 未闭合时可安全
 * 返回 end。
 */
static const char *ini_eol(const char *p, const char *end)
{
    while (p < end && *p != '\n')
        p++;
    return (p < end) ? p + 1 : p;
}

static const char *ini_blank(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    return p;
}

/* `[name]`：未闭合时取到行尾，超长按 cap 截断；返回节头之后的下一行。 */
static const char *ini_sect(const char *p, const char *end, char *sect, size_t cap)
{
    const char *close = p + 1;
    size_t len;

    while (close < end && *close != ']')
        close++;
    len = (size_t)(close - p - 1);
    if (len >= cap)
        len = cap - 1;
    __builtin_memcpy(sect, p + 1, len);
    sect[len] = '\0';
    return ini_eol(close, end);
}

/* 定位行内首个 '='（只认第一个），同时返回行尾便于调用方推进游标。 */
static const char *ini_split(const char *line, const char *end, const char **eq)
{
    const char *p = line;

    *eq = NULL;
    while (p < end && *p != '\n') {
        if (*p == '=' && !*eq)
            *eq = p;
        p++;
    }
    return p;
}

/*
 * 单条 key=value 落库：值侧只剥离行尾的 CR/LF/空格，键侧只剥离尾部的
 * 空格与制表符，前导空白上游已跳过；带节时键前缀为 "节."，超长键按原
 * 语义丢弃而不是截断成另一个有效键。
 */
static void ini_pair(const char *line, const char *eq, const char *eol,
                     const char *sect, config_context_t *ctx)
{
    char key[512];
    char val[1024];
    size_t klen = (size_t)(eq - line);
    size_t vlen;
    const char *vend = eol;

    while (vend > eq + 1 && (vend[-1] == '\r' || vend[-1] == '\n' || vend[-1] == ' '))
        vend--;
    while (klen > 0 && (line[klen - 1] == ' ' || line[klen - 1] == '\t'))
        klen--;
    if (sect[0]) {
        size_t off = strlen(sect) + 1;

        snprintf(key, sizeof(key), "%s.", sect);
        if (off + klen < sizeof(key)) {
            __builtin_memcpy(key + off, line, klen);
            key[off + klen] = '\0';
        }
    } else {
        if (klen >= sizeof(key))
            klen = sizeof(key) - 1;
        __builtin_memcpy(key, line, klen);
        key[klen] = '\0';
    }
    vlen = (size_t)(vend - eq - 1);
    if (vlen >= sizeof(val))
        vlen = sizeof(val) - 1;
    __builtin_memcpy(val, eq + 1, vlen);
    val[vlen] = '\0';
    config_value_t *cv = config_value_create_string(val);
    if (cv)
        config_context_set(ctx, key, cv);
}

static config_error_t parse_ini_simple(const char *data, size_t data_len, config_context_t *ctx)
{
    char section[256] = "";
    const char *p = data;
    const char *end;
    const char *eq;

    if (!data || data_len == 0 || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    end = data + data_len;
    while (p < end) {
        p = ini_blank(p, end);
        if (p >= end)
            break;
        if (*p == '#' || *p == ';') {
            p = ini_eol(p, end);
            continue;
        }
        if (*p == '[') {
            p = ini_sect(p, end, section, sizeof(section));
            continue;
        }
        const char *line = p;

        p = ini_split(line, end, &eq);
        if (eq && eq > line)
            ini_pair(line, eq, p, section, ctx);
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
