/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_effect.c
 * @brief Unified scope effect primitive implementation (dsh ctx.effect()).
 *
 * Backing structure: a growable array of (disposer, ctx) pairs. Disposers
 * always run in REVERSE registration order (stack semantics) so that the
 * last-registered effect is undone first. The scope is single-threaded by
 * contract and never executes anything at registration time.
 */

#include "airy_effect.h"
#include "airy_memory.h"

#include <stdlib.h>

#define AIRY_EFFECT_INIT_CAPACITY 4

typedef struct airy_effect_entry {
    airy_effect_disposer_t disposer;
    void *ctx;
} airy_effect_entry_t;

struct airy_effect {
    airy_effect_entry_t *entries;
    size_t count;
    size_t capacity;
};

airy_err_t airy_effect_create(airy_effect_t **out_scope)
{
    if (!out_scope)
        return AIRY_EINVAL;
    *out_scope = NULL;

    airy_effect_t *scope = (airy_effect_t *)AIRY_CALLOC(1, sizeof(airy_effect_t));
    if (!scope)
        return AIRY_ENOMEM;

    scope->entries = (airy_effect_entry_t *)AIRY_CALLOC(AIRY_EFFECT_INIT_CAPACITY,
                                                        sizeof(airy_effect_entry_t));
    if (!scope->entries) {
        AIRY_FREE(scope);
        return AIRY_ENOMEM;
    }
    scope->capacity = AIRY_EFFECT_INIT_CAPACITY;
    scope->count = 0;

    *out_scope = scope;
    return AIRY_EOK;
}

void airy_effect_destroy(airy_effect_t *scope)
{
    if (!scope)
        return;
    /* Safe default: un-rolled-back side effects are cleaned up in reverse
     * order before the scope itself is freed. */
    airy_effect_rollback(scope);
    if (scope->entries)
        AIRY_FREE(scope->entries);
    AIRY_FREE(scope);
}

airy_err_t airy_effect_add(airy_effect_t *scope, airy_effect_disposer_t disposer, void *ctx)
{
    if (!scope || !disposer)
        return AIRY_EINVAL;

    if (scope->count == scope->capacity) {
        size_t new_cap = scope->capacity * 2;
        airy_effect_entry_t *grown =
            (airy_effect_entry_t *)AIRY_REALLOC(scope->entries, new_cap * sizeof(airy_effect_entry_t));
        if (!grown)
            return AIRY_ENOMEM;
        scope->entries = grown;
        scope->capacity = new_cap;
    }

    scope->entries[scope->count].disposer = disposer;
    scope->entries[scope->count].ctx = ctx;
    scope->count++;
    return AIRY_EOK;
}

void airy_effect_rollback(airy_effect_t *scope)
{
    if (!scope)
        return;
    while (scope->count > 0) {
        scope->count--;
        if (scope->entries[scope->count].disposer)
            scope->entries[scope->count].disposer(scope->entries[scope->count].ctx);
    }
}

void airy_effect_commit(airy_effect_t *scope)
{
    if (!scope)
        return;
    scope->count = 0;
}

void airy_effect_dispose(airy_effect_t *scope, void *ctx)
{
    if (!scope || scope->count == 0)
        return;
    /* Scan from the most recently registered entry backward (closest to
     * the head of the reverse execution order). */
    for (size_t i = scope->count; i > 0; i--) {
        airy_effect_entry_t *e = &scope->entries[i - 1];
        if (e->ctx == ctx) {
            if (e->disposer)
                e->disposer(e->ctx);
            /* Shift the tail down to keep the array compact and preserve
             * relative order of the remaining entries. */
            for (size_t j = i - 1; j + 1 < scope->count; j++)
                scope->entries[j] = scope->entries[j + 1];
            scope->count--;
            return;
        }
    }
}

size_t airy_effect_count(const airy_effect_t *scope)
{
    return scope ? scope->count : 0;
}
