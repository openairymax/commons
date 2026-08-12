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
 * LOG_INFO("system started successfully, version: %s", version);
 * LOG_ERROR("connection failed, errno: %d", errno);
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


/**
 * @brief Log level enumeration
 *
 * Defines 5 log levels following the Syslog standard, supporting
 * fine-grained log control.
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


/**
 * @brief Debug-level log macro
 *
 * Writes a DEBUG-level log, typically used during development/debugging.
 */
#define LOG_DEBUG(fmt, ...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Info-level log macro
 *
 * Writes an INFO-level log, used to record normal system operation.
 */
#define LOG_INFO(fmt, ...) log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Warning-level log macro
 *
 * Writes a WARN-level log, indicating a possible problem that does not
 * affect system operation.
 */
#define LOG_WARN(fmt, ...) log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Error-level log macro
 *
 * Writes an ERROR-level log, indicating a functional error that does not
 * crash the system.
 */
#define LOG_ERROR(fmt, ...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Fatal-error-level log macro
 *
 * Writes a FATAL-level log, indicating the system cannot continue. After
 * logging it usually calls abort() to terminate the program.
 */
#define LOG_FATAL(fmt, ...)                                                 \
    do {                                                                    \
        log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        log_flush();                                                        \
        abort();                                                            \
    } while (0)

/**
 * @brief Conditional log macro
 *
 * Writes a log only when the condition holds, avoiding unnecessary string
 * formatting overhead.
 */
#define LOG_IF(condition, level, fmt, ...)                            \
    do {                                                              \
        if (condition) {                                              \
            log_write(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        }                                                             \
    } while (0)


/**
 * @brief Enable log throttling
 *
 * @param enable Whether to enable
 * @param max_per_sec Maximum identical messages per second (0 for the
 *                    default of 100)
 */
void log_set_throttle(bool enable, uint32_t max_per_sec);

/**
 * @brief Sampled log macro
 *
 * Probabilistically writes a log according to the level's sampling rate
 * (ERROR=100%, WARN=10%, INFO=1%, DEBUG=0.1%).
 *
 * @param level Log level (LOG_LEVEL_*)
 * @param fmt Format string
 * @param ... Format arguments
 */
#define LOG_SAMPLE(level, fmt, ...)                                   \
    do {                                                              \
        if (log_should_sample(level)) {                               \
            log_write(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

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
