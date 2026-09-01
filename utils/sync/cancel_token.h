/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cancel_token.h
 * @brief Async interruptible execution model: cancel token (improvement 1:
 * Codex parallel.rs cancel_token pattern).
 *
 * An atomic cancelled flag plus a callback wake-up chain, spanning
 * taskflow -> work_hall -> sched_d executor -> agent_d/tool_d. Any entity
 * holding the token may request cancellation; waiters poll via
 * airy_cancel_token_is_canceled() or block via airy_cancel_token_wait()
 * (condition-variable wake-up, not busy polling); registered wake-up
 * callbacks fire at the moment of cancellation (used to wake poll/select
 * style I/O waits that cannot block directly on the token).
 *
 * Thread safety: all public interfaces are thread-safe.
 * Pure C core, cross Linux/macOS/Windows (depends on the platform layer's
 * airy_mtx/airy_cond/airy_atomic).
 */

#ifndef AIRY_RT_CANCEL_TOKEN_H
#define AIRY_RT_CANCEL_TOKEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"
#ifdef __cplusplus
extern "C" {
#endif


#define AIRY_CANCEL_TOKEN_MAX_CBS 8

/**
 * @brief Cancel callback: fires when the token is cancelled (used to wake
 * blocked I/O waiters).
 * @param ctx User context passed through at registration
 */
typedef void (*airy_cancel_cb_t)(void *ctx);

/**
 * @brief Cancel token structure (opaque usage: callers hold an instance;
 * do not access internal fields directly).
 */
typedef struct airy_cancel_token {
    airy_mtx_t lock;
    airy_cond_t cond;
    airy_atomic_int_t cancelled;
    int init_done;
    airy_cancel_cb_t cbs[AIRY_CANCEL_TOKEN_MAX_CBS];
    void *cb_ctxs[AIRY_CANCEL_TOKEN_MAX_CBS];
    size_t cb_count;
} airy_cancel_token_t;

/**
 * @brief Initialize a cancel token (active state).
 * @param token Token pointer (non-NULL)
 * @return 0 on success; AIRY_EINVAL invalid args; AIRY_ERR_SYS_MUTEX/
 * CONDITION underlying creation failed
 */
int airy_cancel_token_init(airy_cancel_token_t *token);

/**
 * @brief Destroy a cancel token (idempotent).
 * @param token Token pointer (may be NULL)
 */
void airy_cancel_token_destroy(airy_cancel_token_t *token);

/**
 * @brief Request cancellation: set the atomic flag and fire all wake-up
 * callbacks (idempotent; repeated calls only fire once).
 * @param token Token pointer (may be NULL)
 */
void airy_cancel_token_cancel(airy_cancel_token_t *token);

/**
 * @brief Check whether the token is cancelled (thread-safe, lock-free
 * read of the atomic flag).
 * @param token Token pointer (may be NULL)
 * @return true if cancelled; false if active or invalid args
 */
bool airy_cancel_token_is_canceled(const airy_cancel_token_t *token);

/**
 * @brief Reset the token to active state (for reuse after cancellation;
 * does not reset the callback chain).
 * @param token Token pointer (may be NULL)
 */
void airy_cancel_token_reset(airy_cancel_token_t *token);

/**
 * @brief Register a wake-up callback (appended to the chain; returns
 * AIRY_ERR_OVERFLOW past the limit).
 * @param token Token pointer (non-NULL)
 * @param cb Callback (non-NULL)
 * @param ctx Callback context (may be NULL)
 * @return 0 on success; AIRY_EINVAL invalid args; AIRY_ERR_OVERFLOW chain full
 */
int airy_cancel_token_register(airy_cancel_token_t *token, airy_cancel_cb_t cb, void *ctx);

/**
 * @brief Block waiting for cancellation or timeout (condition-variable
 * wake-up, not busy polling).
 * @param token Token pointer (non-NULL)
 * @param timeout_ms Timeout in ms (0 = wait forever)
 * @return 0 cancelled; 1 timeout; AIRY_EINVAL invalid args
 */
int airy_cancel_token_wait(airy_cancel_token_t *token, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CANCEL_TOKEN_H */
