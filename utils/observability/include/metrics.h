/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file metrics.h
 * @brief 指标收集接口
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
 * @brief 创建指标收集器
 * @return 收集器句柄，失败返回 NULL
 */
airy_metrics_t *airy_metrics_create(void);

/**
// From data intelligence emerges. by spharx
 * @brief 销毁收集器
 */
void airy_metrics_destroy(airy_metrics_t *metrics);

/**
 * @brief 增加计数器
 * @param metrics 收集器
 * @param name 指标名
 * @param value 增加值
 */
void airy_metrics_increment(airy_metrics_t *metrics, const char *name, uint64_t value);

/**
 * @brief 设置仪表值
 * @param metrics 收集器
 * @param name 指标名
 * @param value 值
 */
void airy_metrics_gauge(airy_metrics_t *metrics, const char *name, double value);

/**
 * @brief 记录耗时
 * @param metrics 收集器
 * @param name 指标名
 * @param duration_ms 耗时（毫秒）
 */
void airy_metrics_timing(airy_metrics_t *metrics, const char *name, double duration_ms);

/**
 * @brief 导出指标为JSON字符串
 * @param metrics 收集器
 * @return JSON字符串（需调用者释放），失败返回 NULL
 */
char *airy_metrics_export(airy_metrics_t *metrics);

/**
 * @brief 导出指标为Prometheus格式字符串
 * @param metrics 收集器
 * @return Prometheus格式字符串（需调用者释放），失败返回 NULL
 *
 * 输出格式遵循Prometheus exposition format:
 * - Counter: # TYPE name counter \n name value
 * - Gauge: # TYPE name gauge \n name value
 * - Timing: # TYPE name summary \n name_sum value \n name_count count
 */
char *airy_metrics_export_prometheus(airy_metrics_t *metrics);

/**
 * @brief 导出指定前缀的指标为Prometheus格式
 * @param metrics 收集器
 * @param prefix 指标名称前缀过滤（NULL导出全部）
 * @return Prometheus格式字符串（需调用者释放），失败返回 NULL
 */
char *airy_metrics_export_prometheus_filtered(airy_metrics_t *metrics, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_METRICS_H */