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

    target->mapping = yaml_mapping_grow(target->mapping, tgt_sz, cap);
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
                target->mapping = yaml_mapping_grow(target->mapping, tgt_current, cap);
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

/* 复合键（<<）合并机制件：序列值逐项并入目标映射，其余值整体并
 * 入，随后释放键串。val 已注册于 doc->all_nodes，其生命周期由
 * yaml_destroy() 统一回收（merge_mapping_into 深拷贝并入后不再
 * 引用 val）。 */
static void apply_merge_key(struct parse_ctx *ctx, struct yaml_node *map, char *key,
                            struct yaml_node *val)
{
    if (val->type == YAML_NODE_SEQUENCE) {
        for (size_t si = 0; si < val->sequence.count; si++)
            merge_mapping_into(ctx->doc, map, val->sequence.items[si].item);
    } else {
        merge_mapping_into(ctx->doc, map, val);
    }
    AIRY_FREE(key);
}

/* 映射装配器状态：键缩进在首个键确定，之后所有键共享该缩进 */
struct ymap_builder {
    struct yaml_node *map;
    size_t size;
    size_t cap;
    int indent;
};

static int ymap_init(struct parse_ctx *ctx, struct ymap_builder *b)
{
    b->map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
    b->size = 0;
    b->cap = 8;
    b->indent = -1;
    if (!b->map)
        return -1;
    b->map->mapping =
        (struct yaml_mapping_entry *)AIRY_CALLOC(b->cap, sizeof(struct yaml_mapping_entry));
    return b->map->mapping ? 0 : -1;
}

/* 键值入库：复合键（<<）走合并（合并后长度需从节点回读），
 * 其余按普通条目追加。失败返回 -1（键值所有权留在调用方）。 */
static int ymap_put(struct parse_ctx *ctx, struct ymap_builder *b, char *key,
                    struct yaml_node *val)
{
    if (strcmp(key, "<<") == 0) {
        apply_merge_key(ctx, b->map, key, val);
        b->size = yaml_size(b->map);
        return 0;
    }
    return yaml_mapping_append(&b->map->mapping, &b->size, &b->cap, key, val);
}

/* 冒号后为空：下一行更深则递归解析子结构，否则落空标量 */
static int ymap_block(struct parse_ctx *ctx, struct ymap_builder *b, int ind, char *key)
{
    int next_ind = count_indent(ctx);
    if (next_ind > ind) {
        struct yaml_node *val = parse_value(ctx, ind);
        if (val)
            return ymap_put(ctx, b, key, val);
    }
    /* 无子结构可并入：合并键直接丢弃（无值可并），其余落空串 */
    if (strcmp(key, "<<") == 0) {
        AIRY_FREE(key);
        return 0;
    }
    struct yaml_node *empty = alloc_node(ctx->doc, YAML_NODE_SCALAR);
    if (empty)
        empty->scalar.value = AIRY_STRDUP("");
    return ymap_put(ctx, b, key, empty);
}

/* 冒号后为同行值：解析失败降级为空标量，NONE 类型提升为 SCALAR */
static int ymap_inline(struct parse_ctx *ctx, struct ymap_builder *b, int ind, char *key)
{
    struct yaml_node *val = parse_value(ctx, ind);
    if (!val)
        val = alloc_node(ctx->doc, YAML_NODE_SCALAR);
    if (val && val->type == YAML_NODE_NONE)
        val->type = YAML_NODE_SCALAR;
    if (val && val->type == YAML_NODE_SCALAR && !val->scalar.value)
        val->scalar.value = AIRY_STRDUP("");
    return ymap_put(ctx, b, key, val);
}

/* 解析键并消费其后的 ':'；键缺失/为空/无冒号均结束当前映射 */
static char *ymap_key(struct parse_ctx *ctx, int ind)
{
    char *key = parse_plain_scalar(ctx, ind);
    if (!key || key[0] == '\0') {
        AIRY_FREE(key);
        return NULL;
    }
    skip_ws(ctx);
    if (peek(ctx) != ':') {
        set_error(ctx, "line %d: expected ':' after key '%s'", ctx->line, key);
        AIRY_FREE(key);
        return NULL;
    }
    advance(ctx);
    return key;
}

/* ':' 之后跳过空白与换行，判断是否走空值分支 */
static int ymap_no_inline(struct parse_ctx *ctx)
{
    skip_ws_and_nl(ctx);
    return at_end(ctx) || peek(ctx) == '\n' || peek(ctx) == '\r';
}

struct yaml_node *parse_mapping(struct parse_ctx *ctx, int base_indent)
{
    struct ymap_builder b;
    if (ymap_init(ctx, &b) != 0)
        return NULL;

    while (!at_end(ctx)) {
        skip_ws_nl_comments(ctx);
        if (at_end(ctx))
            break;

        int ind = line_indent(ctx);
        bool dash_line = b.indent < 0 && current_line_has_dash(ctx, ind);

        /* Sequence-item content shares the line with its "- " marker, so
         * the first key may sit at the same leading indentation; every
         * other line must be deeper than the mapping's base indent. */
        if (ind < base_indent || (ind == base_indent && !dash_line))
            break;

        /* All keys of one mapping share the indent of the first key;
         * a shallower line ends the mapping (e.g. the next sequence
         * item or a sibling key of an outer mapping). */
        if (b.indent < 0)
            b.indent = dash_line ? ind + 2 : ind;
        else if (ind < b.indent)
            break;

        char *key = ymap_key(ctx, ind);
        if (!key)
            break;
        int rc = ymap_no_inline(ctx) ? ymap_block(ctx, &b, ind, key)
                                     : ymap_inline(ctx, &b, ind, key);
        if (rc != 0)
            return NULL;
    }

    return b.map;
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
