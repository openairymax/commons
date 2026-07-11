// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#ifndef OBSERVABILITY_COMPAT_H
#define OBSERVABILITY_COMPAT_H

#include "observability.h"

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 类型别名 */
typedef airy_metrics_t airy_observability_t;

/* 指标类型常量（兼容旧API）- 供需要整数值的API使用 */
#ifndef AIRY_OBSERVABILITY_METRIC_MACROS_DEFINED
#define AIRY_OBSERVABILITY_METRIC_MACROS_DEFINED
#define AIRY_METRIC_COUNTER 0
#define AIRY_METRIC_GAUGE 1
#define AIRY_METRIC_HISTOGRAM 2
#endif /* AIRY_OBSERVABILITY_METRIC_MACROS_DEFINED */

/* 函数映射：创建 */
static inline airy_observability_t *airy_observability_create(void)
{
    return airy_metrics_create();
}

/* 函数映射：销毁 */
static inline void airy_observability_destroy(airy_observability_t *obs)
{
    airy_metrics_destroy(obs);
}

/* 函数映射：注册指标（新API不需要注册，直接忽略） */
static inline int airy_observability_register_metric(airy_observability_t *obs,
                                                        const char *name, int type,
                                                        const char *desc)
{
    if (!obs || !name)
        return AIRY_EINVAL;
    (void)desc;
    if (type == AIRY_METRIC_COUNTER) {
        airy_metrics_increment(obs, name, 0);
    } else if (type == AIRY_METRIC_GAUGE) {
        airy_metrics_gauge(obs, name, 0.0);
    }
    return AIRY_SUCCESS;
}

/* 函数映射：计数器增加 */
static inline void airy_observability_increment_counter(airy_observability_t *obs,
                                                           const char *label, int64_t value)
{
    airy_metrics_increment(obs, label, (uint64_t)value);
}

/* 函数映射：直方图记录（用timing替代） */
static inline void airy_observability_record_histogram(airy_observability_t *obs,
                                                          const char *name, double value)
{
    airy_metrics_timing(obs, name, value);
}

/* 函数映射：获取单调时间（纳秒） */
static inline uint64_t airy_get_monotonic_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#ifdef __cplusplus
}
#endif

#endif /* OBSERVABILITY_COMPAT_H */
