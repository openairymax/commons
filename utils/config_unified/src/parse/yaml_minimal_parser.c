// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_parser.c
 * @brief YAML 1.1 parser - syntax tree building (block structures).
 *
 * Implements the YAML syntax layer for block structures: sequence and
 * mapping parsing, merge-key resolution, value dispatch and document
 * markers/directives skipping. Node lifecycle, anchor registry and
 * scalar/flow value parsing live in yaml_minimal_anchor.c /
 * yaml_minimal_value.c (single responsibility per file).
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"

struct yaml_node *parse_sequence(struct parse_ctx *ctx, int base_indent)
{
    struct yaml_node *seq = alloc_node(ctx->doc, YAML_NODE_SEQUENCE);
    if (!seq)
        return NULL;

    size_t cap = 8;
    seq->sequence.items =
        (struct yaml_sequence_item *)AIRY_CALLOC(cap, sizeof(struct yaml_sequence_item));
    seq->sequence.count = 0;
    if (!seq->sequence.items)
        return NULL;

    do {
        skip_ws_nl_comments(ctx);
        if (at_end(ctx))
            break;

        int ind = line_indent(ctx);
        if (ind <= base_indent)
            break;

        if (peek(ctx) != '-')
            break;
        advance(ctx);
        char next_c = peek(ctx);
        if (next_c == ' ' || next_c == '\t' || next_c == '\n') {
            skip_ws(ctx);
        } else {
            ctx->pos--;
        }

        struct yaml_node *item = parse_value(ctx, ind);
        if (!item)
            continue;

        if (seq->sequence.count >= cap) {
            cap *= 2;
            seq->sequence.items = (struct yaml_sequence_item *)
                yaml_safe_realloc(seq->sequence.items, cap * sizeof(struct yaml_sequence_item));
            if (!seq->sequence.items)
                return NULL;
        }
        seq->sequence.items[seq->sequence.count++].item = item;
    } while (!at_end(ctx));

    return seq;
}

void merge_mapping_into(yaml_document_t *doc, struct yaml_node *target,
                        struct yaml_node *source)
{
    if (!doc || !target || !source || target->type != YAML_NODE_MAPPING ||
        source->type != YAML_NODE_MAPPING)
        return;

    size_t src_sz = yaml_size(source);
    size_t tgt_sz = yaml_size(target);
    size_t cap = tgt_sz + 1;
    while (cap < tgt_sz + src_sz)
        cap *= 2;

    target->mapping =
        (struct yaml_mapping_entry *)yaml_safe_realloc(target->mapping,
                                                       cap * sizeof(struct yaml_mapping_entry));
    if (!target->mapping)
        return;

    for (size_t i = 0; i < src_sz; i++) {
        bool found = false;
        size_t tgt_current = yaml_size(target);
        for (size_t j = 0; j < tgt_current; j++) {
            if (target->mapping[j].key && source->mapping[i].key &&
                strcmp(target->mapping[j].key, source->mapping[i].key) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (tgt_current >= cap) {
                cap *= 2;
                target->mapping = (struct yaml_mapping_entry *)
                    yaml_safe_realloc(target->mapping, cap * sizeof(struct yaml_mapping_entry));
                if (!target->mapping)
                    return;
            }
            target->mapping[tgt_current].key = AIRY_STRDUP(source->mapping[i].key);
            target->mapping[tgt_current].value = deep_copy_node(doc, source->mapping[i].value);
        }
    }
}

/* True when the current line starts with a block-sequence marker
 * (indentation followed by "- "), i.e. this mapping is the content of a
 * sequence item.  On such lines the first key sits two columns deeper
 * than the leading indentation reports. */
static bool current_line_has_dash(struct parse_ctx *ctx, int ind)
{
    size_t line_start = ctx->pos;
    while (line_start > 0 && ctx->src[line_start - 1] != '\n' &&
           ctx->src[line_start - 1] != '\r')
        line_start--;
    size_t content = line_start + (size_t)ind;
    return content + 1 < ctx->len && ctx->src[content] == '-' &&
           (ctx->src[content + 1] == ' ' || ctx->src[content + 1] == '\t');
}

struct yaml_node *parse_mapping(struct parse_ctx *ctx, int base_indent)
{
    struct yaml_node *map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
    if (!map)
        return NULL;

    size_t cap = 8;
    map->mapping = (struct yaml_mapping_entry *)AIRY_CALLOC(cap, sizeof(struct yaml_mapping_entry));
    size_t map_size = 0;
    int map_indent = -1;
    if (!map->mapping)
        return NULL;

    while (!at_end(ctx)) {
        skip_ws_nl_comments(ctx);
        if (at_end(ctx))
            break;

        int ind = line_indent(ctx);
        bool dash_line = map_indent < 0 && current_line_has_dash(ctx, ind);

        /* Sequence-item content shares the line with its "- " marker, so
         * the first key may sit at the same leading indentation; every
         * other line must be deeper than the mapping's base indent. */
        if (ind < base_indent || (ind == base_indent && !dash_line))
            break;

        /* All keys of one mapping share the indent of the first key;
         * a shallower line ends the mapping (e.g. the next sequence
         * item or a sibling key of an outer mapping). */
        if (map_indent < 0)
            map_indent = dash_line ? ind + 2 : ind;
        else if (ind < map_indent)
            break;

        char *key = parse_plain_scalar(ctx, ind);
        if (!key || key[0] == '\0') {
            AIRY_FREE(key);
            break;
        }

        skip_ws(ctx);
        if (peek(ctx) != ':') {
            set_error(ctx, "line %d: expected ':' after key '%s'", ctx->line, key);
            AIRY_FREE(key);
            break;
        }
        advance(ctx);

        skip_ws_and_nl(ctx);
        if (at_end(ctx) || peek(ctx) == '\n' || peek(ctx) == '\r') {
            int next_ind = count_indent(ctx);
            if (next_ind > ind) {
                struct yaml_node *val = parse_value(ctx, ind);
                if (val) {
                    if (strcmp(key, "<<") == 0) {
                        if (val->type == YAML_NODE_SEQUENCE) {
                            for (size_t si = 0; si < val->sequence.count; si++) {
                                merge_mapping_into(ctx->doc, map, val->sequence.items[si].item);
                            }
                        } else {
                            merge_mapping_into(ctx->doc, map, val);
                        }
                        AIRY_FREE(key);
                        map_size = yaml_size(map);
                        continue;
                    }
                    if (map_size >= cap) {
                        cap *= 2;
                        map->mapping = (struct yaml_mapping_entry *)yaml_safe_realloc(
                            map->mapping, cap * sizeof(struct yaml_mapping_entry));
                        if (!map->mapping)
                            return NULL;
                    }
                    map->mapping[map_size].key = key;
                    map->mapping[map_size].value = val;
                    map_size++;
                    continue;
                }
            }
            if (strcmp(key, "<<") == 0) {
                AIRY_FREE(key);
                continue;
            }
            struct yaml_node *null_node = alloc_node(ctx->doc, YAML_NODE_SCALAR);
            if (null_node)
                null_node->scalar.value = AIRY_STRDUP("");
            if (map_size >= cap) {
                cap *= 2;
                map->mapping = (struct yaml_mapping_entry *)
                    yaml_safe_realloc(map->mapping, cap * sizeof(struct yaml_mapping_entry));
                if (!map->mapping)
                    return NULL;
            }
            map->mapping[map_size].key = key;
            map->mapping[map_size].value = null_node;
            map_size++;
            continue;
        }

        struct yaml_node *val = parse_value(ctx, ind);
        if (!val)
            val = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (val && val->type == YAML_NODE_NONE)
            val->type = YAML_NODE_SCALAR;
        if (val && val->type == YAML_NODE_SCALAR && !val->scalar.value)
            val->scalar.value = AIRY_STRDUP("");

        if (strcmp(key, "<<") == 0) {
            if (val->type == YAML_NODE_SEQUENCE) {
                for (size_t si = 0; si < val->sequence.count; si++) {
                    merge_mapping_into(ctx->doc, map, val->sequence.items[si].item);
                }
            } else {
                merge_mapping_into(ctx->doc, map, val);
            }
            AIRY_FREE(key);
            /* val stays registered in doc->all_nodes; yaml_destroy() frees it
             * together with every other node (merge_mapping_into deep-copies
             * the merged content, so nothing references val afterwards). */
            map_size = yaml_size(map);
            continue;
        }

        if (map_size >= cap) {
            cap *= 2;
            map->mapping = (struct yaml_mapping_entry *)
                yaml_safe_realloc(map->mapping, cap * sizeof(struct yaml_mapping_entry));
            if (!map->mapping)
                return NULL;
        }
        map->mapping[map_size].key = key;
        map->mapping[map_size].value = val;
        map_size++;
    }

    return map;
}

struct yaml_node *parse_value(struct parse_ctx *ctx, int base_indent)
{
    skip_ws_nl_comments(ctx);
    if (at_end(ctx))
        return alloc_node(ctx->doc, YAML_NODE_NONE);

    char *tag = NULL;
    char *anchor_name = NULL;

    if (peek(ctx) == '!') {
        tag = parse_tag(ctx);
        skip_ws(ctx);
    }

    if (peek(ctx) == '&') {
        advance(ctx);
        anchor_name = parse_anchor_name(ctx);
        skip_ws(ctx);
    }

    if (peek(ctx) == '*')
        return parse_alias_value(ctx, tag, anchor_name);

    char c = peek(ctx);
    if (c == '"' || c == '\'')
        return parse_quoted_value(ctx, c, tag, anchor_name);
    if (c == '|' || c == '>')
        return parse_block_scalar_value(ctx, c == '>', tag, anchor_name);
    if (c == '-') {
        char ahead = '\0';
        if (ctx->pos + 1 < ctx->len)
            ahead = ctx->src[ctx->pos + 1];
        if (ahead == ' ' || ahead == '\t' || ahead == '\n' || ahead == '\r') {
            struct yaml_node *seq = parse_sequence(ctx, base_indent);
            if (seq && anchor_name)
                register_anchor(ctx, anchor_name, seq);
            if (seq && tag) {
                AIRY_FREE(seq->tag);
                seq->tag = tag;
                tag = NULL;
            }
            AIRY_FREE(anchor_name);
            AIRY_FREE(tag);
            return seq;
        }
    }
    if (c == '[')
        return parse_inline_sequence_value(ctx, base_indent, tag, anchor_name);
    if (c == '{')
        return parse_inline_mapping_value(ctx, base_indent, tag, anchor_name);

    size_t saved_pos = ctx->pos;
    char *scalar = parse_plain_scalar(ctx, base_indent);
    if (!scalar) {
        AIRY_FREE(tag);
        AIRY_FREE(anchor_name);
        return alloc_node(ctx->doc, YAML_NODE_NONE);
    }

    skip_ws(ctx);
    if (peek(ctx) == ':') {
        AIRY_FREE(scalar);
        scalar = NULL;
        ctx->pos = saved_pos;
        struct yaml_node *m = parse_mapping(ctx, base_indent);
        if (m && anchor_name)
            register_anchor(ctx, anchor_name, m);
        if (m && tag) {
            AIRY_FREE(m->tag);
            m->tag = tag;
            tag = NULL;
        }
        AIRY_FREE(anchor_name);
        AIRY_FREE(tag);
        return m;
    }

    struct yaml_node *n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
    if (n) {
        n->scalar.value = scalar;
        n->scalar.length = strlen(scalar);
        if (anchor_name) {
            register_anchor(ctx, anchor_name, n);
        }
        if (tag) {
            n->tag = tag;
            tag = NULL;
        }
    } else {
        AIRY_FREE(scalar);
        scalar = NULL;
    }
    AIRY_FREE(anchor_name);
    AIRY_FREE(tag);
    return n;
}

void skip_yaml_directives(struct parse_ctx *ctx)
{
    while (!at_end(ctx)) {
        if (peek(ctx) == '%') {
            while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r') {
                advance(ctx);
            }
            if (!at_end(ctx) && peek(ctx) == '\r')
                advance(ctx);
            if (!at_end(ctx) && peek(ctx) == '\n')
                advance(ctx);
        } else if (peek(ctx) == ' ' || peek(ctx) == '\t' || peek(ctx) == '\n' ||
                   peek(ctx) == '\r') {
            advance(ctx);
        } else {
            break;
        }
    }
}

bool is_document_marker(struct parse_ctx *ctx, const char *marker)
{
    size_t marker_len = strlen(marker);
    if (ctx->pos + marker_len > ctx->len)
        return false;
    if (memcmp(ctx->src + ctx->pos, marker, marker_len) != 0)
        return false;
    if (ctx->pos + marker_len < ctx->len) {
        char after = ctx->src[ctx->pos + marker_len];
        if (after != ' ' && after != '\t' && after != '\n' && after != '\r' && after != '\0')
            return false;
    }
    return true;
}

void skip_document_marker(struct parse_ctx *ctx, const char *marker)
{
    size_t marker_len = strlen(marker);
    ctx->pos += marker_len;
    while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r')
        advance(ctx);
    if (!at_end(ctx) && peek(ctx) == '\r')
        advance(ctx);
    if (!at_end(ctx) && peek(ctx) == '\n')
        advance(ctx);
}
