// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_regex_compile.c
 * @brief Backtracking POSIX ERE engine - compiler domain.
 *
 * Compiles the AST into a Thompson-style instruction graph with explicit
 * "after" continuations (repeat expansion, alternation SPLITs, group
 * SAVE markers), single responsibility. Split out of airy_regex.c.
 */

#include "airy_regex.h"

#include "airy_regex_internal.h"

#include "airy_memory.h"

static airy_re_inst_t *airy_re_inst_new(airy_re_comp_t *co, int op)
{
    airy_re_inst_t *ip = (airy_re_inst_t *)airy_malloc(sizeof(airy_re_inst_t));
    if (!ip) {
        co->error = 1;
        return NULL;
    }
    __builtin_memset(ip, 0, sizeof(*ip));
    ip->op = op;
    ip->link = co->all;
    co->all = ip;
    return ip;
}

/* Clone a linear (branch-free) fragment chain for repeat expansion.
 * body is verified linear by the caller, so a simple a-chain copy is
 * correct and no node map is needed. */
static airy_re_inst_t *airy_re_clone_linear(airy_re_comp_t *co, airy_re_inst_t *body)
{
    airy_re_inst_t *head = NULL;
    airy_re_inst_t *tail = NULL;
    for (airy_re_inst_t *it = body; it; it = it->a) {
        airy_re_inst_t *cp = airy_re_inst_new(co, it->op);
        if (!cp)
            return NULL;
        cp->c = it->c;
        if (tail)
            tail->a = cp;
        else
            head = cp;
        tail = cp;
    }
    return head;
}

/* Chain the tail (successor-less node along the 'a' chain) of fragment
 * 'f' to 'next'. Fragments produced by compile_one have a single entry
 * and their reachable exits end at nodes whose 'a' is NULL (the after
 * continuation slot). */
static void airy_re_chain_tail(airy_re_comp_t *co, airy_re_inst_t *f, airy_re_inst_t *next)
{
    (void)co;
    /* DFS to find all nodes with a == NULL; since our programs are
     * linear chains with SPLIT branches, collect all sinks. For
     * simplicity and correctness we walk the a-chain only when the
     * fragment has no SPLIT/JMP; compiler ensures fragments used in
     * repeat expansion are branch-free except group SAVE wrappers. */
    airy_re_inst_t *t = f;
    while (t && t->a)
        t = t->a;
    if (t)
        t->a = next;
}

static airy_re_inst_t *airy_re_compile_one(airy_re_comp_t *co, airy_re_ast_t *n,
                                           airy_re_inst_t *after);

/* Compile a concat list; returns first instruction or NULL. */
airy_re_inst_t *airy_re_compile_seq(airy_re_comp_t *co, airy_re_ast_t *list,
                                    airy_re_inst_t *after)
{
    airy_re_ast_t *nodes[256];
    int count = 0;
    for (airy_re_ast_t *it = list; it && count < 256; it = it->next)
        nodes[count++] = it;
    airy_re_inst_t *cont = after;
    for (int i = count - 1; i >= 0; i--) {
        cont = airy_re_compile_one(co, nodes[i], cont);
        if (!cont)
            return NULL;
    }
    return cont;
}

/* Compile a repeat: n mandatory copies then (hi-n) optional copies or an
 * unbounded SPLIT loop. body must be branch-free (no SPLIT/JMP) so that
 * cloning preserves semantics. */
static airy_re_inst_t *airy_re_compile_repeat(airy_re_comp_t *co, airy_re_ast_t *n,
                                              airy_re_inst_t *after)
{
    airy_re_inst_t *body = airy_re_compile_one(co, n->a, NULL);
    if (!body)
        return NULL;

    int mandatory = n->lo;
    int unbounded = (n->hi < 0);
    int optional = unbounded ? 1 : (n->hi - n->lo);

    /* Pre-verify the body is linear (branch-free): walk the a-chain and
     * ensure every node has b == NULL. SPLIT/JMP inside the body would
     * make cloning incorrect; reject such patterns rather than emit a
     * wrong engine. */
    for (airy_re_inst_t *it = body; it; it = it->a) {
        if (it->b != NULL) {
            co->error = 1;
            return NULL;
        }
    }

    /* Build from the tail: optional part first, then mandatory copies. */
    airy_re_inst_t *seq = after;

    /* Unbounded tail: SPLIT(loop-back, exit) with the body copy between. */
    if (unbounded) {
        airy_re_inst_t *loop = airy_re_inst_new(co, AIRY_RE_I_SPLIT);
        if (!loop)
            return NULL;
        airy_re_inst_t *cp = airy_re_clone_linear(co, body);
        if (!cp)
            return NULL;
        if (n->greedy) {
            loop->a = cp;
            loop->b = seq;
        } else {
            loop->a = seq;
            loop->b = cp;
        }
        airy_re_chain_tail(co, cp, loop);
        seq = loop;
    } else {
        for (int i = 0; i < optional; i++) {
            airy_re_inst_t *sp = airy_re_inst_new(co, AIRY_RE_I_SPLIT);
            if (!sp)
                return NULL;
            airy_re_inst_t *cp = airy_re_clone_linear(co, body);
            if (!cp)
                return NULL;
            if (n->greedy) {
                sp->a = cp;
                sp->b = seq;
            } else {
                sp->a = seq;
                sp->b = cp;
            }
            airy_re_chain_tail(co, cp, seq);
            seq = sp;
        }
    }

    for (int i = 0; i < mandatory; i++) {
        airy_re_inst_t *cp = airy_re_clone_linear(co, body);
        if (!cp)
            return NULL;
        airy_re_chain_tail(co, cp, seq);
        seq = cp;
    }
    return seq;
}

/* Compile one AST node with its continuation 'after'. Returns the first
 * instruction of the compiled fragment, or NULL on error. */
static airy_re_inst_t *airy_re_compile_one(airy_re_comp_t *co, airy_re_ast_t *n,
                                           airy_re_inst_t *after)
{
    switch (n->op) {
    case AIRY_RE_A_EMPTY:
        return after;
    case AIRY_RE_A_CHAR:
    case AIRY_RE_A_ANCHOR: {
        airy_re_inst_t *ip = airy_re_inst_new(co,
                                              n->op == AIRY_RE_A_CHAR ? AIRY_RE_I_CHAR :
                                                                        AIRY_RE_I_ANCHOR);
        if (!ip)
            return NULL;
        ip->c = n->c;
        ip->a = after;
        return ip;
    }
    case AIRY_RE_A_ANY: {
        airy_re_inst_t *ip = airy_re_inst_new(co, AIRY_RE_I_ANY);
        if (!ip)
            return NULL;
        ip->a = after;
        return ip;
    }
    case AIRY_RE_A_CLASS: {
        unsigned char *nt = (unsigned char *)airy_realloc(co->tables, (size_t)(co->ntab + 1) * 32);
        if (!nt) {
            co->error = 1;
            return NULL;
        }
        co->tables = nt;
        __builtin_memcpy(&co->tables[co->ntab * 32], n->tbl, 32);
        int idx = co->ntab++;
        airy_re_inst_t *ip = airy_re_inst_new(co, AIRY_RE_I_CLASS);
        if (!ip)
            return NULL;
        ip->c = idx;
        ip->a = after;
        return ip;
    }
    case AIRY_RE_A_GROUP: {
        airy_re_inst_t *save0 = airy_re_inst_new(co, AIRY_RE_I_SAVE);
        if (!save0)
            return NULL;
        save0->c = n->gid * 2;
        airy_re_inst_t *save1 = airy_re_inst_new(co, AIRY_RE_I_SAVE);
        if (!save1)
            return NULL;
        save1->c = n->gid * 2 + 1;
        save1->a = after;
        /* The group body is a concat list, not a single node. */
        airy_re_inst_t *body = airy_re_compile_seq(co, n->a, save1);
        if (!body)
            return NULL;
        save0->a = body;
        return save0;
    }
    case AIRY_RE_A_ALT: {
        /* Each alternative is a concat list. */
        airy_re_inst_t *b1 = airy_re_compile_seq(co, n->a, after);
        if (!b1)
            return NULL;
        airy_re_inst_t *b2 = airy_re_compile_seq(co, n->b, after);
        if (!b2)
            return NULL;
        airy_re_inst_t *sp = airy_re_inst_new(co, AIRY_RE_I_SPLIT);
        if (!sp)
            return NULL;
        sp->a = b1;
        sp->b = b2;
        return sp;
    }
    case AIRY_RE_A_REPEAT:
        return airy_re_compile_repeat(co, n, after);
    default:
        co->error = 1;
        return NULL;
    }
}
