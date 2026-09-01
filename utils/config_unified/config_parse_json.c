// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse_json.c
 * @brief Unified config module - JSON format parsing.
 *
 * 2026-08-27 域拆分（原 config_parse.c）：严格递归 JSON
 * object/array 解析，结果写入 config_context_t（点分键）。
 */

#include "config_parse_internal.h"

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

/* ==================== Public entry point ==================== */

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
