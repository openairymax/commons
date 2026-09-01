/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file metrics.h
 * @brief Metrics collection interface.
 */

#ifndef AIRY_RT_UTILS_METRICS_H
#define AIRY_RT_UTILS_METRICS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct airy_metrics airy_metrics_t;

/**
 * @brief Create a metrics collector.
 * @return Collector handle, NULL on failure
 */
airy_metrics_t *airy_metrics_create(void);

/**
 * @brief Destroy a collector.
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
 * @brief Export metrics as a JSON string.
 * @param metrics Collector
 * @return JSON string (caller must free), NULL on failure
 */
char *airy_metrics_export(airy_metrics_t *metrics);

/**
 * @brief Export metrics as a Prometheus format string.
 * @param metrics Collector
 * @return Prometheus format string (caller must free), NULL on failure
 *
 * Output follows the Prometheus exposition format:
 * - Counter: # TYPE name counter \n name value
 * - Gauge: # TYPE name gauge \n name value
 * - Timing: # TYPE name summary \n name_sum value \n name_count count
 */
char *airy_metrics_export_prometheus(airy_metrics_t *metrics);

/**
 * @brief Export metrics with a given prefix as Prometheus format.
 * @param metrics Collector
 * @param prefix Metric name prefix filter (NULL exports all)
 * @return Prometheus format string (caller must free), NULL on failure
 */
char *airy_metrics_export_prometheus_filtered(airy_metrics_t *metrics, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_METRICS_H */