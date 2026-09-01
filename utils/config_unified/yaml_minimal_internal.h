/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file yaml_minimal_internal.h
 * @brief YAML 1.1 parser internal shared definitions.
 *
 * After yaml_minimal.c was split by functional domain, this header
 * carries the shared contract between the pieces:
 *   - yaml_minimal.c           document lifecycle and parsing entry
 *   - yaml_minimal_lexer.c     lexing
 *   - yaml_minimal_parser.c    syntax tree construction (block structures)
 *   - yaml_minimal_anchor.c    node lifecycle and anchor/alias registry
 *   - yaml_minimal_value.c     scalar/flow value parsing
 *   - yaml_minimal_node.c      node type access
 *   - yaml_minimal_scalar.c    scalar type conversion
 *   - yaml_minimal_serialize.c serialization output
 */

#ifndef AIRY_RT_YAML_MINIMAL_INTERNAL_H
#define AIRY_RT_YAML_MINIMAL_INTERNAL_H

#include "yaml_minimal.h"

#include "airy_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INITIAL_NODE_CAPACITY 64
#define MAX_LINE_LEN 8192
#define MAX_DEPTH 64
#define MAX_KEY_LEN 256
#define INITIAL_ANCHORS 64

struct anchor_entry {
    char *name;
    struct yaml_node *node;
};

struct parse_ctx {
    const char *src;
    size_t len;
    size_t pos;
    int line;
    int line_pos;
    int col;
    yaml_document_t *doc;
    struct anchor_entry *anchors;
    int anchor_count;
    int anchor_capacity;
    char *error_msg;
    char **tag_handles;
    char **tag_prefixes;
    int tag_handle_count;
};

void *yaml_safe_realloc(void *ptr, size_t size);
void set_error(struct parse_ctx *ctx, const char *fmt, ...);

/* Lexer */
char peek(struct parse_ctx *ctx);
char advance(struct parse_ctx *ctx);
bool at_end(struct parse_ctx *ctx);
void skip_ws(struct parse_ctx *ctx);
void skip_ws_and_nl(struct parse_ctx *ctx);
void skip_ws_nl_comments(struct parse_ctx *ctx);
int count_indent(struct parse_ctx *ctx);
int line_indent(struct parse_ctx *ctx);
bool is_plain_scalar_char(char c);
char *parse_quoted_string(struct parse_ctx *ctx, char quote);
char *parse_plain_scalar(struct parse_ctx *ctx, int end_indent);
char *parse_tag(struct parse_ctx *ctx);
char *parse_anchor_name(struct parse_ctx *ctx);

/* Syntax tree */
struct yaml_node *alloc_node(yaml_document_t *doc, yaml_node_type_t type);
void free_node(struct yaml_node *node);
void register_anchor(struct parse_ctx *ctx, const char *name, struct yaml_node *node);
struct yaml_node *lookup_anchor(struct parse_ctx *ctx, const char *name);
struct yaml_node *deep_copy_node(yaml_document_t *doc, struct yaml_node *src);
struct yaml_node *parse_value(struct parse_ctx *ctx, int base_indent);
struct yaml_node *parse_sequence(struct parse_ctx *ctx, int base_indent);
struct yaml_node *parse_mapping(struct parse_ctx *ctx, int base_indent);
void merge_mapping_into(yaml_document_t *doc, struct yaml_node *target,
                        struct yaml_node *source);
void skip_yaml_directives(struct parse_ctx *ctx);
bool is_document_marker(struct parse_ctx *ctx, const char *marker);
void skip_document_marker(struct parse_ctx *ctx, const char *marker);
void cleanup_parse_ctx(struct parse_ctx *ctx);

/* Scalar/flow value parsing (yaml_minimal_value.c) */
struct yaml_node *parse_alias_value(struct parse_ctx *ctx, char *tag, char *anchor_name);
struct yaml_node *parse_quoted_value(struct parse_ctx *ctx, int quote, char *tag,
                                     char *anchor_name);
struct yaml_node *parse_block_scalar_value(struct parse_ctx *ctx, bool is_folded,
                                           char *tag, char *anchor_name);
struct yaml_node *parse_inline_sequence_value(struct parse_ctx *ctx, int base_indent,
                                              char *tag, char *anchor_name);
struct yaml_node *parse_inline_mapping_value(struct parse_ctx *ctx, int base_indent,
                                             char *tag, char *anchor_name);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_YAML_MINIMAL_INTERNAL_H */
