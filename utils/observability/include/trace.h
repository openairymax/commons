/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file trace.h
 * @brief Distributed tracing interface.
 */

#ifndef AIRY_RT_UTILS_TRACE_H
#define AIRY_RT_UTILS_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct airy_trace_span airy_trace_span_t;

/**
 * @brief Begin a trace span.
 * @param name Span name
 * @param parent_id Parent span ID (may be NULL)
 * @return Span handle, NULL on failure
 */
airy_trace_span_t *airy_trace_begin(const char *name, const char *parent_id);

/**
 * @brief End a span.
 * @param span Span handle
 */
void airy_trace_end(airy_trace_span_t *span);

/**
 * @brief Add an event to a span.
 * @param span Span handle
 * @param name Event name
 * @param attributes JSON-formatted attributes (may be NULL)
 */
void airy_trace_add_event(airy_trace_span_t *span, const char *name, const char *attributes);

/**
 * @brief Export all trace data as JSON (for debugging).
 * @return JSON string, caller must free; NULL on failure
 */
char *airy_trace_export(void);

/**
 * @brief Clean up all trace data.
 */
void airy_trace_cleanup(void);

/**
 * @brief Get the current number of trace spans.
 * @return Span count
 */
int airy_trace_get_span_count(void);


/**
 * @brief Get a span's trace ID.
 * @param span Span handle (must be non-NULL)
 * @return Trace ID string (read-only, valid for the span lifetime)
 */
const char *airy_trace_span_get_trace_id(const airy_trace_span_t *span);

/**
 * @brief Get a span's ID.
 * @param span Span handle (must be non-NULL)
 * @return Span ID string (read-only, valid for the span lifetime)
 */
const char *airy_trace_span_get_span_id(const airy_trace_span_t *span);

/**
 * @brief Get a span's parent ID.
 * @param span Span handle (must be non-NULL)
 * @return Parent span ID string (read-only, may be empty)
 */
const char *airy_trace_span_get_parent_id(const airy_trace_span_t *span);

/**
 * @brief Get a span's name.
 * @param span Span handle (must be non-NULL)
 * @return Name string (read-only, valid for the span lifetime)
 */
const char *airy_trace_span_get_name(const airy_trace_span_t *span);

/**
 * @brief Get a span's start time.
 * @param span Span handle (must be non-NULL)
 * @return Start time (microseconds)
 */
int64_t airy_trace_span_get_start_time_us(const airy_trace_span_t *span);

/**
 * @brief Get a span's end time.
 * @param span Span handle (must be non-NULL)
 * @return End time (microseconds), 0 means still running
 */
int64_t airy_trace_span_get_end_time_us(const airy_trace_span_t *span);

/**
 * @brief Get a span's status.
 * @param span Span handle (must be non-NULL)
 * @return Status: 0=running, 1=completed, 2=error
 */
int airy_trace_span_get_status(const airy_trace_span_t *span);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_TRACE_H */