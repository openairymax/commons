// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse_yaml.c
 * @brief Unified config module - YAML structural parsing.
 *
 * 2026-08-27 域拆分（原 config_parse.c）：缩进感知 YAML 子集的
 * 状态机骨架、映射/序列递归结构与公共入口；标量细节见
 * config_parse_yaml_scalar.c。
 */

#include "config_parse_yaml.h"

/* ==================== Parse state helpers ==================== */

int yaml_ps_peek(yaml_parse_state_t *s)
{
    return (s->pos < s->len) ? (unsigned char)s->src[s->pos] : -1;
}

int yaml_ps_advance(yaml_parse_state_t *s)
{
    if (s->pos >= s->len)
        return AIRY_EINVAL;
    int c = (unsigned char)s->src[s->pos++];
    if (c == '\n')
        s->line++;
    return c;
}

void yaml_ps_skip_ws(yaml_parse_state_t *s)
{
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == ' ' || c == '\t')
            yaml_ps_advance(s);
        else
            break;
    }
}

__attribute__((unused)) static void yaml_ps_skip_ws_nl(yaml_parse_state_t *s)
{
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            yaml_ps_advance(s);
        else
            break;
    }
}

int yaml_ps_count_indent(yaml_parse_state_t *s)
{
    int indent = 0;
    while (s->pos < s->len && yaml_ps_peek(s) == ' ') {
        indent++;
        yaml_ps_advance(s);
    }
    return indent;
}

void yaml_ps_skip_to_eol(yaml_parse_state_t *s)
{
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == '\n' || c == '\r')
            break;
        yaml_ps_advance(s);
    }
}

void yaml_ps_skip_eol(yaml_parse_state_t *s)
{
    if (s->pos < s->len && yaml_ps_peek(s) == '\r')
        yaml_ps_advance(s);
    if (s->pos < s->len && yaml_ps_peek(s) == '\n')
        yaml_ps_advance(s);
}

/* ==================== Structural recursion ==================== */

config_error_t yaml_parse_mapping(yaml_parse_state_t *s, int base_indent, const char *prefix,
                                  config_context_t *ctx)
{
    char key_buf[768];
    char full_key[1024];

    while (s->pos < s->len) {
        while (s->pos < s->len) {
            if (yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
                yaml_ps_skip_eol(s);
            } else {
                break;
            }
        }
        if (s->pos >= s->len)
            break;

        int ind = yaml_ps_count_indent(s);
        if (ind <= base_indent)
            break;

        yaml_ps_skip_ws(s);
        if (s->pos >= s->len)
            break;

        int c = yaml_ps_peek(s);
        if (c == '#' || c == '\n' || c == '\r') {
            yaml_ps_skip_to_eol(s);
            yaml_ps_skip_eol(s);
            continue;
        }

        if (c == '-' && s->pos + 1 < s->len) {
            int next = (unsigned char)s->src[s->pos + 1];
            if (next == '-' && s->pos + 2 < s->len && (unsigned char)s->src[s->pos + 2] == '-')
                break;
        }

        if (c == '.' && s->pos + 2 < s->len && (unsigned char)s->src[s->pos + 1] == '.' &&
            (unsigned char)s->src[s->pos + 2] == '.')
            break;

        size_t klen = 0;
        while (s->pos < s->len) {
            c = yaml_ps_peek(s);
            if (c == ':' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#')
                break;
            if (klen < sizeof(key_buf) - 1)
                key_buf[klen++] = (char)yaml_ps_advance(s);
            else
                yaml_ps_advance(s);
        }
        key_buf[klen] = '\0';

        yaml_ps_skip_ws(s);
        if (s->pos < s->len && yaml_ps_peek(s) == ':') {
            yaml_ps_advance(s);
            yaml_ps_skip_ws(s);
        }

        if (prefix && prefix[0]) {
            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key_buf);
        } else {
            snprintf(full_key, sizeof(full_key), "%s", key_buf);
        }

        if (strcmp(key_buf, "<<") == 0) {
            yaml_parse_value(s, ind, prefix, ctx);
            continue;
        }

        if (s->pos >= s->len || yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
            yaml_ps_skip_eol(s);

            while (s->pos < s->len) {
                if (yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
                    yaml_ps_skip_eol(s);
                } else {
                    break;
                }
            }
            if (s->pos < s->len) {
                int next_ind = yaml_ps_count_indent(s);
                if (next_ind > ind) {
                    yaml_parse_value(s, ind, full_key, ctx);
                    continue;
                }
            }
            config_value_t *cv = config_value_create_string("");
            if (cv)
                config_context_set(ctx, full_key, cv);
            continue;
        }

        yaml_parse_value(s, ind, full_key, ctx);
    }
    return CONFIG_SUCCESS;
}

config_error_t yaml_parse_sequence(yaml_parse_state_t *s, int base_indent, const char *prefix,
                                   config_context_t *ctx)
{
    int idx = 0;
    while (s->pos < s->len) {
        while (s->pos < s->len) {
            if (yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
                yaml_ps_skip_eol(s);
            } else {
                break;
            }
        }
        if (s->pos >= s->len)
            break;

        int ind = yaml_ps_count_indent(s);
        if (ind <= base_indent)
            break;

        if (yaml_ps_peek(s) != '-')
            break;
        yaml_ps_advance(s);

        int next_c = yaml_ps_peek(s);
        if (next_c == '-' && s->pos + 1 < s->len && (unsigned char)s->src[s->pos + 1] == '-')
            break;

        yaml_ps_skip_ws(s);

        char idx_key[1024];
        snprintf(idx_key, sizeof(idx_key), "%s.%d", prefix, idx);

        if (s->pos >= s->len || yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
            yaml_ps_skip_eol(s);

            while (s->pos < s->len) {
                if (yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
                    yaml_ps_skip_eol(s);
                } else {
                    break;
                }
            }
            if (s->pos < s->len) {
                int next_ind = yaml_ps_count_indent(s);
                if (next_ind > ind) {
                    yaml_parse_value(s, ind, idx_key, ctx);
                    idx++;
                    continue;
                }
            }
            config_value_t *cv = config_value_create_string("");
            if (cv)
                config_context_set(ctx, idx_key, cv);
            idx++;
            continue;
        }

        yaml_parse_value(s, ind, idx_key, ctx);
        idx++;
    }
    return CONFIG_SUCCESS;
}

config_error_t yaml_parse_value(yaml_parse_state_t *s, int base_indent, const char *prefix,
                                config_context_t *ctx)
{
    yaml_ps_skip_ws(s);
    if (s->pos >= s->len)
        return CONFIG_SUCCESS;

    int c;
    if (yaml_ps_skip_anchor_tag(s, &c))
        return CONFIG_SUCCESS;

    if (c == '"' || c == '\'')
        return yaml_parse_quoted(s, c, prefix, ctx);

    if (c == '|' || c == '>')
        return yaml_parse_block_scalar(s, prefix, ctx);

    if (c == '-') {
        if (s->pos + 1 < s->len) {
            int next = (unsigned char)s->src[s->pos + 1];
            if (next == ' ' || next == '\t' || next == '\n' || next == '\r') {
                return yaml_parse_sequence(s, base_indent, prefix, ctx);
            }
        }
    }

    if (c == '[')
        return yaml_parse_inline_sequence(s, prefix, ctx);

    if (c == '{')
        return yaml_parse_inline_mapping(s, prefix, ctx);

    return yaml_parse_plain_scalar_value(s, base_indent, prefix, ctx);
}

/* ==================== Public entry point ==================== */

config_error_t config_parse_yaml(const char *data, size_t data_len, config_context_t *ctx)
{
    if (!data || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    yaml_parse_state_t state;
    state.src = data;
    state.len = data_len;
    state.pos = 0;
    state.line = 1;

    if (data_len >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF) {
        state.pos = 3;
    }

    while (state.pos < state.len) {
        if (yaml_ps_peek(&state) == '%') {
            yaml_ps_skip_to_eol(&state);
            yaml_ps_skip_eol(&state);
        } else if (yaml_ps_peek(&state) == ' ' || yaml_ps_peek(&state) == '\t' ||
                   yaml_ps_peek(&state) == '\n' || yaml_ps_peek(&state) == '\r') {
            yaml_ps_advance(&state);
        } else
            break;
    }

    if (state.pos + 3 <= state.len && memcmp(state.src + state.pos, "---", 3) == 0) {
        char after = (state.pos + 3 < state.len) ? state.src[state.pos + 3] : '\0';
        if (after == ' ' || after == '\t' || after == '\n' || after == '\r' || after == '\0') {
            state.pos += 3;
            yaml_ps_skip_to_eol(&state);
            yaml_ps_skip_eol(&state);
        }
    }

    return yaml_parse_value(&state, -1, "", ctx);
}
