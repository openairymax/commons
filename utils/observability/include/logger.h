// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file logger.h
 * @brief AgentRT 统一日志接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 日志级别值定义（与 logging.h 统一）：
 *   DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4
 * 值越大越严重，与 syslog/Linux 内核惯例一致。
 */

#ifndef AIRY_RT_UTILS_LOGGER_H
#define AIRY_RT_UTILS_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 统一日志级别常量 — 值越大越严重
 * 每个宏独立 #ifndef 保护，避免与 logging_compat.h 交叉包含时重定义 */
#ifndef AIRY_LOG_LEVEL_DEBUG
#define AIRY_LOG_LEVEL_DEBUG 0
#endif
#ifndef AIRY_LOG_LEVEL_INFO
#define AIRY_LOG_LEVEL_INFO 1
#endif
#ifndef AIRY_LOG_LEVEL_WARN
#define AIRY_LOG_LEVEL_WARN 2
#endif
#ifndef AIRY_LOG_LEVEL_ERROR
#define AIRY_LOG_LEVEL_ERROR 3
#endif
#ifndef AIRY_LOG_LEVEL_FATAL
#define AIRY_LOG_LEVEL_FATAL 4
#endif

#ifndef AIRY_LOG_LEVEL
#define AIRY_LOG_LEVEL AIRY_LOG_LEVEL_INFO
#endif

const char *airy_log_set_trace_id(const char *trace_id);
const char *airy_log_get_trace_id(void);
void airy_log_write(int level, const char *file, int line, const char *fmt, ...);

#ifndef AIRY_LOG_ERROR
#define AIRY_LOG_ERROR(fmt, ...) \
    airy_log_write(AIRY_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AIRY_LOG_WARN
#define AIRY_LOG_WARN(fmt, ...) \
    airy_log_write(AIRY_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AIRY_LOG_INFO
#define AIRY_LOG_INFO(fmt, ...) \
    airy_log_write(AIRY_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif

#ifndef AIRY_LOG_DEBUG
#ifdef AIRY_DEBUG
#define AIRY_LOG_DEBUG(fmt, ...) \
    airy_log_write(AIRY_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define AIRY_LOG_DEBUG(fmt, ...) ((void)0)
#endif
#endif

#ifndef AIRY_LOG_FATAL
#define AIRY_LOG_FATAL(fmt, ...)                                                         \
    do {                                                                                    \
        airy_log_write(AIRY_LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        abort();                                                                            \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_LOGGER_H */
