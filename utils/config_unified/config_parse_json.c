// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse_json.c
 * @brief Unified config module - JSON flattening onto config_context_t.
 *
 * 手写递归下降 JSON 解析，语义与 config_parse_yaml 完全对齐：对象展开为
 * a.b.c 层级键，数组展开为 a.0/a.1 数字索引键，空容器与 null 落为空字符
 * 串值。点分路径溢出（键超长）按解析错误处置，不做静默截断；数字解析不
 * 依赖 locale，也不经中间缓冲，避免截断。
 */

#include "config_parse_internal.h"

/* ==================== JSON parsing ==================== */

static config_error_t parse_json_value(const char **pp, const char *end, config_context_t *ctx,
                                       const char *key);
static config_error_t parse_json_scalar(const char **pp, const char *end, config_context_t *ctx,
                                        const char *key);

static void skip_whitespace(const char **pp, const char *end)
{
    while (*pp < end && (**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r'))
        (*pp)++;
}

/* Build "prefix.key"; returns false when the dot path would overflow. */
static bool join_key(char *buf, size_t n, const char *prefix, const char *key)
{
    int r = (prefix && prefix[0]) ? snprintf(buf, n, "%s.%s", prefix, key)
                                  : snprintf(buf, n, "%s", key);
    return r > 0 && (size_t)r < n;
}

/* Build "prefix.index"; returns false when the dot path would overflow. */
static bool join_index(char *buf, size_t n, const char *prefix, size_t index)
{
    int r = (prefix && prefix[0]) ? snprintf(buf, n, "%s.%zu", prefix, index)
                                  : snprintf(buf, n, "%zu", index);
    return r > 0 && (size_t)r < n;
}

/* Decode a single-character JSON escape; '\0' means the escape either
 * needs richer handling (\\u) or is unknown and must be dropped. */
static char json_escape_char(char c)
{
    switch (c) {
    case '"':
        return '"';
    case '\\':
        return '\\';
    case '/':
        return '/';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    default:
        return '\0';
    }
}

static config_error_t parse_json_string(const char **pp, const char *end, char *buf,
                                        size_t buf_size)
{
    if (**pp != '"')
        return CONFIG_ERROR_PARSE;
    (*pp)++;
    size_t len = 0;
    while (*pp < end && **pp != '"') {
        if (**pp != '\\') {
            if (len < buf_size - 1)
                buf[len++] = **pp;
            (*pp)++;
            continue;
        }
        (*pp)++;
        if (*pp >= end)
            break;
        char esc = json_escape_char(**pp);
        if (esc != '\0') {
            if (len < buf_size - 1)
                buf[len++] = esc;
        } else if (**pp == 'u') {
            if (*pp + 4 >= end)
                return CONFIG_ERROR_PARSE;
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
        (*pp)++;
    }
    buf[len] = '\0';
    if (*pp >= end || **pp != '"')
        return CONFIG_ERROR_PARSE;
    (*pp)++;
    return CONFIG_SUCCESS;
}

/* Advance past a run of ASCII decimal digits. */
static const char *skip_digits(const char *p, const char *e)
{
    while (p < e && *p >= '0' && *p <= '9')
        p++;
    return p;
}

/* Consume one JSON number token in place; report whether it is a float.
 * The sign, if any, is handled here, so callers pass the token start. */
static const char *scan_number(const char *p, const char *e, bool *is_float)
{
    bool flt = false;

    if (p < e && *p == '-')
        p++;
    p = skip_digits(p, e);
    if (p < e && *p == '.') {
        flt = true;
        p = skip_digits(p + 1, e);
    }
    if (p < e && (*p == 'e' || *p == 'E')) {
        flt = true;
        p++;
        if (p < e && (*p == '+' || *p == '-'))
            p++;
        p = skip_digits(p, e);
    }
    *is_float = flt;
    return p;
}

/* Accumulate integer digits with saturation at INT64_MAX. */
static int64_t num_int(const char *p, const char *e, bool neg)
{
    int64_t v = 0;

    for (; p < e; p++) {
        int d = *p - '0';
        if (v > (INT64_MAX - d) / 10) {
            v = INT64_MAX;
            break;
        }
        v = v * 10 + d;
    }
    return neg ? -v : v;
}

/* Decode the optional exponent suffix of a JSON number; 0 when absent. */
static int num_exp(const char *p, const char *e)
{
    int exp = 0;

    if (p >= e || (*p != 'e' && *p != 'E'))
        return 0;
    p++;
    bool neg = false;
    if (p < e && (*p == '+' || *p == '-')) {
        neg = (*p == '-');
        p++;
    }
    for (; p < e && *p >= '0' && *p <= '9'; p++) {
        if (exp < 1000000)
            exp = exp * 10 + (*p - '0');
    }
    return neg ? -exp : exp;
}

/* Locale-independent float parse over the already-scanned signless token. */
static double num_float(const char *p, const char *e)
{
    double v = 0.0;

    for (; p < e && *p >= '0' && *p <= '9'; p++)
        v = v * 10.0 + (double)(*p - '0');
    if (p < e && *p == '.') {
        double scale = 0.1;
        for (p++; p < e && *p >= '0' && *p <= '9'; p++) {
            v += (double)(*p - '0') * scale;
            scale *= 0.1;
        }
    }
    int exp = num_exp(p, e);
    while (exp > 0) {
        v *= 10.0;
        exp--;
    }
    while (exp < 0) {
        v /= 10.0;
        exp++;
    }
    return v;
}

/* Locale-independent JSON number build: the C atof/strtod honour the
 * active locale's decimal separator, but the JSON grammar is always an
 * ASCII '.'. The token is walked in place, so no fixed-width buffer can
 * truncate the value. */
static config_value_t *make_number(const char *s, const char *e, bool is_float)
{
    bool neg = (*s == '-');
    const char *p = neg ? s + 1 : s;

    if (!is_float) {
        int64_t v = num_int(p, e, neg);
        if (v >= INT32_MIN && v <= INT32_MAX)
            return config_value_create_int((int32_t)v);
        return config_value_create_int64(v);
    }
    return config_value_create_double(neg ? -num_float(p, e) : num_float(p, e));
}

/* Store a freshly built value under key; an empty key (a container at the
 * document root) has nowhere to go and is dropped. */
static config_error_t store_value(config_context_t *ctx, const char *key, config_value_t *cv)
{
    if (!cv)
        return CONFIG_ERROR_OUT_OF_MEMORY;
    if (!key || !key[0]) {
        config_value_destroy(cv);
        return CONFIG_SUCCESS;
    }
    config_error_t err = config_context_set(ctx, key, cv);
    if (err != CONFIG_SUCCESS)
        config_value_destroy(cv);
    return err;
}

static config_error_t parse_json_scalar(const char **pp, const char *end, config_context_t *ctx,
                                        const char *key)
{
    const char *p = *pp;

    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        bool is_float = false;
        const char *start = p;
        const char *tok = scan_number(p, end, &is_float);
        *pp = tok;
        return store_value(ctx, key, make_number(start, tok, is_float));
    }
    if ((size_t)(end - p) >= 4 && strncmp(p, "true", 4) == 0) {
        *pp = p + 4;
        return store_value(ctx, key, config_value_create_bool(true));
    }
    if ((size_t)(end - p) >= 5 && strncmp(p, "false", 5) == 0) {
        *pp = p + 5;
        return store_value(ctx, key, config_value_create_bool(false));
    }
    if ((size_t)(end - p) >= 4 && strncmp(p, "null", 4) == 0) {
        *pp = p + 4;
        return store_value(ctx, key, config_value_create_string(""));
    }
    return CONFIG_ERROR_PARSE;
}

static config_error_t parse_json_array(const char **pp, const char *end, config_context_t *ctx,
                                       const char *prefix)
{
    if (**pp != '[')
        return CONFIG_ERROR_PARSE;
    (*pp)++;
    skip_whitespace(pp, end);
    if (*pp < end && **pp == ']') {
        (*pp)++;
        return store_value(ctx, prefix, config_value_create_string(""));
    }

    size_t index = 0;
    while (*pp < end) {
        char idx_key[768];
        if (!join_index(idx_key, sizeof(idx_key), prefix, index))
            return CONFIG_ERROR_PARSE;

        config_error_t err = parse_json_value(pp, end, ctx, idx_key);
        if (err != CONFIG_SUCCESS)
            return err;
        index++;

        skip_whitespace(pp, end);
        if (*pp < end && **pp == ',') {
            (*pp)++;
            continue;
        }
        if (*pp < end && **pp == ']') {
            (*pp)++;
            return CONFIG_SUCCESS;
        }
        return CONFIG_ERROR_PARSE;
    }
    return CONFIG_ERROR_PARSE;
}

static config_error_t parse_json_object(const char **pp, const char *end, config_context_t *ctx,
                                        const char *prefix)
{
    if (**pp != '{')
        return CONFIG_ERROR_PARSE;
    (*pp)++;
    skip_whitespace(pp, end);
    if (*pp < end && **pp == '}') {
        (*pp)++;
        return store_value(ctx, prefix, config_value_create_string(""));
    }

    while (*pp < end) {
        skip_whitespace(pp, end);
        if (*pp >= end || **pp != '"')
            return CONFIG_ERROR_PARSE;

        char key[512];
        config_error_t err = parse_json_string(pp, end, key, sizeof(key));
        if (err != CONFIG_SUCCESS)
            return err;

        skip_whitespace(pp, end);
        if (*pp >= end || **pp != ':')
            return CONFIG_ERROR_PARSE;
        (*pp)++;

        char full_key[768];
        if (!join_key(full_key, sizeof(full_key), prefix, key))
            return CONFIG_ERROR_PARSE;

        err = parse_json_value(pp, end, ctx, full_key);
        if (err != CONFIG_SUCCESS)
            return err;

        skip_whitespace(pp, end);
        if (*pp < end && **pp == ',') {
            (*pp)++;
            continue;
        }
        if (*pp < end && **pp == '}') {
            (*pp)++;
            return CONFIG_SUCCESS;
        }
        return CONFIG_ERROR_PARSE;
    }
    return CONFIG_ERROR_PARSE;
}

static config_error_t parse_json_value(const char **pp, const char *end, config_context_t *ctx,
                                       const char *key)
{
    skip_whitespace(pp, end);
    if (*pp >= end)
        return CONFIG_ERROR_PARSE;

    switch (**pp) {
    case '{':
        return parse_json_object(pp, end, ctx, key);
    case '[':
        return parse_json_array(pp, end, ctx, key);
    case '"': {
        char buf[4096];
        config_error_t err = parse_json_string(pp, end, buf, sizeof(buf));
        if (err != CONFIG_SUCCESS)
            return err;
        return store_value(ctx, key, config_value_create_string(buf));
    }
    default:
        return parse_json_scalar(pp, end, ctx, key);
    }
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
    if (*p != '{' && *p != '[')
        return CONFIG_ERROR_PARSE;
    return parse_json_value(&p, end, ctx, NULL);
}
