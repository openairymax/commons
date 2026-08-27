// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse_yaml_scalar.c
 * @brief Unified config module - YAML scalar & flow collection parsing.
 *
 * 2026-08-27 域拆分（原 config_parse.c）：引号标量、块标量、
 * 锚点/别名/标签前缀、流式序列/映射与纯标量解析。结构递归骨架见
 * config_parse_yaml.c。
 */

#include "config_parse_yaml.h"

static void yaml_ps_skip_token(yaml_parse_state_t *s)
{
    while (s->pos < s->len && yaml_ps_peek(s) != ' ' && yaml_ps_peek(s) != '\t' &&
           yaml_ps_peek(s) != '\n' && yaml_ps_peek(s) != '\r')
        yaml_ps_advance(s);
}

static void yaml_ps_rtrim(char *buf, size_t *len)
{
    while (*len > 0 && (buf[*len - 1] == ' ' || buf[*len - 1] == '\t'))
        (*len)--;
}

/* Consume leading anchor ('&'), alias ('*') and tag ('!') prefixes; when an
 * alias is consumed the value has no payload, so report it via the return
 * value. Otherwise c receives the first payload character. */
bool yaml_ps_skip_anchor_tag(yaml_parse_state_t *s, int *c)
{
    int ch = yaml_ps_peek(s);
    if (ch == '&') {
        yaml_ps_advance(s);
        yaml_ps_skip_token(s);
        yaml_ps_skip_ws(s);
        ch = yaml_ps_peek(s);
    }
    if (ch == '*') {
        yaml_ps_advance(s);
        yaml_ps_skip_token(s);
        return true;
    }
    if (ch == '!') {
        yaml_ps_advance(s);
        if (s->pos < s->len && yaml_ps_peek(s) == '!')
            yaml_ps_advance(s);
        yaml_ps_skip_token(s);
        yaml_ps_skip_ws(s);
        ch = yaml_ps_peek(s);
    }
    *c = ch;
    return false;
}

config_error_t yaml_parse_quoted(yaml_parse_state_t *s, int quote,
                                 const char *prefix, config_context_t *ctx)
{
    yaml_ps_advance(s);
    char val_buf[2048];
    size_t vlen = 0;
    while (s->pos < s->len && yaml_ps_peek(s) != quote && vlen < sizeof(val_buf) - 1) {
        int ch = yaml_ps_advance(s);
        if (ch == '\\' && s->pos < s->len) {
            ch = yaml_ps_advance(s);
            switch (ch) {
            case 'n':
                ch = '\n';
                break;
            case 't':
                ch = '\t';
                break;
            case 'r':
                ch = '\r';
                break;
            default:
                break;
            }
        }
        val_buf[vlen++] = (char)ch;
    }
    if (s->pos < s->len && yaml_ps_peek(s) == quote)
        yaml_ps_advance(s);
    val_buf[vlen] = '\0';
    config_value_t *cv = config_value_create_string(val_buf);
    if (cv)
        config_context_set(ctx, prefix, cv);
    return CONFIG_SUCCESS;
}

/* Read the indented lines of a literal/folded block scalar into val_buf;
 * stops at EOF or at a line dedented below the block indent. */
static void yaml_ps_read_block_lines(yaml_parse_state_t *s, int *block_indent, char *val_buf,
                                     size_t *vlen, size_t buf_size)
{
    while (s->pos < s->len) {
        size_t saved_pos = s->pos;
        int line_ind = 0;
        while (s->pos < s->len && yaml_ps_peek(s) == ' ') {
            line_ind++;
            yaml_ps_advance(s);
        }
        if (s->pos >= s->len)
            break;
        int lc = yaml_ps_peek(s);
        if (lc == '\n' || lc == '\r') {
            yaml_ps_skip_eol(s);
            if (*vlen < buf_size - 1)
                val_buf[(*vlen)++] = '\n';
            continue;
        }
        if (*block_indent < 0)
            *block_indent = line_ind;
        if (line_ind < *block_indent) {
            s->pos = saved_pos;
            break;
        }
        if (*vlen > 0 && *vlen < buf_size - 1)
            val_buf[(*vlen)++] = '\n';
        while (s->pos < s->len && yaml_ps_peek(s) != '\n' && yaml_ps_peek(s) != '\r') {
            if (*vlen < buf_size - 1)
                val_buf[(*vlen)++] = (char)yaml_ps_advance(s);
            else
                yaml_ps_advance(s);
        }
        yaml_ps_skip_eol(s);
    }
}

config_error_t yaml_parse_block_scalar(yaml_parse_state_t *s, const char *prefix,
                                       config_context_t *ctx)
{
    yaml_ps_advance(s);
    while (s->pos < s->len && (yaml_ps_peek(s) == '-' || yaml_ps_peek(s) == '+' ||
                               (yaml_ps_peek(s) >= '1' && yaml_ps_peek(s) <= '9')))
        yaml_ps_advance(s);
    yaml_ps_skip_to_eol(s);
    yaml_ps_skip_eol(s);

    int block_indent = -1;
    char val_buf[4096];
    size_t vlen = 0;
    yaml_ps_read_block_lines(s, &block_indent, val_buf, &vlen, sizeof(val_buf));
    val_buf[vlen] = '\0';
    config_value_t *cv = config_value_create_string(val_buf);
    if (cv)
        config_context_set(ctx, prefix, cv);
    return CONFIG_SUCCESS;
}

static void yaml_parse_inline_seq_item(yaml_parse_state_t *s, const char *prefix, int idx,
                                       config_context_t *ctx)
{
    char idx_key[1024];
    snprintf(idx_key, sizeof(idx_key), "%s.%d", prefix, idx);
    char val_buf[1024];
    size_t vlen = 0;
    while (s->pos < s->len && yaml_ps_peek(s) != ',' && yaml_ps_peek(s) != ']' &&
           yaml_ps_peek(s) != '\n') {
        if (vlen < sizeof(val_buf) - 1)
            val_buf[vlen++] = (char)yaml_ps_advance(s);
        else
            yaml_ps_advance(s);
    }
    val_buf[vlen] = '\0';
    yaml_ps_rtrim(val_buf, &vlen);
    val_buf[vlen] = '\0';
    config_value_t *cv = config_value_create_string(val_buf);
    if (cv)
        config_context_set(ctx, idx_key, cv);
}

config_error_t yaml_parse_inline_sequence(yaml_parse_state_t *s, const char *prefix,
                                          config_context_t *ctx)
{
    yaml_ps_advance(s);
    yaml_ps_skip_ws(s);
    int idx = 0;
    while (s->pos < s->len && yaml_ps_peek(s) != ']' && yaml_ps_peek(s) != '\n' &&
           yaml_ps_peek(s) != '\r') {
        if (yaml_ps_peek(s) == ',') {
            yaml_ps_advance(s);
            yaml_ps_skip_ws(s);
            continue;
        }
        yaml_parse_inline_seq_item(s, prefix, idx, ctx);
        idx++;
        yaml_ps_skip_ws(s);
    }
    if (s->pos < s->len && yaml_ps_peek(s) == ']')
        yaml_ps_advance(s);
    return CONFIG_SUCCESS;
}

static void yaml_parse_inline_map_pair(yaml_parse_state_t *s, const char *prefix,
                                       config_context_t *ctx)
{
    char kbuf[512];
    size_t klen = 0;
    while (s->pos < s->len && yaml_ps_peek(s) != ':' && yaml_ps_peek(s) != ',' &&
           yaml_ps_peek(s) != '}' && yaml_ps_peek(s) != '\n') {
        if (klen < sizeof(kbuf) - 1)
            kbuf[klen++] = (char)yaml_ps_advance(s);
        else
            yaml_ps_advance(s);
    }
    kbuf[klen] = '\0';
    yaml_ps_rtrim(kbuf, &klen);
    kbuf[klen] = '\0';
    yaml_ps_skip_ws(s);
    if (s->pos < s->len && yaml_ps_peek(s) == ':')
        yaml_ps_advance(s);
    yaml_ps_skip_ws(s);
    char fkey[1024];
    snprintf(fkey, sizeof(fkey), "%s.%s", prefix, kbuf);
    char vbuf[1024];
    size_t vlen2 = 0;
    while (s->pos < s->len && yaml_ps_peek(s) != ',' && yaml_ps_peek(s) != '}' &&
           yaml_ps_peek(s) != '\n') {
        if (vlen2 < sizeof(vbuf) - 1)
            vbuf[vlen2++] = (char)yaml_ps_advance(s);
        else
            yaml_ps_advance(s);
    }
    vbuf[vlen2] = '\0';
    yaml_ps_rtrim(vbuf, &vlen2);
    vbuf[vlen2] = '\0';
    config_value_t *cv = config_value_create_string(vbuf);
    if (cv)
        config_context_set(ctx, fkey, cv);
}

config_error_t yaml_parse_inline_mapping(yaml_parse_state_t *s, const char *prefix,
                                         config_context_t *ctx)
{
    yaml_ps_advance(s);
    yaml_ps_skip_ws(s);
    while (s->pos < s->len && yaml_ps_peek(s) != '}') {
        if (yaml_ps_peek(s) == ',') {
            yaml_ps_advance(s);
            yaml_ps_skip_ws(s);
            continue;
        }
        yaml_parse_inline_map_pair(s, prefix, ctx);
        yaml_ps_skip_ws(s);
    }
    if (s->pos < s->len && yaml_ps_peek(s) == '}')
        yaml_ps_advance(s);
    return CONFIG_SUCCESS;
}

config_error_t yaml_parse_plain_scalar_value(yaml_parse_state_t *s, int base_indent,
                                             const char *prefix, config_context_t *ctx)
{
    char val_buf[2048];
    size_t vlen = 0;
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == ':' && s->pos + 1 < s->len &&
            (s->src[s->pos + 1] == ' ' || s->src[s->pos + 1] == '\n'))
            break;
        if (c == '#' && vlen > 0 && val_buf[vlen - 1] == ' ')
            break;
        if (c == '\n' || c == '\r')
            break;
        if (vlen < sizeof(val_buf) - 1)
            val_buf[vlen++] = (char)yaml_ps_advance(s);
        else
            yaml_ps_advance(s);
    }
    yaml_ps_rtrim(val_buf, &vlen);
    val_buf[vlen] = '\0';

    yaml_ps_skip_ws(s);
    if (s->pos < s->len && yaml_ps_peek(s) == ':') {
        yaml_ps_advance(s);
        yaml_ps_skip_ws(s);
        yaml_parse_mapping(s, base_indent, val_buf, ctx);
        return CONFIG_SUCCESS;
    }

    config_value_t *cv = config_value_create_string(val_buf);
    if (cv)
        config_context_set(ctx, prefix, cv);
    return CONFIG_SUCCESS;
}
