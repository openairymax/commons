// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_regex.c
 * @brief Backtracking POSIX ERE engine - public API and VM executor.
 *
 * Exposes the POSIX regcomp/regexec/regfree API and implements the
 * recursive backtracking executor. ERE parsing and AST compilation live
 * in airy_regex_parse.c / airy_regex_compile.c.
 *
 * Design:
 * - Two phases: parse the ERE text into an AST, then compile the AST into
 *   a Thompson-style instruction graph with explicit "after"
 *   continuations. Backtracking over SPLIT yields correct
 *   greedy/non-greedy semantics; SAVE instructions around groups record
 *   capture offsets.
 * - Real engine, no stubs: literals, '.', classes (ranges, negation),
 *   anchors ^ $, quantifiers * + ? {n,m}, alternation |, groups with up
 *   to 9 captures, escapes, and the GNU non-greedy *? +? ?? extension.
 * - Self-contained: depends only on airy_memory.h; POSIX builds use the
 *   system <regex.h> instead. The engine itself is unit-tested on Linux
 *   by compiling this file directly (no platform guards).
 *
 * BAN compliance: no sscanf/strcpy/memcpy/malloc; allocations go through
 * airy_malloc/airy_free/airy_realloc (MSVC-safe inline wrappers, no
 * __extension__).
 */

#include "airy_regex.h"

#include "airy_regex_internal.h"

#include "airy_memory.h"

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* VM: recursive backtracking executor                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *s;      /* whole string (for '^') */
    const char *start;  /* match start (for capture offsets) */
    int caps[2 * AIRY_RE_MAX_GROUPS];
    const airy_regex_t *re;
    int depth;
    int overflow;
} airy_re_vm_t;

static int airy_re_run(airy_re_vm_t *vm, airy_re_inst_t *pc, const char *sp)
{
    if (++vm->depth > AIRY_RE_MAX_DEPTH) {
        vm->overflow = 1;
        vm->depth--;
        return 0;
    }
    for (;;) {
        if (!pc) {
            vm->depth--;
            return 1; /* end of program: match */
        }
        switch (pc->op) {
        case AIRY_RE_I_CHAR:
            if (*sp && (unsigned char)*sp == (unsigned char)pc->c) {
                sp++;
                pc = pc->a;
            } else {
                vm->depth--;
                return 0;
            }
            break;
        case AIRY_RE_I_ANY:
            if (*sp) {
                sp++;
                pc = pc->a;
            } else {
                vm->depth--;
                return 0;
            }
            break;
        case AIRY_RE_I_CLASS: {
            const unsigned char *tbl = &vm->re->tables[(size_t)pc->c * 32];
            unsigned char cc = (unsigned char)*sp;
            if (cc && (tbl[cc >> 3] & (unsigned char)(1u << (cc & 7)))) {
                sp++;
                pc = pc->a;
            } else {
                vm->depth--;
                return 0;
            }
            break;
        }
        case AIRY_RE_I_ANCHOR:
            if (pc->c == '^') {
                if (sp == vm->s) {
                    pc = pc->a;
                } else {
                    vm->depth--;
                    return 0;
                }
            } else { /* '$' */
                if (*sp == '\0') {
                    pc = pc->a;
                } else {
                    vm->depth--;
                    return 0;
                }
            }
            break;
        case AIRY_RE_I_SAVE:
            vm->caps[pc->c] = (int)(sp - vm->start);
            pc = pc->a;
            break;
        case AIRY_RE_I_SPLIT: {
            int save_caps[2 * AIRY_RE_MAX_GROUPS];
            __builtin_memcpy(save_caps, vm->caps, sizeof(save_caps));
            const char *save_sp = sp;
            if (airy_re_run(vm, pc->a, sp))
                return 1;
            if (vm->overflow) {
                vm->depth--;
                return 0;
            }
            __builtin_memcpy(vm->caps, save_caps, sizeof(save_caps));
            sp = save_sp;
            if (airy_re_run(vm, pc->b, sp)) {
                vm->depth--;
                return 1;
            }
            vm->depth--;
            return 0;
        }
        case AIRY_RE_I_JMP:
            pc = pc->a;
            break;
        case AIRY_RE_I_MATCH:
            vm->depth--;
            return 1;
        default:
            vm->depth--;
            return 0;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

static void airy_re_free_all(airy_regex_t *re)
{
    for (airy_re_ast_t *it = re->ast_head; it;) {
        airy_re_ast_t *nx = it->own;
        airy_free(it);
        it = nx;
    }
    for (airy_re_inst_t *it = re->inst_head; it;) {
        airy_re_inst_t *nx = it->link;
        airy_free(it);
        it = nx;
    }
    airy_free(re->tables);
    airy_free(re->pat_owned);
    __builtin_memset(re, 0, sizeof(*re));
}

int airy_re_regcomp(airy_regex_t *preg, const char *pattern, int cflags)
{
    if (!preg || !pattern)
        return -1;
    __builtin_memset(preg, 0, sizeof(*preg));
    if (cflags & ~(REG_EXTENDED | REG_NOSUB))
        return -1;

    size_t plen = strlen(pattern);
    preg->pat_owned = (char *)airy_malloc(plen + 1);
    if (!preg->pat_owned)
        return -1;
    __builtin_memcpy(preg->pat_owned, pattern, plen + 1);

    airy_re_parser_t ps;
    __builtin_memset(&ps, 0, sizeof(ps));
    ps.p = pattern;
    ps.end = pattern + plen;
    ps.ngroups = 0;

    airy_re_ast_t *root = airy_re_parse_alt(&ps);
    if (!root || ps.error) {
        for (airy_re_ast_t *it = ps.head; it;) {
            airy_re_ast_t *nx = it->own;
            airy_free(it);
            it = nx;
        }
        airy_free(preg->pat_owned);
        __builtin_memset(preg, 0, sizeof(*preg));
        return -1;
    }
    preg->ast_head = ps.head;
    preg->nsub = ps.ngroups + 1;
    preg->re_flags = cflags;

    airy_re_comp_t co;
    __builtin_memset(&co, 0, sizeof(co));
    airy_re_inst_t *prog = airy_re_compile_seq(&co, root, NULL);
    if (!prog || co.error) {
        /* free everything */
        for (airy_re_ast_t *it = ps.head; it;) {
            airy_re_ast_t *nx = it->own;
            airy_free(it);
            it = nx;
        }
        for (airy_re_inst_t *it = co.all; it;) {
            airy_re_inst_t *nx = it->link;
            airy_free(it);
            it = nx;
        }
        airy_free(co.tables);
        airy_free(preg->pat_owned);
        __builtin_memset(preg, 0, sizeof(*preg));
        return -1;
    }
    /* The VM treats a NULL continuation as match-success, so the program
     * needs no explicit MATCH instruction. */
    preg->prog = prog;
    preg->inst_head = co.all;
    preg->tables = co.tables;
    preg->ntab = co.ntab;
    return 0;
}

int airy_re_regexec(const airy_regex_t *preg, const char *string, size_t nmatch,
                    airy_regmatch_t pmatch[], int eflags)
{
    (void)eflags;
    if (!preg || !preg->prog || !string)
        return -1;

    size_t slen = strlen(string);
    const char *end = string + slen;

    for (const char *start = string;; start++) {
        airy_re_vm_t vm;
        __builtin_memset(&vm, 0, sizeof(vm));
        vm.s = string;
        vm.start = start;
        vm.re = preg;
        if (airy_re_run(&vm, preg->prog, start)) {
            if (nmatch > 0 && pmatch) {
                size_t groups = nmatch < AIRY_RE_MAX_GROUPS ? nmatch : AIRY_RE_MAX_GROUPS;
                for (size_t g = 0; g < groups; g++) {
                    pmatch[g].rm_so = vm.caps[2 * (int)g];
                    pmatch[g].rm_eo = vm.caps[2 * (int)g + 1];
                }
            }
            return 0;
        }
        if (vm.overflow)
            return -1;
        if (start >= end)
            break;
    }
    return REG_NOMATCH;
}

void airy_re_regfree(airy_regex_t *preg)
{
    if (!preg)
        return;
    airy_re_free_all(preg);
}

#ifdef __cplusplus
}
#endif
