/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * safety_guard_types.h - Canonical SafetyGuard boundary Type Definitions
 *
 * Single source of truth for the SafetyGuard event/result contract that
 * crosses the atoms <-> cupolas boundary. atoms/coreloopthree builds the
 * event and interprets the decision; cupolas implements the guard chain.
 * Both sides MUST include this file instead of defining the types locally
 * (ARC-02: atoms must not include an upper-layer header directly).
 *
 * Only the boundary-crossing subset lives here. Guard descriptors, policies,
 * quotas, audit entries and callbacks stay in cupolas/include/safety_guard.h,
 * which includes this header.
 */

#ifndef AIRY_RT_SAFETY_GUARD_TYPES_H
#define AIRY_RT_SAFETY_GUARD_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAFETY_MAX_SUBJECT_LEN 128
#define SAFETY_MAX_ACTION_LEN 64
#define SAFETY_MAX_RESOURCE_LEN 256

typedef enum {
    SAFETY_EVENT_ACCESS_REQUEST = 0,
    SAFETY_EVENT_RESOURCE_ALLOCATE,
    SAFETY_EVENT_DATA_FLOW,
    SAFETY_EVENT_EXECUTION_START,
    SAFETY_EVENT_EXECUTION_COMPLETE,
    SAFETY_EVENT_POLICY_CHANGE,
    SAFETY_EVENT_QUOTA_EXCEEDED,
    SAFETY_EVENT_VIOLATION_DETECTED,
    SAFETY_EVENT_EMERGENCY_STOP
} safety_event_type_t;

typedef enum {
    SAFETY_DECISION_ALLOW = 0,
    SAFETY_DECISION_DENY,
    SAFETY_DECISION_CONDITIONAL,
    SAFETY_DECISION_DEFER,
    SAFETY_DECISION_ABORT
} safety_decision_t;

typedef enum {
    SAFETY_SEVERITY_INFO = 0,
    SAFETY_SEVERITY_WARNING,
    SAFETY_SEVERITY_ERROR,
    SAFETY_SEVERITY_CRITICAL,
    SAFETY_SEVERITY_FATAL
} safety_severity_t;

typedef struct {
    safety_event_type_t type;
    char subject[SAFETY_MAX_SUBJECT_LEN];
    char action[SAFETY_MAX_ACTION_LEN];
    char resource[SAFETY_MAX_RESOURCE_LEN];
    const void *context;
    size_t context_size;
    uint64_t timestamp;
    uint32_t flags;
} safety_event_t;

typedef struct {
    safety_decision_t decision;
    char reason[256];
    safety_severity_t severity;
    uint32_t conditions;
    void *modified_context;
    size_t modified_context_size;
} safety_result_t;

typedef struct safety_guard_context_s safety_guard_context_t;

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SAFETY_GUARD_TYPES_H */
