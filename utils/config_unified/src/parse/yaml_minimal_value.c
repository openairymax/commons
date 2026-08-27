// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_value.c
 * @brief YAML 1.1 parser - scalar and flow value parsing.
 *
 * Implements the YAML value layer: alias resolution, quoted strings,
 * block scalars (literal | / folded > with chomping), and flow
 * (inline) sequences/mappings, single responsibility. Split out of
 * yaml_minimal_parser.c.
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"

/* Block scalar chomping modes: clip (single trailing newline), strip
 * (no trailing newline) and keep (all trailing newlines). */
enum { CHOMP_CLIP, CHOMP_STRIP, CHOMP_KEEP };

static void yaml_skip_eol(struct parse_ctx *ctx)
{
    if (!at_end(ctx) && peek(ctx) == '\r')
        advance(ctx);
    if (!at_end(ctx) && peek(ctx) == '\n')
        advance(ctx);
}

static bool yaml_buf_append(char **buf, size_t *cap, size_t *len, char ch)
{
    if (*len + 2 >= *cap) {
        size_t new_cap = *cap * 2;
        char *nb = (char *)yaml_safe_realloc(*buf, new_cap);
        if (!nb) {
            *buf = NULL;
            return false;
        }
        *cap = new_cap;
        *buf = nb;
    }
    (*buf)[(*len)++] = ch;
    return true;
}

struct yaml_node *parse_alias_value(struct parse_ctx *ctx, char *tag, char *anchor_name)
{
    advance(ctx);
    char *alias_name = parse_anchor_name(ctx);
    struct yaml_node *aliased = lookup_anchor(ctx, alias_name);
    if (!aliased) {
        set_error(ctx, "line %d: unknown alias '%s'", ctx->line, alias_name);
        AIRY_FREE(alias_name);
        AIRY_FREE(tag);
        AIRY_FREE(anchor_name);
        return NULL;
    }
    struct yaml_node *copy = deep_copy_node(ctx->doc, aliased);
    if (copy) {
        if (tag) {
            AIRY_FREE(copy->tag);
            copy->tag = tag;
            tag = NULL;
        }
        if (anchor_name) {
            register_anchor(ctx, anchor_name, copy);
        }
    }
    AIRY_FREE(alias_name);
    AIRY_FREE(anchor_name);
    AIRY_FREE(tag);
    return copy;
}

struct yaml_node *parse_quoted_value(struct parse_ctx *ctx, int quote, char *tag,
                                     char *anchor_name)
{
    char *s = parse_quoted_string(ctx, (char)quote);
    struct yaml_node *n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
    if (n) {
        n->scalar.value = s;
        n->scalar.length = s ? strlen(s) : 0;
        if (anchor_name)
            register_anchor(ctx, anchor_name, n);
        if (tag) {
            n->tag = tag;
            tag = NULL;
        }
    }
    AIRY_FREE(anchor_name);
    AIRY_FREE(tag);
    return n;
}

static void parse_block_header(struct parse_ctx *ctx, int *chomp, int *block_indent)
{
    *chomp = CHOMP_CLIP;
    *block_indent = -1;

    while (!at_end(ctx)) {
        char mod = peek(ctx);
        if (mod == '-') {
            *chomp = CHOMP_STRIP;
            advance(ctx);
        } else if (mod == '+') {
            *chomp = CHOMP_KEEP;
            advance(ctx);
        } else if (mod >= '1' && mod <= '9') {
            *block_indent = mod - '0';
            advance(ctx);
        } else
            break;
    }

    skip_ws(ctx);
    if (!at_end(ctx) && peek(ctx) == '#') {
        while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r')
            advance(ctx);
    }
    yaml_skip_eol(ctx);
}

static void yaml_append_separator(bool is_folded, char **buf, size_t *cap, size_t *len)
{
    if (is_folded) {
        if (!(*len > 0 && (*buf)[*len - 1] == '\n'))
            yaml_buf_append(buf, cap, len, ' ');
    } else {
        yaml_buf_append(buf, cap, len, '\n');
    }
}

/* Read a single block scalar line into buf; returns false when the block
 * ends (EOF or dedent) or when the growing buffer ran out of memory. */
static bool yaml_read_block_line(struct parse_ctx *ctx, bool is_folded, int *block_indent,
                                 char **buf, size_t *cap, size_t *len,
                                 int *first_line_indent, bool *has_content)
{
    int line_ind = 0;
    while (!at_end(ctx) && peek(ctx) == ' ') {
        line_ind++;
        advance(ctx);
    }

    if (at_end(ctx))
        return false;

    char line_char = peek(ctx);
    if (line_char == '\n' || line_char == '\r') {
        yaml_skip_eol(ctx);
        if (!yaml_buf_append(buf, cap, len, '\n'))
            return false;
        return true;
    }

    if (line_char == ' ' || line_char == '\t')
        return true;

    if (*block_indent < 0) {
        if (*first_line_indent < 0) {
            *first_line_indent = line_ind;
            *block_indent = line_ind;
        }
    }

    if (line_ind < *block_indent && line_char != '\0')
        return false;

    if (*has_content)
        yaml_append_separator(is_folded, buf, cap, len);

    while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r') {
        if (!yaml_buf_append(buf, cap, len, (char)advance(ctx)))
            return false;
    }
    *has_content = true;
    yaml_skip_eol(ctx);
    return true;
}

static void yaml_read_block_lines(struct parse_ctx *ctx, bool is_folded, int block_indent,
                                  char **buf, size_t *cap, size_t *len)
{
    int first_line_indent = -1;
    bool has_content = false;
    while (!at_end(ctx)) {
        if (!yaml_read_block_line(ctx, is_folded, &block_indent, buf, cap, len,
                                  &first_line_indent, &has_content))
            break;
    }
}

static void yaml_apply_chomp(int chomp, char **buf, size_t *cap, size_t *len)
{
    switch (chomp) {
    case CHOMP_STRIP:
        while (*len > 0 && (*buf)[*len - 1] == '\n')
            (*len)--;
        break;
    case CHOMP_KEEP:
        break;
    case CHOMP_CLIP:
    default:
        if (*len > 0 && (*buf)[*len - 1] != '\n')
            yaml_buf_append(buf, cap, len, '\n');
        break;
    }
}

struct yaml_node *parse_block_scalar_value(struct parse_ctx *ctx, bool is_folded,
                                           char *tag, char *anchor_name)
{
    advance(ctx);

    int chomp;
    int block_indent;
    parse_block_header(ctx, &chomp, &block_indent);

    size_t cap = 1024;
    size_t len = 0;
    char *buf = (char *)AIRY_MALLOC(cap);
    if (!buf)
        return NULL;

    yaml_read_block_lines(ctx, is_folded, block_indent, &buf, &cap, &len);
    if (!buf)
        return NULL;
    yaml_apply_chomp(chomp, &buf, &cap, &len);
    if (!buf)
        return NULL;

    buf[len] = '\0';
    struct yaml_node *n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
    if (n) {
        n->scalar.value = buf;
        n->scalar.length = len;
        if (anchor_name)
            register_anchor(ctx, anchor_name, n);
        if (tag) {
            n->tag = tag;
            tag = NULL;
        }
    }
    AIRY_FREE(anchor_name);
    AIRY_FREE(tag);
    return n;
}

struct yaml_node *parse_inline_sequence_value(struct parse_ctx *ctx, int base_indent,
                                              char *tag, char *anchor_name)
{
    advance(ctx);
    struct yaml_node *seq = alloc_node(ctx->doc, YAML_NODE_SEQUENCE);
    size_t cap = 4;
    seq->sequence.items =
        (struct yaml_sequence_item *)AIRY_CALLOC(cap, sizeof(struct yaml_sequence_item));
    seq->sequence.count = 0;
    skip_ws(ctx);
    while (!at_end(ctx) && peek(ctx) != ']') {
        if (seq->sequence.count > 0) {
            if (peek(ctx) == ',')
                advance(ctx);
            skip_ws(ctx);
        }
        struct yaml_node *item = parse_value(ctx, base_indent + 100);
        if (item) {
            if (seq->sequence.count >= cap) {
                cap *= 2;
                seq->sequence.items = (struct yaml_sequence_item *)yaml_safe_realloc(
                    seq->sequence.items, cap * sizeof(struct yaml_sequence_item));
                if (!seq->sequence.items)
                    return NULL;
            }
            seq->sequence.items[seq->sequence.count++].item = item;
        }
        skip_ws(ctx);
    }
    if (peek(ctx) == ']')
        advance(ctx);
    if (anchor_name)
        register_anchor(ctx, anchor_name, seq);
    if (tag) {
        AIRY_FREE(seq->tag);
        seq->tag = tag;
        tag = NULL;
    }
    AIRY_FREE(anchor_name);
    AIRY_FREE(tag);
    return seq;
}

struct yaml_node *parse_inline_mapping_value(struct parse_ctx *ctx, int base_indent,
                                             char *tag, char *anchor_name)
{
    advance(ctx);
    struct yaml_node *map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
    size_t cap = 4;
    map->mapping =
        (struct yaml_mapping_entry *)AIRY_CALLOC(cap, sizeof(struct yaml_mapping_entry));
    size_t msz = 0;
    skip_ws(ctx);
    while (!at_end(ctx) && peek(ctx) != '}') {
        if (msz > 0) {
            if (peek(ctx) == ',')
                advance(ctx);
            skip_ws(ctx);
        }
        char *k = parse_plain_scalar(ctx, base_indent + 100);
        if (!k)
            break;
        skip_ws(ctx);
        if (peek(ctx) == ':')
            advance(ctx);
        skip_ws(ctx);
        struct yaml_node *v = parse_value(ctx, base_indent + 100);
        if (!v)
            v = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (v && v->type == YAML_NODE_NONE)
            v->type = YAML_NODE_SCALAR;
        if (v && v->type == YAML_NODE_SCALAR && !v->scalar.value)
            v->scalar.value = AIRY_STRDUP("");
        if (msz >= cap) {
            cap *= 2;
            map->mapping = (struct yaml_mapping_entry *)
                yaml_safe_realloc(map->mapping, cap * sizeof(struct yaml_mapping_entry));
            if (!map->mapping)
                return NULL;
        }
        map->mapping[msz].key = k;
        map->mapping[msz].value = v;
        msz++;
        skip_ws(ctx);
    }
    if (peek(ctx) == '}')
        advance(ctx);
    if (anchor_name)
        register_anchor(ctx, anchor_name, map);
    if (tag) {
        AIRY_FREE(map->tag);
        map->tag = tag;
        tag = NULL;
    }
    AIRY_FREE(anchor_name);
    AIRY_FREE(tag);
    return map;
}
