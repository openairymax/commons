/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_effect.h
 * @brief Unified scope effect primitive (dsh ctx.effect() pattern):
 * register-now, undo-in-reverse-order.
 *
 * Modeled on deepseek-harness's `ctx.effect()` semantics:
 *   1. The side effect has ALREADY happened when the code reaches the
 *      registration point ("注册即副作用"): registering an effect never
 *      executes anything, it only records an undo action (disposer).
 *   2. On the failure/rollback path the registered disposers run in
 *      REVERSE registration order ("逆序回滚"), so that resources opened
 *      first are released last and cross-module undo dependencies hold.
 *   3. On the success/commit path nothing runs: the side effect is kept
 *      (no undo needed).
 *
 * Scope is single-threaded by design (one scope per flow/request, like
 * dsh's per-flow ctx); no internal locking is needed. A scope is
 * reusable: after rollback()/commit() it is empty and can accept new
 * registrations.
 *
 * Typical usage:
 * @code
 * airy_effect_t *fx = NULL;
 * if (airy_effect_create(&fx) != AIRY_EOK) return;
 * airy_effect_add(fx, my_undo, my_handle);      // side effect already done
 * if (!do_step()) { airy_effect_rollback(fx); airy_effect_destroy(fx); return; }
 * airy_effect_commit(fx);                        // keep side effects
 * airy_effect_destroy(fx);                       // empty scope, just frees
 * @endcode
 *
 * Thread safety: not thread-safe (single-threaded scope contract).
 * Pure C core, cross Linux/macOS/Windows (memory via airy_memory).
 */

#ifndef AIRY_RT_AIRY_EFFECT_H
#define AIRY_RT_AIRY_EFFECT_H

#include <stddef.h>

#include "airymax/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Undo disposer: releases/undoes a registered side effect.
 * @param ctx User context captured at registration (may be NULL)
 */
typedef void (*airy_effect_disposer_t)(void *ctx);

/**
 * @brief Effect scope (opaque; manage via airy_effect_* API only).
 */
typedef struct airy_effect airy_effect_t;

/**
 * @brief Create an effect scope (empty).
 * @param out_scope [out] scope handle (OWNER, airy_effect_destroy)
 * @return AIRY_EOK on success; AIRY_EINVAL invalid args; AIRY_ENOMEM
 *         allocation failure
 */
airy_err_t airy_effect_create(airy_effect_t **out_scope);

/**
 * @brief Destroy an effect scope.
 *
 * If the scope still holds registered disposers (neither rollback() nor
 * commit() was called), they run in reverse order first (safe default:
 * un-released side effects are cleaned up). Idempotent; NULL safe.
 *
 * @param scope Scope handle (may be NULL, TRANSFER)
 */
void airy_effect_destroy(airy_effect_t *scope);

/**
 * @brief Register an undo disposer ("注册即副作用": the side effect has
 * already happened; this call only records how to undo it).
 * @param scope Scope handle (non-NULL, BORROW)
 * @param disposer Undo disposer (non-NULL)
 * @param ctx Disposer context (may be NULL)
 * @return AIRY_EOK on success; AIRY_EINVAL invalid args; AIRY_ENOMEM
 *         allocation failure
 */
airy_err_t airy_effect_add(airy_effect_t *scope, airy_effect_disposer_t disposer, void *ctx);

/**
 * @brief Execute all registered disposers in REVERSE registration order
 * and empty the scope (failure/rollback path). Scope stays usable.
 * @param scope Scope handle (may be NULL, BORROW)
 */
void airy_effect_rollback(airy_effect_t *scope);

/**
 * @brief Commit: empty the scope WITHOUT executing disposers (success
 * path; the side effects are kept, no undo needed). Scope stays usable.
 * @param scope Scope handle (may be NULL, BORROW)
 */
void airy_effect_commit(airy_effect_t *scope);

/**
 * @brief Manually dispose a single registered effect identified by its
 * context pointer (early explicit cleanup). The disposer runs once and
 * the entry is removed; later rollback()/commit()/destroy() no longer
 * touch it. On multiple registrations with the same ctx, the most
 * recently registered one (closest to the end of the reverse order) is
 * disposed.
 * @param scope Scope handle (may be NULL, BORROW)
 * @param ctx Context captured at registration
 */
void airy_effect_dispose(airy_effect_t *scope, void *ctx);

/**
 * @brief Number of registered disposers.
 * @param scope Scope handle (may be NULL)
 * @return Count of registered disposers (0 for NULL scope)
 */
size_t airy_effect_count(const airy_effect_t *scope);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_AIRY_EFFECT_H */
