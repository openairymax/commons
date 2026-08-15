/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file observability.h
 * @brief Observability tools (logging, metrics, tracing).
 */

#ifndef AIRY_RT_UTILS_OBSERVABILITY_H
#define AIRY_RT_UTILS_OBSERVABILITY_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* A-ULP SSoT (S-2 收敛, 2026-08-14): 5 级日志枚举与 AIRY_LOG_LEVEL_* 常量、
 * AIRY_LOG_* 宏的唯一入口为同目录 logger.h（数值与 [SC] airymax/log_types.h
 * enum airy_log_level 严格一致）。此处不再重复定义。 */
#include "logger.h"

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Set the current trace ID.
 * @param trace_id Trace ID (auto-generated if NULL)
 * @return The set trace ID
 */
const char *airy_log_set_trace_id(const char *trace_id);

/**
 * @brief Get the current trace ID.
 * @return Trace ID, may be NULL
 */
const char *airy_log_get_trace_id(void);

/**
 * @brief Write a log record.
 * @param level Log level
 * @param file File name (usually __FILE__)
 * @param line Line number (usually __LINE__)
 * @param fmt Format string
 * @param ... Arguments
 */
void airy_log_write(int level, const char *file, int line, const char *fmt, ...);


/**
 * @brief Metrics collector handle.
 */
typedef struct airy_metrics airy_metrics_t;

/**
 * @brief Create a metrics collector.
 * @return Collector handle, NULL on failure
 */
airy_metrics_t *airy_metrics_create(void);

/**
 * @brief Destroy a metrics collector.
 * @param metrics Collector handle
 */
void airy_metrics_destroy(airy_metrics_t *metrics);

/**
 * @brief Increment a counter.
 * @param metrics Collector
 * @param name Metric name
 * @param value Increment value
 */
void airy_metrics_increment(airy_metrics_t *metrics, const char *name, uint64_t value);

/**
 * @brief Set a gauge value.
 * @param metrics Collector
 * @param name Metric name
 * @param value Value
 */
void airy_metrics_gauge(airy_metrics_t *metrics, const char *name, double value);

/**
 * @brief Record a timing.
 * @param metrics Collector
 * @param name Metric name
 * @param duration_ms Duration (milliseconds)
 */
void airy_metrics_timing(airy_metrics_t *metrics, const char *name, double duration_ms);

/**
 * @brief Export metrics as JSON.
 * @param metrics Collector
 * @return JSON string (caller must free), NULL on failure
 */
char *airy_metrics_export(airy_metrics_t *metrics);


/**
 * @brief Trace span handle.
 */
typedef struct airy_trace_span airy_trace_span_t;

/**
 * @brief Begin a span.
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
 * @brief Export trace data as JSON (usually for debugging).
 * @return JSON string (caller must free), NULL on failure
 */
char *airy_trace_export(void);


/**
 * @brief Get the monotonic clock time (nanoseconds).
 * @return Monotonic nanosecond timestamp
 */
static inline uint64_t airy_get_monotonic_time_ns(void)
{
#if defined(_WIN32) || defined(_WIN64)
    /* Windows 无 clock_gettime/CLOCK_MONOTONIC，用 QPC（与
     * platform.h 的 airy_time_ns 同源实现）。 */
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_OBSERVABILITY_H */