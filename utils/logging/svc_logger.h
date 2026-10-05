/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file svc_logger.h
 * @brief Service-layer log macro aliases (SVC_LOG_* -> AIRY_LOG_*).
 *
 * 0.1.19 §205 收敛：本头原为兼容外观层（ airy_logger_* 对象族、trace
 * 上下文族、AIRY_LOG_*_T 变体等），勘察证实其主体外部零消费、与
 * observability/logger.h 权威宏链重复，全部退役；现仅保留守护进程
 * 服务层惯用的 SVC_LOG_* 一一别名与 log_level_t 用户态别名。日志
 * 初始化/清理直接使用 logging.h 的 log_init/log_cleanup。
 *
 * @see logging.h observability/logger.h
 */

#ifndef AIRY_RT_SVC_LOGGER_H
#define AIRY_RT_SVC_LOGGER_H

#include "error.h"
#include "platform.h"
/* SSoT: AIRY_STRNCPY_TERM 权威定义点为 airy_memory_inline.h（可移植，
 * MSVC 走 CRT 分支），本头历史副本硬编码 __builtin_*（MSVC 不可编译），
 * 已删除改为重导出。 */
#include "../memory/airy_memory_inline.h"

#include <logging.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log level type — 用户态唯一别名 (S-2 收敛, 2026-08-14)
 *
 * 5 级日志枚举的唯一权威源为 [SC] 共享契约头 airymax/log_types.h 的
 * enum airy_log_level（AIRY_LOG_DEBUG=0 .. AIRY_LOG_FATAL=4）。
 * 用户态统一以 log_level_t（utils/logging/logging.h）为内部实现类型，
 * 本 typedef 是跨模块兼容别名，数值与 [SC] 严格一致（0-4）。
 * 旧的 _E 后缀枚举（types.h）已删除——它与此 typedef 以互斥保护
 * 双定义同一类型名，导致类型随 include 顺序漂移。
 */
#ifndef AIRY_LOG_LEVEL_T_DEFINED
#define AIRY_LOG_LEVEL_T_DEFINED
typedef log_level_t airy_log_level_t;
#endif

/*
 * S-2 收敛 (2026-08-14) + §205 收敛 (0.1.19): SVC_LOG_TRACE 与
 * SVC_LOG_FATAL 零消费者已删除（TRACE≡DEBUG、FATAL 语义由 FATAL 级
 * 宏承担）；服务层实际使用的四级别名保留，一一映射到权威宏
 * （observability/logger.h）→ commons log_write()。
 */

#define SVC_LOG_DEBUG(...) AIRY_LOG_DEBUG(__VA_ARGS__)

#define SVC_LOG_INFO(...) AIRY_LOG_INFO(__VA_ARGS__)

#define SVC_LOG_WARN(...) AIRY_LOG_WARN(__VA_ARGS__)

#define SVC_LOG_ERROR(...) AIRY_LOG_ERROR(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SVC_LOGGER_H */
