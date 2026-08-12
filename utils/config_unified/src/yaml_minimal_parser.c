// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_parser.c
 * @brief YAML 1.1 解析器 - 语法树构建
 *
 * 本文件实现 YAML 语法层：节点分配/释放、锚点与别名、
 * 值/序列/映射/合并键解析、文档标记与指令跳过，单一职责。
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"

struct yaml_node *alloc_node(yaml_document_t *doc, yaml_node_type_t type)
{
    if (doc->node_count >= doc->node_capacity) {
        size_t new_cap = doc->node_capacity * 2;
        if (new_cap == 0)
            new_cap = INITIAL_NODE_CAPACITY;
        struct yaml_node **new_nodes =
            (struct yaml_node **)AIRY_REALLOC(doc->all_nodes, sizeof(struct yaml_node *) * new_cap);
        if (!new_nodes)
            return NULL;
        doc->all_nodes = new_nodes;
        doc->node_capacity = new_cap;
    }
    struct yaml_node *node = (struct yaml_node *)AIRY_CALLOC(1, sizeof(struct yaml_node));
    if (!node)
        return NULL;
    node->type = type;
    node->line = 0;
    doc->all_nodes[doc->node_count++] = node;
    return node;
}

void free_node(struct yaml_node *node)
{
    if (!node)
        return;
    switch (node->type) {
    case YAML_NODE_SCALAR:
        AIRY_FREE(node->scalar.value);
        node->scalar.value = NULL;
        break;
    case YAML_NODE_MAPPING:
        if (node->mapping) {
            for (size_t i = 0; i < yaml_size(node); i++) {
                AIRY_FREE(node->mapping[i].key);
                node->mapping[i].key = NULL;
            }
            AIRY_FREE(node->mapping);
            node->mapping = NULL;
        }
        break;
    case YAML_NODE_SEQUENCE:
        if (node->sequence.items) {
            AIRY_FREE(node->sequence.items);
            node->sequence.items = NULL;
        }
        break;
    default:
        break;
    }
    AIRY_FREE(node->anchor_name);
    node->anchor_name = NULL;
    AIRY_FREE(node->tag);
    node->tag = NULL;
}

void register_anchor(struct parse_ctx *ctx, const char *name, struct yaml_node *node)
{
    if (!name || !node)
        return;
    if (ctx->anchor_count >= ctx->anchor_capacity) {
        int new_cap = ctx->anchor_capacity * 2;
        if (new_cap == 0)
            new_cap = INITIAL_ANCHORS;
        struct anchor_entry *new_anchors =
            (struct anchor_entry *)AIRY_REALLOC(ctx->anchors,
                                                sizeof(struct anchor_entry) * new_cap);
        if (!new_anchors)
            return;
        ctx->anchors = new_anchors;
        ctx->anchor_capacity = new_cap;
    }
    if (node->anchor_name)
        AIRY_FREE(node->anchor_name);
    node->anchor_name = AIRY_STRDUP(name);
    ctx->anchors[ctx->anchor_count].name = AIRY_STRDUP(name);
    ctx->anchors[ctx->anchor_count].node = node;
    ctx->anchor_count++;
}

struct yaml_node *lookup_anchor(struct parse_ctx *ctx, const char *name)
{
    if (!name)
        return NULL;
    for (int i = 0; i < ctx->anchor_count; i++) {
        if (ctx->anchors[i].name && strcmp(ctx->anchors[i].name, name) == 0) {
            return ctx->anchors[i].node;
        }
    }
    return NULL;
}

struct yaml_node *deep_copy_node(yaml_document_t *doc, struct yaml_node *src)
{
    if (!src)
        return NULL;
    struct yaml_node *copy = alloc_node(doc, src->type);
    if (!copy)
        return NULL;
    copy->line = src->line;
    if (src->anchor_name)
        copy->anchor_name = AIRY_STRDUP(src->anchor_name);
    if (src->tag)
        copy->tag = AIRY_STRDUP(src->tag);
    switch (src->type) {
    case YAML_NODE_SCALAR:
        if (src->scalar.value) {
            copy->scalar.value = AIRY_STRDUP(src->scalar.value);
            copy->scalar.length = src->scalar.length;
        }
        break;
    case YAML_NODE_MAPPING: {
        size_t sz = yaml_size(src);
        copy->mapping =
            (struct yaml_mapping_entry *)AIRY_CALLOC(sz + 1, sizeof(struct yaml_mapping_entry));
        for (size_t i = 0; i < sz; i++) {
            copy->mapping[i].key = AIRY_STRDUP(src->mapping[i].key);
            copy->mapping[i].value = deep_copy_node(doc, src->mapping[i].value);
        }
        break;
    }
    case YAML_NODE_SEQUENCE: {
        copy->sequence.count = src->sequence.count;
        copy->sequence.items =
            (struct yaml_sequence_item *)AIRY_CALLOC(src->sequence.count,
                                                     sizeof(struct yaml_sequence_item));
        for (size_t i = 0; i < src->sequence.count; i++) {
            copy->sequence.items[i].item = deep_copy_node(doc, src->sequence.items[i].item);
        }
        break;
    }
    default:
        break;
    }
    return copy;
}

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
        skip_ws_and_nl(ctx);
        if (at_end(ctx))
            break;

        int ind = count_indent(ctx);
        if (ind <= base_indent)
            break;
        if (ind != base_indent + 2 && ind != base_indent + 4 && ind != base_indent + 1) {
            set_error(ctx, "line %d: inconsistent sequence indentation", ctx->line);
            break;
        }

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

struct yaml_node *parse_mapping(struct parse_ctx *ctx, int base_indent)
{
    struct yaml_node *map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
    if (!map)
        return NULL;

    size_t cap = 8;
    map->mapping = (struct yaml_mapping_entry *)AIRY_CALLOC(cap, sizeof(struct yaml_mapping_entry));
    size_t map_size = 0;
    if (!map->mapping)
        return NULL;

    while (!at_end(ctx)) {
        skip_ws_and_nl(ctx);
        if (at_end(ctx))
            break;

        int ind = count_indent(ctx);
        if (ind <= base_indent)
            break;

        if (map_size > 0 && ind < base_indent + 2)
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
            free_node(val);
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

    if (peek(ctx) == '*') {
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

    char c = peek(ctx);
    if (c == '"') {
        char *s = parse_quoted_string(ctx, '"');
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
    if (c == '\'') {
        char *s = parse_quoted_string(ctx, '\'');
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
    if (c == '|' || c == '>') {
        bool is_folded = (c == '>');
        advance(ctx);

        enum { CHOMP_CLIP, CHOMP_STRIP, CHOMP_KEEP } chomp = CHOMP_CLIP;
        int block_indent = -1;

        while (!at_end(ctx)) {
            char mod = peek(ctx);
            if (mod == '-') {
                chomp = CHOMP_STRIP;
                advance(ctx);
            } else if (mod == '+') {
                chomp = CHOMP_KEEP;
                advance(ctx);
            } else if (mod >= '1' && mod <= '9') {
                block_indent = mod - '0';
                advance(ctx);
            } else
                break;
        }

        skip_ws(ctx);
        if (!at_end(ctx) && peek(ctx) == '#') {
            while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r')
                advance(ctx);
        }
        if (!at_end(ctx) && peek(ctx) == '\r')
            advance(ctx);
        if (!at_end(ctx) && peek(ctx) == '\n')
            advance(ctx);

        size_t cap = 1024;
        size_t len = 0;
        char *buf = (char *)AIRY_MALLOC(cap);
        if (!buf)
            return NULL;

        int first_line_indent = -1;
        bool has_content = false;

        while (!at_end(ctx)) {
            int line_ind = 0;
            while (!at_end(ctx) && peek(ctx) == ' ') {
                line_ind++;
                advance(ctx);
            }

            if (at_end(ctx))
                break;

            char line_char = peek(ctx);
            if (line_char == '\n' || line_char == '\r') {
                if (peek(ctx) == '\r')
                    advance(ctx);
                if (peek(ctx) == '\n')
                    advance(ctx);
                if (chomp == CHOMP_KEEP) {
                    if (len + 2 >= cap) {
                        cap *= 2;
                        buf = (char *)yaml_safe_realloc(buf, cap);
                        if (!buf)
                            return NULL;
                    }
                    buf[len++] = '\n';
                } else {
                    if (len + 2 >= cap) {
                        cap *= 2;
                        buf = (char *)yaml_safe_realloc(buf, cap);
                        if (!buf)
                            return NULL;
                    }
                    buf[len++] = '\n';
                }
                continue;
            }

            if (line_char == ' ' || line_char == '\t') {
                continue;
            }

            if (block_indent < 0) {
                if (first_line_indent < 0) {
                    first_line_indent = line_ind;
                    block_indent = line_ind;
                }
            }

            if (line_ind < block_indent && line_char != '\0')
                break;

            if (has_content) {
                if (is_folded) {
                    if (len > 0 && buf[len - 1] == '\n') {
                        buf[len - 1] = '\n';
                    } else {
                        if (len + 2 >= cap) {
                            cap *= 2;
                            buf = (char *)yaml_safe_realloc(buf, cap);
                            if (!buf)
                                return NULL;
                        }
                        buf[len++] = ' ';
                    }
                } else {
                    if (len + 2 >= cap) {
                        cap *= 2;
                        buf = (char *)yaml_safe_realloc(buf, cap);
                        if (!buf)
                            return NULL;
                    }
                    buf[len++] = '\n';
                }
            }

            while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r') {
                if (len + 2 >= cap) {
                    cap *= 2;
                    buf = (char *)yaml_safe_realloc(buf, cap);
                    if (!buf)
                        return NULL;
                }
                buf[len++] = advance(ctx);
            }
            has_content = true;

            if (!at_end(ctx) && peek(ctx) == '\r')
                advance(ctx);
            if (!at_end(ctx) && peek(ctx) == '\n')
                advance(ctx);
        }

        switch (chomp) {
        case CHOMP_STRIP:
            while (len > 0 && buf[len - 1] == '\n')
                len--;
            break;
        case CHOMP_KEEP:
            break;
        case CHOMP_CLIP:
        default:
            if (len > 0 && buf[len - 1] != '\n') {
                if (len + 2 >= cap) {
                    cap *= 2;
                    buf = (char *)yaml_safe_realloc(buf, cap);
                    if (!buf)
                        return NULL;
                }
                buf[len++] = '\n';
            }
            break;
        }

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
    if (c == '[') {
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
    if (c == '{') {
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

void cleanup_parse_ctx(struct parse_ctx *ctx)
{
    if (ctx->anchors) {
        for (int i = 0; i < ctx->anchor_count; i++) {
            AIRY_FREE(ctx->anchors[i].name);
        }
        AIRY_FREE(ctx->anchors);
    }
    if (ctx->tag_handles) {
        for (int i = 0; i < ctx->tag_handle_count; i++) {
            AIRY_FREE(ctx->tag_handles[i]);
        }
        AIRY_FREE(ctx->tag_handles);
    }
    if (ctx->tag_prefixes) {
        for (int i = 0; i < ctx->tag_handle_count; i++) {
            AIRY_FREE(ctx->tag_prefixes[i]);
        }
        AIRY_FREE(ctx->tag_prefixes);
    }
}
