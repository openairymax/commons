/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_regex_internal.h
 * @brief Backtracking POSIX ERE engine - internal shared definitions.
 *
 * After airy_regex.c was split by functional domain, this header carries
 * the AST/instruction layouts and the cross-file compile/parse entry
 * points:
 *   - airy_regex.c        public API + VM executor
 *   - airy_regex_parse.c  ERE text -> AST parser
 *   - airy_regex_compile.c AST -> instruction graph compiler
 */

#ifndef AIRY_RT_COMPAT_REGEX_INTERNAL_H
#define AIRY_RT_COMPAT_REGEX_INTERNAL_H

#include "airy_regex.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AIRY_RE_MAX_GROUPS 10  /* group 0 (whole match) + up to 9 captures */
#define AIRY_RE_MAX_DEPTH   4096
#define AIRY_RE_MAX_BODY    256 /* repeat expansion cap (prevents DoS blowup) */

/* --- AST node kinds (parser output) --- */
enum {
    AIRY_RE_A_EMPTY = 0,
    AIRY_RE_A_CHAR,   /* c */
    AIRY_RE_A_ANY,
    AIRY_RE_A_CLASS,  /* tbl[32] (negation folded into table) */
    AIRY_RE_A_ANCHOR, /* c = '^' or '$' */
    AIRY_RE_A_ALT,    /* a | b */
    AIRY_RE_A_REPEAT, /* child (.a), lo, hi, greedy */
    AIRY_RE_A_GROUP   /* child (.a), gid */
};

typedef struct airy_re_ast {
    int op;
    int c;
    int lo, hi;
    int greedy;
    int gid;
    unsigned char tbl[32];
    struct airy_re_ast *a;      /* first child */
    struct airy_re_ast *b;      /* second child */
    struct airy_re_ast *next;   /* next in concat list */
    struct airy_re_ast *own;    /* ownership list (all AST nodes) */
} airy_re_ast_t;

/* --- Compiled instruction kinds --- */
enum {
    AIRY_RE_I_CHAR = 1,
    AIRY_RE_I_ANY,
    AIRY_RE_I_CLASS,
    AIRY_RE_I_ANCHOR,
    AIRY_RE_I_SAVE,
    AIRY_RE_I_SPLIT,
    AIRY_RE_I_JMP,
    AIRY_RE_I_MATCH
};

typedef struct airy_re_inst airy_re_inst_t;
struct airy_re_inst {
    int op;
    int c;                 /* char / anchor / save slot / class index */
    airy_re_inst_t *a;     /* split branch A / jmp target / successor */
    airy_re_inst_t *b;     /* split branch B (unused for others) */
    airy_re_inst_t *link;  /* ownership list (all allocated instructions) */
};

/* --- Parser context (airy_regex_parse.c) --- */
typedef struct airy_re_parser {
    const char *p;
    const char *end;
    airy_re_ast_t *head; /* ownership list */
    int ngroups;
    int error;
} airy_re_parser_t;

/* --- Compiler context (airy_regex_compile.c) --- */
typedef struct airy_re_comp {
    airy_re_inst_t *all; /* ownership list head */
    unsigned char *tables;
    int ntab;
    int error;
} airy_re_comp_t;

/* Parser entry (airy_regex_parse.c) */
airy_re_ast_t *airy_re_parse_alt(airy_re_parser_t *ps);

/* Compiler entry (airy_regex_compile.c): compile a concat list; returns
 * first instruction or NULL. */
airy_re_inst_t *airy_re_compile_seq(airy_re_comp_t *co, airy_re_ast_t *list,
                                    airy_re_inst_t *after);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMPAT_REGEX_INTERNAL_H */
