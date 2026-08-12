// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_lexer.c
 * @brief YAML 1.1 解析器 - 词法解析
 *
 * 本文件实现 YAML 词法层：字符流游标、空白/缩进跳过、
 * 引号/纯量/标签/锚名扫描与错误记录，单一职责。
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

void *yaml_safe_realloc(void *ptr, size_t size)
{
    void *tmp = AIRY_REALLOC(ptr, size);
    if (!tmp) {
        AIRY_FREE(ptr);
    }
    return tmp;
}

char *parse_tag(struct parse_ctx *ctx)
{
    if (peek(ctx) != '!')
        return NULL;
    advance(ctx);
    if (peek(ctx) == '!')
        advance(ctx);
    size_t cap = 64;
    size_t len = 0;
    char *buf = (char *)AIRY_MALLOC(cap);
    if (!buf)
        return NULL;
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ':')
            break;
        advance(ctx);
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

char *parse_anchor_name(struct parse_ctx *ctx)
{
    size_t cap = 64;
    size_t len = 0;
    char *buf = (char *)AIRY_MALLOC(cap);
    if (!buf)
        return NULL;
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ':')
            break;
        advance(ctx);
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

void set_error(struct parse_ctx *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    AIRY_FREE(ctx->doc->error_msg);
    ctx->doc->error_msg = AIRY_STRDUP(buf);
}

char peek(struct parse_ctx *ctx)
{
    if (ctx->pos >= ctx->len)
        return '\0';
    return ctx->src[ctx->pos];
}

char advance(struct parse_ctx *ctx)
{
    if (ctx->pos >= ctx->len)
        return '\0';
    char c = ctx->src[ctx->pos++];
    if (c == '\n') {
        ctx->line++;
        ctx->line_pos = 0;
    } else {
        ctx->line_pos++;
    }
    return c;
}

bool at_end(struct parse_ctx *ctx)
{
    return ctx->pos >= ctx->len;
}

void skip_ws(struct parse_ctx *ctx)
{
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t')
            advance(ctx);
        else
            break;
    }
}

void skip_ws_and_nl(struct parse_ctx *ctx)
{
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            advance(ctx);
        else
            break;
    }
}

int count_indent(struct parse_ctx *ctx)
{
    int indent = 0;
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ') {
            indent++;
            advance(ctx);
        } else if (c == '\t') {
            set_error(ctx, "line %d: YAML forbids tabs for indentation", ctx->line);
            advance(ctx);
            indent += 2;
        } else
            break;
    }
    return indent;
}

bool is_plain_scalar_char(char c)
{
    switch (c) {
    case ':':
    case '{':
    case '}':
    case '[':
    case ']':
    case ',':
    case '&':
    case '*':
    case '#':
    case '|':
    case '>':
    case '"':
    case '\'':
    case '%':
    case '@':
    case '`':
    case '\n':
    case '\r':
        return false;
    default:
        return !isspace((unsigned char)c);
    }
}

char *parse_quoted_string(struct parse_ctx *ctx, char quote)
{
    advance(ctx);
    size_t cap = 128;
    size_t len = 0;
    char *buf = (char *)AIRY_MALLOC(cap);
    if (!buf)
        return NULL;

    while (!at_end(ctx)) {
        char c = advance(ctx);
        if (c == quote && (len == 0 || buf[len - 1] != '\\'))
            break;
        if (c == '\\' && !at_end(ctx)) {
            char next = advance(ctx);
            switch (next) {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case '\\':
                c = '\\';
                break;
            case '\'':
                c = '\'';
                break;
            case '"':
                c = '"';
                break;
            default: /* keep both */
                break;
            }
        }
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

char *parse_plain_scalar(struct parse_ctx *ctx, int end_indent)
{
    size_t cap = 256;
    size_t len = 0;
    char *buf = (char *)AIRY_MALLOC(cap);
    if (!buf)
        return NULL;

    while (!at_end(ctx)) {
        char c = peek(ctx);

        if (c == ':' && (ctx->pos + 1 < ctx->len &&
                         (ctx->src[ctx->pos + 1] == ' ' || ctx->src[ctx->pos + 1] == '\n')))
            break;
        if (c == '#' && len > 0 && buf[len - 1] == ' ')
            break;
        if (c == '\n' || c == '\r')
            break;
        if (!is_plain_scalar_char(c))
            break;

        advance(ctx);
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }

    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t'))
        len--;
    buf[len] = '\0';

    if (len == 0) {
        AIRY_FREE(buf);
        return AIRY_STRDUP("");
    }
    return buf;
}
