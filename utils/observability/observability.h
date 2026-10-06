/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file observability.h
 * @brief Observability umbrella header (logging + tracing).
 *
 * @details
 * S-2 SSoT 收敛（2026-10-06）：本头仅做聚合转发，声明唯一权威源为同目录
 * logger.h（日志）与 trace.h（追踪）。历史遗留的 airy_metrics_* 基础收集器
 * 已随度量族收敛至 unified_metrics.h（um_*）而退役，不再于本头重复声明。
 */

#ifndef AIRY_RT_UTILS_OBSERVABILITY_H
#define AIRY_RT_UTILS_OBSERVABILITY_H

#include "logger.h"
#include "trace.h"

#endif /* AIRY_RT_UTILS_OBSERVABILITY_H */
