/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file logging.h
 * @brief Unified layered logging system: core-layer API.
 *
 * @details
 * This module provides the unified layered logging system core interface:
 * - 5 log levels: DEBUG, INFO, WARN, ERROR, FATAL
 * - Multiple output targets: console, file, network, Syslog, memory buffer
 * - Multiple output formats: text, JSON, structured, binary
 * - Thread-safe trace-ID management
 * - Runtime configuration hot reload
 *
 * Architecture layers:
 * 1. Core layer (this file): platform-independent interface and type
 *    definitions
 * 2. Atomic layer: high-performance, thread-safe log-write implementation
 * 3. Service layer: advanced features (rotation, filtering, transport,
 *    monitoring)
 *
 * @note Thread safety: all public interfaces are thread-safe
 * @see ARCHITECTURAL_PRINCIPLES.md E-2 observability principle
 * @see logging_format.md log format specification
 */

/**
 * @details
 * Usage example:
 * @code
 *
 * log_config_t manager = {
 *     .level = LOG_LEVEL_INFO,
 *     .output = LOG_OUTPUT_CONSOLE,
 *     .format = LOG_FORMAT_JSON
 * };
 * log_init(&manager);
 *
 *
 * AIRY_LOG_INFO("system started successfully, version: %s", version);
 * AIRY_LOG_ERROR("connection failed, errno: %d", errno);
 *
 *
 * log_set_trace_id("req-123456");
 *
 *
 * log_cleanup();
 * @endcode
 */

#ifndef AIRY_RT_COMMON_LOGGING_H
#define AIRY_RT_COMMON_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <compat.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * S-2 收敛 (2026-08-14, 用户决策): 引入 AIRY_LOG_* 权威宏源
 * （commons/utils/observability/include/logger.h，基于 airy_log_write →
 * log_write_va）。logging.h 作为统一日志入口头：任何 include 本头的文件
 * 自动获得 AIRY_LOG_* 宏（5 级）与 log_write 函数层。observability/logger.h
 * 不依赖本头（无循环），airy_log_write 实现位于 observability/src/logger.c。
 *
 * 注意：必须使用相对路径而非 <logger.h>，否则会被外部仓
 * products/memoryrovol/include/logger.h（同名头，含 LOG_*→AIRY_LOG_* 兼容
 * 映射）在 -I 顺序中遮蔽，导致 AIRY_LOG_* 宏不可见（S-2 收敛回归验证发现）。
 */
#include "../../observability/include/logger.h"


/**
 * @brief Log level enumeration
 *
 * Defines 5 log levels following the Syslog standard, supporting
 * fine-grained log control.
 *
 * A-ULP SSoT (S-2 收敛, 2026-08-14): 本枚举与 [SC] 共享契约头
 * airymax/log_types.h 的 enum airy_log_level 数值严格一致
 * （DEBUG=0/INFO=1/WARN=2/ERROR=3/FATAL=4），作为用户态内部实现类型；
 * 跨态契约（Ring Buffer/printk）以 airy_log_record 128B 固定格式为准。
 */
typedef enum {

    LOG_LEVEL_DEBUG = 0,


    LOG_LEVEL_INFO = 1,


    LOG_LEVEL_WARN = 2,


    LOG_LEVEL_ERROR = 3,


    LOG_LEVEL_FATAL = 4,


    LOG_LEVEL_COUNT = 5
} log_level_t;

/**
 * @brief Get the log-level name string
 *
 * Converts a log-level enum to a readable string.
 *
 * @param level Log level
 * @return Level name string, "UNKNOWN" for an invalid level
 */
const char *log_level_to_string(log_level_t level);

/**
 * @brief Parse a log level from a string
 *
 * Converts a string (e.g. "DEBUG", "INFO") to a log-level enum.
 *
 * @param str Log-level string (case-insensitive)
 * @return The corresponding log level, LOG_LEVEL_INFO on parse failure
 */
log_level_t log_level_from_string(const char *str);


/**
 * @brief Log output target enumeration
 *
 * Defines the targets logs can be written to; multiple targets can be
 * active simultaneously.
 */
typedef enum {

    LOG_OUTPUT_CONSOLE = 0,


    LOG_OUTPUT_FILE = 1,


    LOG_OUTPUT_SYSLOG = 2,


    LOG_OUTPUT_NETWORK = 3,


    LOG_OUTPUT_BUFFER = 4,


    LOG_OUTPUT_COUNT = 5
} log_output_t;


/**
 * @brief Log output format enumeration
 *
 * Defines the log formatting modes, supporting several structured formats.
 */
typedef enum {

    LOG_FORMAT_TEXT = 0,


    LOG_FORMAT_JSON = 1,


    LOG_FORMAT_STRUCTURED = 2,


    LOG_FORMAT_BINARY = 3,


    LOG_FORMAT_COUNT = 4
} log_format_t;


/**
 * @brief Log record structure
 *
 * Represents a complete log record with all metadata and the message
 * content. Used to pass records between the logging layers.
 */
typedef struct {

    uint64_t timestamp;


    log_level_t level;


    const char *module;


    int line;


    const char *trace_id;


    const char *span_id;


    const char *message;


    uint64_t thread_id;


    uint32_t process_id;
} log_record_t;


/**
 * @brief Log configuration structure
 *
 * Configures the logging system behavior; supports runtime modification.
 */
typedef struct {

    log_level_t level;


    uint32_t outputs;


    log_format_t format;


    const char *file_path;


    size_t max_file_size;


    int max_backup_count;


    const char *network_host;


    uint16_t network_port;


    int syslog_facility;


    bool async_mode;


    size_t async_buffer_size;


    bool enable_statistics;


    bool enable_throttling;


    uint32_t throttle_max_per_sec;
} log_config_t;


/**
 * @brief Initialize the logging system
 *
 * Initializes the logging system with the given configuration. Must be
 * called before any other logging function (except log_set_default_config).
 *
 * @param manager Log configuration, NULL for defaults
 * @return 0 on success, negative on error (error codes in log_error.h)
 */
int log_init(const log_config_t *manager);

/**
 * @brief Set the default log configuration
 *
 * Sets the default configuration used when log_init() is called without a
 * configuration. May be called before log_init() to preset the
 * configuration.
 *
 * @param manager Default log configuration
 * @return 0 on success, negative on error
 */
int log_set_default_config(const log_config_t *manager);

/**
 * @brief Write a log record
 *
 * Writes a log message; the core write function of the logging system.
 * Depending on the configuration, the record may go to multiple targets.
 *
 * @param level Log level
 * @param module Module name (usually __FILE__ or a component name)
 * @param line Source line number (usually __LINE__)
 * @param fmt Format string, printf-style
 * @param ... Format arguments
 */
void log_write(log_level_t level, const char *module, int line, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(__printf__, 4, 5)))
#endif
    ;

/**
 * @brief Write a log record with a va_list
 *
 * The va_list variant of log_write(), for scenarios that need to pass a
 * va_list.
 *
 * @param level Log level
 * @param module Module name
 * @param line Source line number
 * @param fmt Format string
 * @param args Variadic argument list
 */
void log_write_va(log_level_t level, const char *module, int line, const char *fmt, va_list args);

/**
 * @brief Set the current thread's trace ID
 *
 * Sets the trace ID for the current thread; all subsequent logs include
 * this ID. Used to trace request flows in distributed systems.
 *
 * @param trace_id Trace ID string, auto-generated if NULL
 * @return The effective trace ID (internally stored, no need to free)
 */
const char *log_set_trace_id(const char *trace_id);

/**
 * @brief Get the current thread's trace ID
 *
 * @return Trace ID string, NULL if not set
 */
const char *log_get_trace_id(void);

/**
 * @brief Set the current thread's Span ID
 *
 * Sets the OpenTelemetry Span ID for the current thread; all subsequent
 * logs include this ID. Used for span identification in trace linking.
 *
 * @param span_id Span ID string, cleared if NULL
 * @return The effective Span ID (internally stored, no need to free)
 */
const char *log_set_span_id(const char *span_id);

/**
 * @brief Get the current thread's Span ID
 *
 * @return Span ID string, NULL if not set
 */
const char *log_get_span_id(void);

/**
 * @brief Set a module log level
 *
 * Sets an independent log level for a specific module, overriding the
 * global level. Supports wildcard matching (e.g. "network.*" matches all
 * network modules).
 *
 * @param module_pattern Module name pattern (supports wildcard *)
 * @param level Log level
 * @return 0 on success, negative on error
 */
int log_set_module_level(const char *module_pattern, log_level_t level);

/**
 * @brief Module log-level configuration info (read-only snapshot)
 *
 * Describes a single module-level filter rule; filled by
 * log_get_module_info().
 */
typedef struct {
    char pattern[128];
    log_level_t level;
} log_module_info_t;

/**
 * @brief Get the number of configured module-level filters
 *
 * Returns the number of module-level filters registered via
 * log_set_module_level(), for logging introspection and migration
 * monitoring. Thread-safe.
 *
 * @return Number of registered module-level filters (0 when uninitialized)
 */
size_t log_get_module_count(void);

/**
 * @brief Enumerate the configured module-level filters
 *
 * Snapshots the current module-level filter table into the out_info array.
 * Thread-safe.
 *
 * @param[out] out_info Output array, may be NULL (returns 0 in that case)
 * @param[in] max_count out_info array capacity
 * @return Number of entries actually filled (no more than max_count and
 *         the registration total)
 */
size_t log_get_module_info(log_module_info_t *out_info, size_t max_count);

/**
 * @brief Reload the log configuration
 *
 * Reloads the log configuration from a config file; supports hot reload.
 *
 * @param config_path Config file path, NULL for the default path
 * @return 0 on success, negative on error
 */
int log_reload_config(const char *config_path);

/**
 * @brief Flush the log buffer
 *
 * Force-flushes all buffered logs to the output targets. Call before
 * program exit to ensure all logs are written.
 */
void log_flush(void);

/**
 * @brief Clean up the logging system
 *
 * Releases logging resources and flushes all buffered logs. Should be
 * called before program exit.
 */
void log_cleanup(void);


/*
 * S-2 收敛 (2026-08-14, 用户决策: 与 [SC] log_types.h 枚举名对齐):
 * LOG_* 宏已全量迁移为 AIRY_LOG_*（权威宏定义在
 * commons/utils/observability/include/logger.h，调用 airy_log_write →
 * log_write_va）。本头文件仅保留函数层 API（log_write/log_write_va/
 * log_init 等）与 log_level_t 内部类型，不再定义 LOG_* 宏。
 * SVC_LOG_*（服务层专用）仍由 svc_logger.h 提供，映射到 AIRY_LOG_*。
 */

/**
 * @brief Enable log throttling
 *
 * @param enable Whether to enable
 * @param max_per_sec Maximum identical messages per second (0 for the
 *                    default of 100)
 */
void log_set_throttle(bool enable, uint32_t max_per_sec);

/**
 * @brief Check whether the current log should be sampled
 *
 * Probabilistic decision based on the level's sampling rate. ERROR/FATAL
 * always return true (100% sampling).
 *
 * @param level Log level
 * @return true to output, false to skip
 */
bool log_should_sample(log_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMON_LOGGING_H */
