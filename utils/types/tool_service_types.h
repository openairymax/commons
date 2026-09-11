/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * tool_service_types.h - Canonical tool service boundary Type Definitions
 *
 * Single source of truth for the tool metadata/execution contract that
 * crosses the atoms <-> tool_d boundary. atoms/coreloopthree builds the
 * execution request and interprets the result; daemons/tool_d implements
 * the service. Both sides MUST include this file instead of defining the
 * types locally (ARC-02: atoms must not include an upper-layer header
 * directly).
 *
 * Only the boundary-crossing subset lives here. The service lifecycle
 * entry points (tool_service_create/destroy/register/get/list/stats and
 * the interactive-approval entry points) stay in
 * daemons/tool_d/include/tool_service.h, which includes this header.
 */

#ifndef AIRY_RT_TOOL_SERVICE_TYPES_H
#define AIRY_RT_TOOL_SERVICE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tool_service tool_service_t;

/**
 * @brief Tool access type (improvement 1 P1d: parallel-tool concurrency gating).
 *
 * READ tools are read-only with no side effects -> concurrency gate read
 * lock (multiple tools run in parallel); WRITE tools have side effects ->
 * concurrency gate write lock (mutually exclusive, serial execution).
 * The first enum entry is WRITE (=0), so zero-initialized defaults to
 * mutually-exclusive serial execution (safe default).
 */
typedef enum {
    TOOL_ACCESS_WRITE = 0,
    TOOL_ACCESS_READ = 1,
} tool_access_t;

/** @brief Tool parameter definition (JSON Schema-format string). */
typedef struct {
    const char *name;
    const char *schema;
    int required; /* Whether required (0=optional, 1=required). Consistent
                   * with the gateway tool schema's required array (SSoT):
                   * fs_list.path is optional (omitted -> list the current
                   * dir), fs_read/fs_write/shell_run are required. */
} tool_param_t;

/** @brief Tool metadata. */
typedef struct {
    char *id;
    char *name;
    char *description;
    char *executable;
    tool_param_t *params;
    size_t param_count;
    int timeout_sec;
    int cacheable;
    tool_access_t access;
    char *permission_rule;
} tool_metadata_t;

/** @brief Tool-execution request. */
typedef struct {
    const char *tool_id;
    const char *params_json;
    int stream;
    const char *agent_id;
    void *user_data;
} tool_execute_request_t;

/**
 * @brief Tool-execution failure tiers (improvement 3: Codex
 *        Fatal/RespondToModel/normal three-state).
 *
 * Upper layers (taskflow/work-hall/blueprint scheduling) decide task
 * semantics by tier:
 *   - FATAL             -> terminate the task (fail-closed), cascade-cancel
 *                          related executions
 *   - RESPOND_TO_MODEL  -> return the result to the upper layer, task keeps
 *                          running (start failure/approval denial, etc.)
 *   - NORMAL_FAIL       -> return wrapped success:false, task continues
 *                          (retry configurable)
 */
typedef enum {
    TOOL_RESULT_CLASS_SUCCESS = 0,
    TOOL_RESULT_CLASS_FATAL,
    TOOL_RESULT_CLASS_RESPOND_TO_MODEL,
    TOOL_RESULT_CLASS_NORMAL_FAIL,
} tool_result_class_t;

/** @brief Tool-execution result (non-streaming). */
typedef struct {
    int success;
    char *output;
    char *error;
    int exit_code;
    uint64_t duration_ms;
    tool_result_class_t failure_class;
} tool_result_t;

/**
 * @brief Streaming-output callback.
 * @param chunk  Output data chunk
 * @param is_stderr Whether it is error output
 * @param user_data User data
 */
typedef void (*tool_stream_callback_t)(const char *chunk, int is_stderr, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TOOL_SERVICE_TYPES_H */
