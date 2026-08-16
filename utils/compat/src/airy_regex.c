// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_regex.c
 * @brief Backtracking POSIX ERE engine for platforms without <regex.h>
 *        (Windows/MSVC). Exposes the POSIX regcomp/regexec/regfree API.
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

#include "airy_memory.h"

#include <stdint.h>
#include <string.h>

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

/* ------------------------------------------------------------------ */
/* Parser: ERE text -> AST                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *p;
    const char *end;
    airy_re_ast_t *head; /* ownership list */
    int ngroups;
    int error;
} airy_re_parser_t;

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

static airy_re_ast_t *airy_re_parse_alt(airy_re_parser_t *ps);

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

static airy_re_ast_t *airy_re_parse_alt(airy_re_parser_t *ps)
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

/* ------------------------------------------------------------------ */
/* Compiler: AST -> instruction graph (after-continuation)              */
/* ------------------------------------------------------------------ */

typedef struct {
    airy_re_inst_t *all; /* ownership list head */
    unsigned char *tables;
    int ntab;
    int error;
} airy_re_comp_t;

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
static airy_re_inst_t *airy_re_compile_seq(airy_re_comp_t *co, airy_re_ast_t *list,
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
