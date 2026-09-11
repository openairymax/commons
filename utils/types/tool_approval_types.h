/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * tool_approval_types.h - Canonical tool-approval boundary Type Definitions
 *
 * Single source of truth for the tool-approval decision contract that
 * crosses the atoms <-> tool_d boundary. atoms/coreloopthree requests an
 * approval decision before executing a tool; the daemon implements the
 * permission check, parameter sanitization, SafetyGuard chain and audit
 * recording. Both sides MUST include this file instead of defining the
 * types locally (ARC-02: atoms must not include an upper-layer header
 * directly).
 *
 * Only the boundary-crossing subset lives here. The approval entry points
 * (tool_approval_create/destroy/check/check_for_agent/sanitize_params/
 * get_stats/set_safety_guard_bridge/get_agent_id) stay in
 * daemons/tool_d/include/tool_approval.h, which includes this header.
 */

#ifndef AIRY_RT_TOOL_APPROVAL_TYPES_H
#define AIRY_RT_TOOL_APPROVAL_TYPES_H

#include "tool_service_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct safety_guard_bridge_s safety_guard_bridge_t;

/** @brief Tool-approval result. */
typedef enum {
    TOOL_APPROVAL_ALLOWED = 0,
    TOOL_APPROVAL_DENIED,
    TOOL_APPROVAL_SANITIZED,
    TOOL_APPROVAL_PENDING_AUDIT
} tool_approval_result_t;

/** @brief Tool-approval context. */
typedef struct tool_approval_ctx tool_approval_ctx_t;

/** @brief Tool-approval config. */
typedef struct {
    const char *agent_id;
    bool enable_safety_guard_chain;
    bool enable_audit_logging;
    const char *permission_rules;
} tool_approval_config_t;

/** @brief Detailed approval result. */
typedef struct {
    tool_approval_result_t decision;
    char reason[256];
    char sanitized_params[4096];
    int permission_check_passed;
    int safety_guard_passed;
    int params_were_sanitized;
} tool_approval_detail_t;

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TOOL_APPROVAL_TYPES_H */
