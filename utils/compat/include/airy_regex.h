/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef AIRY_RT_COMPAT_REGEX_H
#define AIRY_RT_COMPAT_REGEX_H

/*
 * Cross-platform regular expression support.
 *
 * - On Windows (no <regex.h> in MSVC): the airy_re_* POSIX-ERE engine
 *   (airy_regex.c) is used, and the POSIX names regcomp/regexec/regfree
 *   plus regex_t/regmatch_t are mapped onto it so tool_d's builtin_fs /
 *   builtin_net source stays identical on both platforms.
 * - On POSIX: the system <regex.h> is used; airy_re_* still exists (the
 *   engine is compiled into commons and unit-tested on Linux) but POSIX
 *   callers never reference it.
 *
 * Engine scope: literals, '.', character classes (ranges, negation),
 * anchors ^ $, quantifiers * + ? {n,m} (incl. non-greedy *? +? ??),
 * alternation |, groups with up to 9 captures, and escapes.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compiled regular expression (engine-owned state). */
typedef struct airy_regex {
    struct airy_re_ast *ast_head;  /* all AST nodes (ownership list) */
    struct airy_re_inst *inst_head; /* all instructions (ownership list) */
    struct airy_re_inst *prog;     /* program entry point */
    unsigned char *tables;         /* character class tables */
    int ntab;
    int nsub;                      /* capture count (max group index + 1) */
    int re_flags;
    char *pat_owned;               /* pattern copy */
} airy_regex_t;

typedef struct airy_regmatch {
    int rm_so;
    int rm_eo;
} airy_regmatch_t;

int airy_re_regcomp(airy_regex_t *preg, const char *pattern, int cflags);
int airy_re_regexec(const airy_regex_t *preg, const char *string, size_t nmatch,
                    airy_regmatch_t pmatch[], int eflags);
void airy_re_regfree(airy_regex_t *preg);

#ifdef _WIN32

#define REG_EXTENDED 1u
#define REG_NOSUB    2u
#define REG_NOMATCH  1

/* POSIX name mapping used by cross-platform tool sources. */
#define regex_t airy_regex_t
#define regmatch_t airy_regmatch_t
#define regcomp airy_re_regcomp
#define regexec airy_re_regexec
#define regfree airy_re_regfree

#else /* POSIX: use the system <regex.h>. */

#include <regex.h>

#endif /* _WIN32 */

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMPAT_REGEX_H */
