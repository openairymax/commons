// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal.c
 * @brief YAML 1.1 parser - document lifecycle and parsing entry.
 *
 * Keeps the document object lifecycle and parsing entry: document
 * create/destroy, string/file/multi-document parsing, error and root
 * node access.
 *
 * Lexing, syntax tree building, node access, scalar conversion and
 * serialization are split into yaml_minimal_lexer.c / yaml_minimal_parser.c /
 * yaml_minimal_node.c / yaml_minimal_scalar.c /
 * yaml_minimal_serialize.c.
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

yaml_document_t *yaml_create(void)
{
    yaml_document_t *doc = (yaml_document_t *)AIRY_CALLOC(1, sizeof(yaml_document_t));
    return doc;
}

void yaml_destroy(yaml_document_t *doc)
{
    if (!doc)
        return;
    for (size_t i = 0; i < doc->node_count; i++) {
        free_node(doc->all_nodes[i]);
    }
    AIRY_FREE(doc->all_nodes);
    AIRY_FREE(doc->source);
    AIRY_FREE(doc->error_msg);
    AIRY_FREE(doc);
}

int yaml_parse_string(yaml_document_t *doc, const char *input, size_t len)
{
    if (!doc || !input)
        return AIRY_EINVAL;
    __builtin_memset(doc, 0, sizeof(*doc));

    size_t start_pos = 0;
    if (len >= 3 && (unsigned char)input[0] == 0xEF && (unsigned char)input[1] == 0xBB &&
        (unsigned char)input[2] == 0xBF) {
        start_pos = 3;
    }

    size_t effective_len = (len > 0) ? len : strlen(input);
    if (effective_len == 0) {
        doc->root = alloc_node(doc, YAML_NODE_SCALAR);
        if (doc->root)
            doc->root->scalar.value = AIRY_STRDUP("");
        return 0;
    }

    doc->source = (char *)AIRY_MALLOC(effective_len + 1);
    if (!doc->source)
        return AIRY_EINVAL;
    __builtin_memcpy(doc->source, input, effective_len);
    doc->source[effective_len] = '\0';
    doc->source_len = effective_len;

    struct parse_ctx ctx;
    __builtin_memset(&ctx, 0, sizeof(ctx));
    ctx.src = doc->source;
    ctx.len = effective_len;
    ctx.pos = start_pos;
    ctx.doc = doc;
    ctx.line = 1;
    ctx.anchor_count = 0;
    ctx.anchor_capacity = 0;
    ctx.anchors = NULL;
    ctx.tag_handles = NULL;
    ctx.tag_prefixes = NULL;
    ctx.tag_handle_count = 0;

    skip_yaml_directives(&ctx);

    if (is_document_marker(&ctx, "---")) {
        skip_document_marker(&ctx, "---");
    }

    doc->root = parse_value(&ctx, -1);

    cleanup_parse_ctx(&ctx);
    return doc->root ? 0 : -1;
}

int yaml_parse_multi(yaml_document_t *doc, const char *input, size_t len)
{
    if (!doc || !input)
        return AIRY_EINVAL;

    size_t start_pos = 0;
    if (len >= 3 && (unsigned char)input[0] == 0xEF && (unsigned char)input[1] == 0xBB &&
        (unsigned char)input[2] == 0xBF) {
        start_pos = 3;
    }

    size_t effective_len = (len > 0) ? len : strlen(input);

    char *source_copy = (char *)AIRY_MALLOC(effective_len + 1);
    if (!source_copy)
        return AIRY_EINVAL;
    __builtin_memcpy(source_copy, input, effective_len);
    source_copy[effective_len] = '\0';

    struct parse_ctx ctx;
    __builtin_memset(&ctx, 0, sizeof(ctx));
    ctx.src = source_copy;
    ctx.len = effective_len;
    ctx.pos = start_pos;
    ctx.doc = doc;
    ctx.line = 1;
    ctx.anchor_count = 0;
    ctx.anchor_capacity = 0;
    ctx.anchors = NULL;

    skip_yaml_directives(&ctx);
    if (is_document_marker(&ctx, "---")) {
        skip_document_marker(&ctx, "---");
    }

    doc->source = source_copy;
    doc->source_len = effective_len;
    doc->document_index = 0;
    doc->root = parse_value(&ctx, -1);

    yaml_document_t *current = doc;

    while (!at_end(&ctx)) {
        skip_ws_and_nl(&ctx);

        if (is_document_marker(&ctx, "...")) {
            skip_document_marker(&ctx, "...");
            skip_ws_and_nl(&ctx);
        }

        if (at_end(&ctx))
            break;

        skip_yaml_directives(&ctx);

        if (is_document_marker(&ctx, "---")) {
            skip_document_marker(&ctx, "---");
        } else {
            break;
        }

        yaml_document_t *next_doc = yaml_create();
        if (!next_doc)
            break;

        next_doc->source = source_copy;
        next_doc->source_len = effective_len;
        next_doc->document_index = current->document_index + 1;

        ctx.doc = next_doc;
        ctx.anchor_count = 0;

        next_doc->root = parse_value(&ctx, -1);
        current->next = next_doc;
        current = next_doc;
    }

    cleanup_parse_ctx(&ctx);
    return doc->root ? 0 : -1;
}

void yaml_destroy_chain(yaml_document_t *doc)
{
    if (!doc)
        return;
    yaml_document_t *current = doc;
    while (current) {
        yaml_document_t *next = current->next;
        current->next = NULL;
        current->source = NULL;
        yaml_destroy(current);
        current = next;
    }
}

int yaml_parse_file(yaml_document_t *doc, const char *filepath)
{
    if (!doc || !filepath)
        return AIRY_EINVAL;
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        doc->error_msg = (char *)AIRY_MALLOC(256);
        snprintf(doc->error_msg, 256, "Cannot open file: %s", filepath);
        return AIRY_EINVAL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 10 * 1024 * 1024) {
        fclose(f);
        doc->error_msg = AIRY_STRDUP("File too large");
        return AIRY_EINVAL;
    }
    if (sz == 0) {
        fclose(f);
        doc->root = alloc_node(doc, YAML_NODE_SCALAR);
        if (doc->root)
            doc->root->scalar.value = AIRY_STRDUP("");
        return 0;
    }
    char *buf = (char *)AIRY_MALLOC(sz + 1);
    if (!buf) {
        fclose(f);
        return AIRY_EINVAL;
    }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    if (rd != (size_t)sz) {
        AIRY_FREE(buf);
        return AIRY_EINVAL;
    }
    int ret = yaml_parse_string(doc, buf, rd);
    AIRY_FREE(buf);
    return ret;
}

const char *yaml_get_error(const yaml_document_t *doc)
{
    return doc ? doc->error_msg : NULL;
}

struct yaml_node *yaml_root(const yaml_document_t *doc)
{
    return doc ? doc->root : NULL;
}
