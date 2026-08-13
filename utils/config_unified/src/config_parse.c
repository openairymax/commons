// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse.c
 * @brief Unified config module - format parsing layer (JSON/INI/YAML).
 *
 * Standalone parsing module split out of config_source.c; implements
 * pure string parsing for three formats, dispatched by format string:
 *   - config_parse_json : strict recursive JSON object/array parsing
 *   - config_parse_ini  : classic INI section/key-value parsing
 *   - config_parse_yaml : indentation-aware YAML subset parsing
 *
 * Parsing results are written into config_context_t
 * (dotted key -> config_value_t).
 */

#include "config_source.h"
#include "core_config.h"
#include "logging_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"

/* ==================== JSON parsing ==================== */

static config_error_t parse_json_value(const char **pp, const char *end, config_value_t **out);
static config_error_t parse_json_object(const char **pp, const char *end, config_context_t *ctx,
                                        const char *prefix);
static config_error_t parse_json_array(const char **pp, const char *end, config_value_t **out);

static void skip_whitespace(const char **pp, const char *end)
{
    while (*pp < end && (**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r'))
        (*pp)++;
}

static config_error_t parse_json_string(const char **pp, const char *end, char *buf,
                                        size_t buf_size)
{
    if (**pp != '"')
        return CONFIG_ERROR_PARSE;
    (*pp)++;
    size_t len = 0;
    while (*pp < end && **pp != '"') {
        if (**pp == '\\') {
            (*pp)++;
            if (*pp >= end)
                break;
            switch (**pp) {
            case '"':
                if (len < buf_size - 1)
                    buf[len++] = '"';
                break;
            case '\\':
                if (len < buf_size - 1)
                    buf[len++] = '\\';
                break;
            case '/':
                if (len < buf_size - 1)
                    buf[len++] = '/';
                break;
            case 'n':
                if (len < buf_size - 1)
                    buf[len++] = '\n';
                break;
            case 'r':
                if (len < buf_size - 1)
                    buf[len++] = '\r';
                break;
            case 't':
                if (len < buf_size - 1)
                    buf[len++] = '\t';
                break;
            case 'b':
                if (len < buf_size - 1)
                    buf[len++] = '\b';
                break;
            case 'f':
                if (len < buf_size - 1)
                    buf[len++] = '\f';
                break;
            case 'u': {
                if (*pp + 4 < end) {
                    unsigned int code = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = *(*pp + 1 + (size_t)i);
                        code <<= 4;
                        if (h >= '0' && h <= '9')
                            code |= (unsigned int)(h - '0');
                        else if (h >= 'a' && h <= 'f')
                            code |= (unsigned int)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F')
                            code |= (unsigned int)(h - 'A' + 10);
                        else
                            return CONFIG_ERROR_PARSE;
                    }
                    *pp += 4;
                    if (code < 0x80) {
                        if (len < buf_size - 1)
                            buf[len++] = (char)code;
                    } else if (code < 0x800) {
                        if (len < buf_size - 2) {
                            buf[len++] = (char)(0xC0 | (code >> 6));
                            buf[len++] = (char)(0x80 | (code & 0x3F));
                        }
                    } else {
                        if (len < buf_size - 3) {
                            buf[len++] = (char)(0xE0 | (code >> 12));
                            buf[len++] = (char)(0x80 | ((code >> 6) & 0x3F));
                            buf[len++] = (char)(0x80 | (code & 0x3F));
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
            (*pp)++;
        } else {
            if (len < buf_size - 1)
                buf[len++] = **pp;
            (*pp)++;
        }
    }
    buf[len] = '\0';
    if (*pp < end && **pp == '"')
        (*pp)++;
    return CONFIG_SUCCESS;
}

static config_error_t parse_json_value(const char **pp, const char *end, config_value_t **out)
{
    skip_whitespace(pp, end);
    if (*pp >= end)
        return CONFIG_ERROR_PARSE;

    if (**pp == '"') {
        char buf[4096];
        config_error_t err = parse_json_string(pp, end, buf, sizeof(buf));
        if (err != CONFIG_SUCCESS)
            return err;
        *out = config_value_create_string(buf);
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (**pp == '-' || (**pp >= '0' && **pp <= '9')) {
        const char *num_start = *pp;
        if (**pp == '-')
            (*pp)++;
        while (*pp < end && **pp >= '0' && **pp <= '9')
            (*pp)++;
        bool is_float = false;
        if (*pp < end && **pp == '.') {
            is_float = true;
            (*pp)++;
            while (*pp < end && **pp >= '0' && **pp <= '9')
                (*pp)++;
        }
        if (*pp < end && (**pp == 'e' || **pp == 'E')) {
            is_float = true;
            (*pp)++;
            if (*pp < end && (**pp == '+' || **pp == '-'))
                (*pp)++;
            while (*pp < end && **pp >= '0' && **pp <= '9')
                (*pp)++;
        }
        char num_buf[64];
        size_t nlen = (size_t)(*pp - num_start);
        if (nlen >= sizeof(num_buf))
            nlen = sizeof(num_buf) - 1;
        __builtin_memcpy(num_buf, num_start, nlen);
        num_buf[nlen] = '\0';
        if (is_float) {
            *out = config_value_create_double(atof(num_buf));
        } else {
            *out = config_value_create_int((int32_t)atol(num_buf));
        }
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (strncmp(*pp, "true", 4) == 0) {
        *out = config_value_create_bool(true);
        *pp += 4;
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (strncmp(*pp, "false", 5) == 0) {
        *out = config_value_create_bool(false);
        *pp += 5;
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (strncmp(*pp, "null", 4) == 0) {
        *pp += 4;
        *out = config_value_create_string("");
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (**pp == '[') {
        return parse_json_array(pp, end, out);
    } else if (**pp == '{') {
        config_context_t *sub_ctx = config_context_create(NULL);
        if (!sub_ctx)
            return CONFIG_ERROR_OUT_OF_MEMORY;
        config_error_t err = parse_json_object(pp, end, sub_ctx, NULL);
        if (err != CONFIG_SUCCESS) {
            config_context_destroy(sub_ctx);
            return err;
        }
        *out = config_value_create_object(16);
        if (!*out) {
            config_context_destroy(sub_ctx);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
        config_context_destroy(sub_ctx);
        return CONFIG_SUCCESS;
    }
    return CONFIG_ERROR_PARSE;
}

static config_error_t parse_json_array(const char **pp, const char *end, config_value_t **out)
{
    if (**pp != '[')
        return CONFIG_ERROR_PARSE;
    (*pp)++;
    *out = config_value_create_array(16);
    if (!*out)
        return CONFIG_ERROR_OUT_OF_MEMORY;

    while (*pp < end) {
        skip_whitespace(pp, end);
        if (*pp >= end || **pp == ']') {
            (*pp)++;
            return CONFIG_SUCCESS;
        }
        if (**pp == ',') {
            (*pp)++;
            continue;
        }

        config_value_t *item = NULL;
        config_error_t err = parse_json_value(pp, end, &item);
        if (err != CONFIG_SUCCESS || !item)
            continue;

        config_value_array_append(*out, item);
    }
    return CONFIG_SUCCESS;
}

static config_error_t parse_json_object(const char **pp, const char *end, config_context_t *ctx,
                                        const char *prefix)
{
    if (**pp != '{')
        return CONFIG_ERROR_PARSE;
    (*pp)++;

    while (*pp < end) {
        skip_whitespace(pp, end);
        if (*pp >= end || **pp == '}') {
            (*pp)++;
            return CONFIG_SUCCESS;
        }
        if (**pp == ',') {
            (*pp)++;
            continue;
        }
        if (**pp != '"') {
            (*pp)++;
            continue;
        }

        char key[512];
        config_error_t err = parse_json_string(pp, end, key, sizeof(key));
        if (err != CONFIG_SUCCESS)
            continue;

        skip_whitespace(pp, end);
        if (*pp < end && **pp == ':')
            (*pp)++;
        skip_whitespace(pp, end);

        char full_key[768];
        if (prefix && prefix[0]) {
            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key);
        } else {
            snprintf(full_key, sizeof(full_key), "%s", key);
        }

        if (*pp < end && **pp == '{') {
            config_error_t err2 = parse_json_object(pp, end, ctx, full_key);
            if (err2 != CONFIG_SUCCESS)
                continue;
        } else {
            config_value_t *value = NULL;
            err = parse_json_value(pp, end, &value);
            if (err != CONFIG_SUCCESS || !value)
                continue;
            config_context_set(ctx, full_key, value);
        }
    }
    return CONFIG_SUCCESS;
}

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

/* ==================== YAML parsing ==================== */

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int line;
} yaml_parse_state_t;

static int yaml_ps_peek(yaml_parse_state_t *s)
{
    return (s->pos < s->len) ? (unsigned char)s->src[s->pos] : -1;
}

static int yaml_ps_advance(yaml_parse_state_t *s)
{
    if (s->pos >= s->len)
        return AIRY_EINVAL;
    int c = (unsigned char)s->src[s->pos++];
    if (c == '\n')
        s->line++;
    return c;
}

static void yaml_ps_skip_ws(yaml_parse_state_t *s)
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

static int yaml_ps_count_indent(yaml_parse_state_t *s)
{
    int indent = 0;
    while (s->pos < s->len && yaml_ps_peek(s) == ' ') {
        indent++;
        yaml_ps_advance(s);
    }
    return indent;
}

static void yaml_ps_skip_to_eol(yaml_parse_state_t *s)
{
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == '\n' || c == '\r')
            break;
        yaml_ps_advance(s);
    }
}

static void yaml_ps_skip_eol(yaml_parse_state_t *s)
{
    if (s->pos < s->len && yaml_ps_peek(s) == '\r')
        yaml_ps_advance(s);
    if (s->pos < s->len && yaml_ps_peek(s) == '\n')
        yaml_ps_advance(s);
}

static config_error_t yaml_parse_value(yaml_parse_state_t *s, int base_indent, const char *prefix,
                                       config_context_t *ctx);

static config_error_t yaml_parse_mapping(yaml_parse_state_t *s, int base_indent, const char *prefix,
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

static config_error_t yaml_parse_sequence(yaml_parse_state_t *s, int base_indent,
                                          const char *prefix, config_context_t *ctx)
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
static bool yaml_ps_skip_anchor_tag(yaml_parse_state_t *s, int *c)
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

static config_error_t yaml_parse_quoted(yaml_parse_state_t *s, int quote,
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

static config_error_t yaml_parse_block_scalar(yaml_parse_state_t *s, const char *prefix,
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

static config_error_t yaml_parse_inline_sequence(yaml_parse_state_t *s, const char *prefix,
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

static config_error_t yaml_parse_inline_mapping(yaml_parse_state_t *s, const char *prefix,
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

static config_error_t yaml_parse_plain_scalar_value(yaml_parse_state_t *s, int base_indent,
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

static config_error_t yaml_parse_value(yaml_parse_state_t *s, int base_indent, const char *prefix,
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

/* ==================== Public entry points ==================== */

config_error_t config_parse_json(const char *data, size_t data_len, config_context_t *ctx)
{
    if (!data || data_len == 0 || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    const char *p = data;
    const char *end = data + data_len;
    skip_whitespace(&p, end);
    if (p >= end)
        return CONFIG_SUCCESS;
    if (*p == '{')
        return parse_json_object(&p, end, ctx, NULL);
    return CONFIG_ERROR_PARSE;
}

config_error_t config_parse_ini(const char *data, size_t data_len, config_context_t *ctx)
{
    return parse_ini_simple(data, data_len, ctx);
}

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
