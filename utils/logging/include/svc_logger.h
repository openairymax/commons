/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file svc_logger.h
 * @brief Service-layer logging compatibility interface (native
 *        definitions).
 *
 * P0.17 phase 2: migrated from daemons/common/include/svc_logger.h to
 * commons/utils/logging/include/svc_logger.h, removing the atoms->daemons
 * compile-time reverse dependency (IRON-6). Provides the SVC_LOG_* macros,
 * the airy_trace_context_t type, and the logger interface, shared by atoms
 * and daemons.
 *
 * @see logging.h
 */

#ifndef AIRY_RT_SVC_LOGGER_H
#define AIRY_RT_SVC_LOGGER_H


#include "error.h"
#include "platform.h"

#include <logging.h>
#include <stdbool.h>
#include <stdio.h>

/* P0.17 phase 2: AIRY_STRNCPY_TERM safe string-copy macro.
 * Originally defined in commons/utils/memory/include/airy_memory.h:607,
 * but fully including airy_memory.h would drag in the heavy error.h/
 * airy_memory.h dependency chain (circular-dependency conflicts with
 * AIRY_EINVAL/AIRY_ERR_BUSY/airy_time_ms). It is inlined here with an
 * ifndef guard to avoid duplicate definitions. */
#ifndef AIRY_STRNCPY_TERM
#define AIRY_STRNCPY_TERM(dst, src, size)                               \
    do {                                                                \
        size_t _len = __builtin_strlen(src);                            \
        size_t _copy = ((_len) < ((size) - 1)) ? (_len) : ((size) - 1); \
        __builtin_memcpy((dst), (src), _copy);                          \
        (dst)[_copy] = '\0';                                            \
    } while (0)
#endif

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

/* Compatibility with old log-level names
 *
 * Note: the AIRY_LOG_DEBUG / INFO / WARN / ERROR / FATAL value-macro
 * definitions were removed because they conflict with the functional
 * macros of the same name in observability/logger.h. When a log-level
 * constant is needed, use the LOG_LEVEL_* enum values directly. When log
 * output is needed, use the AIRY_LOG_*("fmt", ...) functional macros (from
 * observability/logger.h) or the SVC_LOG_* macros.
 */
#ifndef AIRY_LOG_TRACE
#define AIRY_LOG_TRACE LOG_LEVEL_DEBUG
#endif
#ifndef AIRY_LOG_OFF
#define AIRY_LOG_OFF LOG_LEVEL_COUNT
#endif


/**
 * @brief Trace context structure
 * @note For distributed tracing; supports TraceID/SpanID/SessionID
 */
typedef struct {
    char trace_id[36];
    char span_id[16];
    char session_id[36];
    char parent_span_id[16];
} airy_trace_context_t;


/**
 * @brief Logger handle type
 * @note The compatibility layer uses a singleton pattern
 */
typedef struct airy_logger_s *airy_logger_t;


/**
 * @brief Log output target types
 */
typedef log_output_t airy_log_target_type_t;

#define AIRY_LOG_TARGET_FILE LOG_OUTPUT_FILE
#define AIRY_LOG_TARGET_STDOUT LOG_OUTPUT_CONSOLE
#define AIRY_LOG_TARGET_STDERR LOG_OUTPUT_CONSOLE
#define AIRY_LOG_TARGET_SYSLOG LOG_OUTPUT_SYSLOG
#define AIRY_LOG_TARGET_CALLBACK LOG_OUTPUT_NETWORK

/**
 * @brief Log callback function type
 */
typedef void (*airy_log_callback_t)(airy_log_level_t level, const char *timestamp,
                                    const char *logger_name, const airy_trace_context_t *trace_ctx,
                                    const char *message, const char *file, int line,
                                    void *user_data);

/**
 * @brief Log output target configuration
 */
typedef struct {
    airy_log_target_type_t type;
    union {
        struct {
            char path[260];
            uint64_t max_size;
            int max_files;
        } file;
        struct {
            airy_log_callback_t callback;
            void *user_data;
        } callback;
    } config;
} airy_log_target_t;


/**
 * @brief Logger configuration
 */
typedef struct {
    char name[64];
    airy_log_level_t level;
    airy_log_target_t *targets;
    int target_count;
    bool include_source;
    bool include_trace;
    bool json_format;
} airy_logger_config_t;


/**
 * @brief Initialize the logging system
 * @param config [in] Global configuration (may be NULL for defaults)
 * @return 0 on success, non-zero on failure
 */
static inline int airy_log_init(const airy_logger_config_t *config)
{
    log_config_t log_cfg = {0};

    if (config) {
        log_cfg.level = (log_level_t)config->level;
        log_cfg.format = config->json_format ? LOG_FORMAT_JSON : LOG_FORMAT_TEXT;
        log_cfg.outputs = (1 << LOG_OUTPUT_CONSOLE);


        for (int i = 0; i < config->target_count; i++) {
            if (config->targets[i].type == AIRY_LOG_TARGET_FILE) {
                log_cfg.outputs |= (1 << LOG_OUTPUT_FILE);
                log_cfg.file_path = config->targets[i].config.file.path;
                log_cfg.max_file_size = config->targets[i].config.file.max_size;
                log_cfg.max_backup_count = config->targets[i].config.file.max_files;
            }
        }
    }

    return log_init(&log_cfg);
}

/**
 * @brief Shut down the logging system
 */
static inline void airy_log_shutdown(void)
{
    log_cleanup();
}

/**
 * @brief Set the global log level
 * @param level [in] Log level
 */
static inline void airy_log_set_level(airy_log_level_t level)
{
    /* Forward to the real module-level filter ("*" matches every module),
     * instead of a no-op: callers (devtools tests, docs) expect the level
     * change to take effect. */
    (void)log_set_module_level("*", (log_level_t)level);
}

/**
 * @brief Get the global log level
 * @return Current log level
 */
static inline airy_log_level_t airy_log_get_level(void)
{
    /* Read back the "*" filter installed by airy_log_set_level; fall back
     * to the default level when none is configured. */
    log_module_info_t info[1];
    size_t n = log_get_module_info(info, 1);
    for (size_t i = 0; i < n; i++) {
        if (info[i].pattern[0] == '*' && info[i].pattern[1] == '\0')
            return (airy_log_level_t)info[i].level;
    }
    return (airy_log_level_t)LOG_LEVEL_INFO;
}


/**
 * @brief Generate a new trace context
 * @param ctx [out] Trace context output
 */
static inline void airy_trace_new(airy_trace_context_t *ctx)
{
    if (!ctx)
        return;


    snprintf(ctx->trace_id, sizeof(ctx->trace_id), "%08x-%04x-%04x-%04x-%012llx",
             (uint32_t)airy_time_ns(), (uint16_t)(airy_time_ns() >> 16),
             (uint16_t)(airy_time_ns() >> 32), (uint16_t)(airy_time_ns() >> 48),
             (unsigned long long)airy_thread_id());

    ctx->span_id[0] = '\0';
    ctx->session_id[0] = '\0';
    ctx->parent_span_id[0] = '\0';
}

/**
 * @brief Generate a new SpanID
 * @param ctx [in,out] Trace context
 */
static inline void airy_trace_new_span(airy_trace_context_t *ctx)
{
    if (!ctx)
        return;
    snprintf(ctx->span_id, sizeof(ctx->span_id), "%llx", (unsigned long long)airy_time_ns());
}

/**
 * @brief Get the current thread's trace context
 * @return Trace context pointer (thread-local storage)
 */
static inline airy_trace_context_t *airy_trace_current(void)
{
    static AIRY_THREAD_LOCAL airy_trace_context_t tls_trace = {0};
    return &tls_trace;
}

/**
 * @brief Set the current thread's trace context
 * @param ctx [in] Trace context
 */
static inline void airy_trace_set_current(const airy_trace_context_t *ctx)
{
    if (ctx) {
        airy_trace_context_t *current = airy_trace_current();
        if (current) {
            *current = *ctx;
        }
    }
}

/**
 * @brief Set the current session ID
 * @param session_id [in] Session ID
 */
static inline void airy_trace_set_session_id(const char *session_id)
{
    if (session_id) {
        airy_trace_context_t *ctx = airy_trace_current();
        if (ctx) {
            AIRY_STRNCPY_TERM(ctx->session_id, session_id, sizeof(ctx->session_id));
        }
    }
}

/**
 * @brief Get the current session ID
 * @return Session ID string
 */
static inline const char *airy_trace_get_session_id(void)
{
    airy_trace_context_t *ctx = airy_trace_current();
    return ctx ? ctx->session_id : "";
}


/**
 * @brief Get the default logger
 * @return Default logger handle
 */
static inline airy_logger_t airy_logger_default(void)
{
    return (airy_logger_t)1;
}

/**
 * @brief Create a logger
 * @param config [in] Configuration
 * @return Logger handle
 */
static inline airy_logger_t airy_logger_create(const airy_logger_config_t *config)
{
    (void)config;
    return airy_logger_default();
}

/**
 * @brief Destroy a logger
 * @param logger [in] Logger handle
 */
static inline void airy_logger_destroy(airy_logger_t logger)
{
    (void)logger;
}

/**
 * @brief Set a logger's level
 * @param logger [in] Logger handle
 * @param level [in] Log level
 */
static inline void airy_logger_set_level(airy_logger_t logger, airy_log_level_t level)
{
    (void)logger;
    (void)level;
}

/**
 * @brief Write a log record
 * @param logger [in] Logger handle
 * @param level [in] Log level
 * @param file [in] Source file name
 * @param line [in] Line number
 * @param func [in] Function name
 * @param fmt [in] Formatted message
 * @param ... [in] Variadic arguments
 */
static inline void airy_logger_log(airy_logger_t logger, airy_log_level_t level, const char *file,
                                   int line, const char *func, const char *fmt, ...)
{
    (void)logger;
    (void)func;
    va_list args;
    va_start(args, fmt);
    log_write_va((log_level_t)level, file, line, fmt, args);
    va_end(args);
}

/**
 * @brief Write a log record with a trace context
 */
static inline void airy_logger_log_with_trace(airy_logger_t logger, airy_log_level_t level,
                                              const airy_trace_context_t *trace_ctx,
                                              const char *file, int line, const char *func,
                                              const char *fmt, ...)
{
    (void)logger;
    (void)func;

    if (trace_ctx && trace_ctx->trace_id[0]) {
        log_set_trace_id(trace_ctx->trace_id);
    }

    va_list args;
    va_start(args, fmt);
    log_write_va((log_level_t)level, file, line, fmt, args);
    va_end(args);
}


/**
 * @brief Log macro (internal use)
 */
#define AIRY_LOG_IMPL(logger, level, ...)                    \
    do {                                                     \
        (void)(logger);                                      \
        log_write((level), __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

/**
 * @brief Log macro with trace context (internal use)
 */
#define AIRY_LOG_TRACE_IMPL(logger, level, trace_ctx, ...)     \
    do {                                                       \
        (void)(logger);                                        \
        if ((trace_ctx) != NULL && (trace_ctx)->trace_id[0]) { \
            log_set_trace_id((trace_ctx)->trace_id);           \
        }                                                      \
        log_write((level), __FILE__, __LINE__, __VA_ARGS__);   \
    } while (0)


/*
 * S-2 收敛 (2026-08-14, 用户决策): 5 级 LOG_* 宏已全量迁移为 AIRY_LOG_*
 * （权威定义在 observability/logger.h）。此处删除 LOG_TRACE/LOG_DEBUG/
 * LOG_INFO/LOG_WARN/LOG_ERROR/LOG_FATAL 旧宏，防止 LOG_* 轨复活；
 * 保留带 trace 上下文的 *_T 变体（服务层扩展，改名 AIRY_LOG_*_T 对齐
 * AIRY_LOG_* 前缀）；SVC_LOG_* 服务层宏保留，映射到 AIRY_LOG_*。
 */

/**
 * @brief Trace-level log with trace context
 */
#define AIRY_LOG_TRACE_T(ctx, ...) \
    AIRY_LOG_TRACE_IMPL(airy_logger_default(), LOG_LEVEL_DEBUG, (ctx), __VA_ARGS__)

/**
 * @brief Debug-level log with trace context
 */
#define AIRY_LOG_DEBUG_T(ctx, ...) \
    AIRY_LOG_TRACE_IMPL(airy_logger_default(), LOG_LEVEL_DEBUG, (ctx), __VA_ARGS__)

/**
 * @brief Info-level log with trace context
 */
#define AIRY_LOG_INFO_T(ctx, ...) \
    AIRY_LOG_TRACE_IMPL(airy_logger_default(), LOG_LEVEL_INFO, (ctx), __VA_ARGS__)

/**
 * @brief Warning-level log with trace context
 */
#define AIRY_LOG_WARN_T(ctx, ...) \
    AIRY_LOG_TRACE_IMPL(airy_logger_default(), LOG_LEVEL_WARN, (ctx), __VA_ARGS__)

/**
 * @brief Error-level log with trace context
 */
#define AIRY_LOG_ERROR_T(ctx, ...) \
    AIRY_LOG_TRACE_IMPL(airy_logger_default(), LOG_LEVEL_ERROR, (ctx), __VA_ARGS__)

/**
 * @brief Fatal-level log with trace context
 */
#define AIRY_LOG_FATAL_T(ctx, ...) \
    AIRY_LOG_TRACE_IMPL(airy_logger_default(), LOG_LEVEL_FATAL, (ctx), __VA_ARGS__)


/**
 * @brief Log an error and return the error code
 */
#define AIRY_LOG_ERROR_RETURN(code, ...) \
    do {                                 \
        AIRY_LOG_ERROR(__VA_ARGS__);     \
        return (code);                   \
    } while (0)

/**
 * @brief Conditional-check log
 */
#define AIRY_LOG_CHECK(cond, level, ...)                                \
    do {                                                                \
        if (!(cond)) {                                                  \
            AIRY_LOG_IMPL(airy_logger_default(), (level), __VA_ARGS__); \
        }                                                               \
    } while (0)


/**
 * @brief SVC_-prefixed log macros - fully equivalent to AIRY_LOG_*
 * @note The daemon service layer uniformly uses the SVC_ prefix to avoid
 *       name conflicts with other modules. These macros map directly to
 *       the AIRY_LOG_* 权威宏 (observability/logger.h) → commons log_write().
 */


#define SVC_LOG_TRACE(...) AIRY_LOG_DEBUG(__VA_ARGS__)


#define SVC_LOG_DEBUG(...) AIRY_LOG_DEBUG(__VA_ARGS__)


#define SVC_LOG_INFO(...) AIRY_LOG_INFO(__VA_ARGS__)


#define SVC_LOG_WARN(...) AIRY_LOG_WARN(__VA_ARGS__)


#define SVC_LOG_ERROR(...) AIRY_LOG_ERROR(__VA_ARGS__)


#define SVC_LOG_FATAL(...) AIRY_LOG_FATAL(__VA_ARGS__)


/**
 * @brief Convert a log level to a string
 */
static inline const char *airy_log_level_to_string(airy_log_level_t level)
{
    return log_level_to_string((log_level_t)level);
}

/**
 * @brief Convert a string to a log level
 */
static inline airy_log_level_t airy_log_level_from_string(const char *str)
{
    return (airy_log_level_t)log_level_from_string(str);
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SVC_LOGGER_H */
