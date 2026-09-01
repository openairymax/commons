// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_anchor.c
 * @brief YAML 1.1 parser - node lifecycle and anchor/alias registry.
 *
 * Implements the YAML node lifecycle (alloc/free, deep copy) and the
 * anchor/alias registry (register/lookup), plus parse-context cleanup,
 * single responsibility. Split out of yaml_minimal_parser.c.
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
            /* Snapshot the entry count: clearing keys below makes
             * yaml_size() shrink, which would otherwise stop the loop
             * after the first entry and leak the remaining keys. */
            size_t entry_count = yaml_size(node);
            for (size_t i = 0; i < entry_count; i++) {
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
    AIRY_FREE(node);
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
