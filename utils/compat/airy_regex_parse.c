// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_regex_parse.c
 * @brief Backtracking POSIX ERE engine - parser domain.
 *
 * Parses the ERE text into an AST (alternation / concatenation /
 * quantifiers / classes / groups / anchors / escapes), single
 * responsibility. Split out of airy_regex.c.
 */

#include "airy_regex.h"

#include "airy_regex_internal.h"

#include "airy_memory.h"

#include <string.h>

static airy_re_ast_t *airy_re_ast_new(airy_re_parser_t *ps, int op)
{
    airy_re_ast_t *n = (airy_re_ast_t *)airy_malloc(sizeof(airy_re_ast_t));
    if (!n) {
        ps->error = 1;
        return NULL;
    }
    __builtin_memset(n, 0, sizeof(*n));
    n->op = op;
    n->hi = -1;
    n->greedy = 1;
    n->own = ps->head;
    ps->head = n;
    return n;
}

static int airy_re_peek(const airy_re_parser_t *ps)
{
    return (ps->p < ps->end) ? (unsigned char)*ps->p : -1;
}

static int airy_re_next(airy_re_parser_t *ps)
{
    return (ps->p < ps->end) ? (unsigned char)*ps->p++ : -1;
}

/* Parse a class body after '[' (with optional leading '^'). */
static int airy_re_parse_class_body(airy_re_parser_t *ps, airy_re_ast_t *n, int neg)
{
    __builtin_memset(n->tbl, 0, sizeof(n->tbl));
    int closed = 0;
    for (;;) {
        if (ps->p >= ps->end) {
            ps->error = 1;
            return -1;
        }
        int ch = airy_re_next(ps);
        if (ch == ']') {
            closed = 1;
            break;
        }
        if (ch == '\\' && ps->p < ps->end) {
            ch = airy_re_next(ps);
            if (ch < 0) {
                ps->error = 1;
                return -1;
            }
            n->tbl[(unsigned char)ch >> 3] |= (unsigned char)(1u << ((unsigned char)ch & 7));
            continue;
        }
        if (ps->p + 1 < ps->end && ps->p[0] == '-' && ps->p[1] != ']') {
            int hi = ps->p[1];
            ps->p += 2;
            if (hi < ch) {
                ps->error = 1;
                return -1;
            }
            for (int k = ch; k <= hi; k++)
                n->tbl[(unsigned char)k >> 3] |= (unsigned char)(1u << ((unsigned char)k & 7));
            continue;
        }
        n->tbl[(unsigned char)ch >> 3] |= (unsigned char)(1u << ((unsigned char)ch & 7));
    }
    if (!closed) {
        ps->error = 1;
        return -1;
    }
    if (neg) {
        for (int i = 0; i < 32; i++)
            n->tbl[i] = (unsigned char)~n->tbl[i];
    }
    return 0;
}

/* Concatenation: builds a list; returns list head. */
static airy_re_ast_t *airy_re_parse_concat(airy_re_parser_t *ps)
{
    airy_re_ast_t *head = NULL;
    airy_re_ast_t *tail = NULL;
    for (;;) {
        int ch = airy_re_peek(ps);
        if (ch < 0 || ch == ')' || ch == '|')
            break;
        airy_re_ast_t *atom = NULL;

        if (ch == '(') {
            airy_re_next(ps);
            if (ps->ngroups >= AIRY_RE_MAX_GROUPS - 1) {
                ps->error = 1;
                return NULL;
            }
            int gid = ++ps->ngroups;
            airy_re_ast_t *inner = airy_re_parse_alt(ps);
            if (!inner)
                return NULL;
            if (airy_re_peek(ps) != ')') {
                ps->error = 1;
                return NULL;
            }
            airy_re_next(ps);
            atom = airy_re_ast_new(ps, AIRY_RE_A_GROUP);
            if (!atom)
                return NULL;
            atom->gid = gid;
            atom->a = inner;
        } else if (ch == '^' || ch == '$') {
            airy_re_next(ps);
            atom = airy_re_ast_new(ps, AIRY_RE_A_ANCHOR);
            if (!atom)
                return NULL;
            atom->c = ch;
        } else if (ch == '.') {
            airy_re_next(ps);
            atom = airy_re_ast_new(ps, AIRY_RE_A_ANY);
            if (!atom)
                return NULL;
        } else if (ch == '[') {
            airy_re_next(ps);
            int neg = 0;
            if (airy_re_peek(ps) == '^') {
                airy_re_next(ps);
                neg = 1;
            }
            atom = airy_re_ast_new(ps, AIRY_RE_A_CLASS);
            if (!atom)
                return NULL;
            if (airy_re_parse_class_body(ps, atom, neg) != 0)
                return NULL;
        } else if (ch == '\\') {
            airy_re_next(ps);
            int esc = airy_re_next(ps);
            if (esc < 0) {
                ps->error = 1;
                return NULL;
            }
            atom = airy_re_ast_new(ps, AIRY_RE_A_CHAR);
            if (!atom)
                return NULL;
            atom->c = esc;
        } else if (strchr("|+*?{})", ch) != NULL) {
            ps->error = 1; /* meta without operand */
            return NULL;
        } else {
            airy_re_next(ps);
            atom = airy_re_ast_new(ps, AIRY_RE_A_CHAR);
            if (!atom)
                return NULL;
            atom->c = ch;
        }

        /* Quantifier? */
        int q = airy_re_peek(ps);
        if (q == '*' || q == '+' || q == '?' || q == '{') {
            airy_re_next(ps);
            airy_re_ast_t *rep = airy_re_ast_new(ps, AIRY_RE_A_REPEAT);
            if (!rep)
                return NULL;
            rep->a = atom;
            if (q == '*') {
                rep->lo = 0;
                rep->hi = -1;
            } else if (q == '+') {
                rep->lo = 1;
                rep->hi = -1;
            } else if (q == '?') {
                rep->lo = 0;
                rep->hi = 1;
            } else { /* '{' */
                int n = 0;
                int digits = 0;
                while (ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9') {
                    n = n * 10 + (*ps->p - '0');
                    ps->p++;
                    digits = 1;
                }
                if (!digits) {
                    ps->error = 1;
                    return NULL;
                }
                if (ps->p < ps->end && *ps->p == ',') {
                    ps->p++;
                    int m = 0, d2 = 0;
                    while (ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9') {
                        m = m * 10 + (*ps->p - '0');
                        ps->p++;
                        d2 = 1;
                    }
                    rep->hi = d2 ? m : -1;
                } else {
                    rep->hi = n;
                }
                if (ps->p >= ps->end || *ps->p != '}') {
                    ps->error = 1;
                    return NULL;
                }
                ps->p++;
                rep->lo = n;
            }
            if (rep->lo > AIRY_RE_MAX_BODY || (rep->hi >= 0 && rep->hi > AIRY_RE_MAX_BODY)) {
                ps->error = 1;
                return NULL;
            }
            if (rep->hi >= 0 && rep->lo > rep->hi) {
                ps->error = 1; /* POSIX: {n,m} requires n <= m */
                return NULL;
            }
            /* GNU non-greedy extension */
            if (ps->p < ps->end && *ps->p == '?') {
                rep->greedy = 0;
                ps->p++;
            }
            atom = rep;
        }

        if (!head)
            head = atom;
        else
            tail->next = atom;
        tail = atom;
    }
    return head;
}

airy_re_ast_t *airy_re_parse_alt(airy_re_parser_t *ps)
{
    airy_re_ast_t *left = airy_re_parse_concat(ps);
    if (!left && ps->error)
        return NULL;
    while (airy_re_peek(ps) == '|') {
        airy_re_next(ps);
        airy_re_ast_t *right = airy_re_parse_concat(ps);
        if (!right)
            return NULL;
        airy_re_ast_t *alt = airy_re_ast_new(ps, AIRY_RE_A_ALT);
        if (!alt)
            return NULL;
        alt->a = left;
        alt->b = right;
        left = alt;
    }
    return left;
}
