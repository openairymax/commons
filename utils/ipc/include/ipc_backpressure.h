/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file ipc_backpressure.h
 * @brief IPC Bus backpressure control - three-level policy.
 *
 * P0.17 phase 4: migrated from daemons/common/include/ to commons,
 * removing the compile-time reverse dependency atoms->daemons (IRON-6).
 * The daemons version is kept as a re-export compat header.
 *
 * P1.24: three-level backpressure policy prevents IPC Bus overload:
 *   - Queue > 80%: producers slow down
 *   - Queue > 90%: droppable messages are dropped
 *   - Queue > 95%: reject new connections + warn
 *   - Queue < 60%: resume normal rate
 */

#ifndef AIRY_RT_IPC_BACKPRESSURE_H
#define AIRY_RT_IPC_BACKPRESSURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backpressure level.
 */
typedef enum {
    IPC_BP_NORMAL = 0,
    IPC_BP_SLOW = 1,
    IPC_BP_DROP = 2,
    IPC_BP_REJECT = 3
} ipc_bp_level_t;

/**
 * @brief Backpressure config.
 */
typedef struct {
    size_t queue_capacity;
    uint32_t slow_threshold_pct;
    uint32_t drop_threshold_pct;
    uint32_t reject_threshold_pct;
    uint32_t recover_threshold_pct;
    uint32_t sample_interval_ms;
} ipc_bp_config_t;

/**
 * @brief Backpressure stats.
 */
typedef struct {
    ipc_bp_level_t current_level;
    size_t queue_depth;
    size_t queue_capacity;
    uint64_t total_sent;
    uint64_t total_dropped;
    uint64_t total_rejected;
    uint64_t slow_down_events;
    uint64_t recover_events;
} ipc_bp_stats_t;

/**
 * @brief Backpressure controller handle.
 */
typedef struct ipc_bp_controller ipc_bp_controller_t;

/**
 * @brief Create a backpressure controller.
 *
 * @param config Config (NULL uses defaults)
 * @return Controller handle, NULL on failure
 */
ipc_bp_controller_t *ipc_bp_create(const ipc_bp_config_t *config);

/**
 * @brief Destroy a backpressure controller.
 */
void ipc_bp_destroy(ipc_bp_controller_t *ctrl);

/**
 * @brief Update the queue depth and evaluate the backpressure level.
 *
 * Sampled every 5s; the backpressure level is computed from the queue
 * depth.
 *
 * @param ctrl Controller
 * @param current_depth Current queue depth
 * @return Current backpressure level
 */
ipc_bp_level_t ipc_bp_update(ipc_bp_controller_t *ctrl, size_t current_depth);

/**
 * @brief Check whether a message should be sent.
 *
 * @param ctrl Controller
 * @param is_droppable Whether the message is droppable
 *                     (log/metrics and other low priority traffic)
 * @return true allows sending, false means drop/reject
 */
bool ipc_bp_should_send(ipc_bp_controller_t *ctrl, bool is_droppable);

/**
 * @brief Check whether a new connection should be accepted.
 *
 * @param ctrl Controller
 * @return true accepts, false rejects
 */
bool ipc_bp_should_accept_connection(ipc_bp_controller_t *ctrl);

/**
 * @brief Get backpressure stats.
 */
void ipc_bp_get_stats(ipc_bp_controller_t *ctrl, ipc_bp_stats_t *out_stats);

/**
 * @brief Get the current backpressure level.
 */
ipc_bp_level_t ipc_bp_get_level(ipc_bp_controller_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_BACKPRESSURE_H */
