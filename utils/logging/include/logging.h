/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file logging.h
 * @brief 统一分层日志系统核心层API
 *
 * @details
 * 本模块提供统一的分层日志系统核心层接口，支持：
 * - 5级日志级别：DEBUG, INFO, WARN, ERROR, FATAL
 * - 多输出目标：控制台、文件、网络、Syslog、内存缓冲
 * - 多格式输出：文本、JSON、结构化、二进制
 * - 线程安全的追踪ID管理
 * - 运行时配置热重载
 *
 * 架构层次：
 * 1. 核心层（本文件）：定义接口和数据类型的平台无关抽象
 * 2. 原子层：提供高性能、线程安全的日志写入实现
 * 3. 服务层：提供高级功能（轮转、过滤、传输、监控）
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-03-30
 * @version 2.0
 *
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-2 可观测性原则
 * @see logging_format.md 日志格式规范
 */

/**
 * @details
 * 使用示例：
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
 * LOG_INFO("系统启动成功，版本: %s", version);
 * LOG_ERROR("连接失败，错误码: %d", errno);
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
 * @brief 日志级别枚举
 *
 * 定义5级日志级别，遵循Syslog标准，支持细粒度日志控制。
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
 * @brief 获取日志级别名称字符串
 *
 * 将日志级别枚举转换为可读的字符串表示。
 *
 * @param level 日志级别
 * @return 级别名称字符串，无效级别返回"UNKNOWN"
 */
const char *log_level_to_string(log_level_t level);

/**
 * @brief 从字符串解析日志级别
 *
 * 将字符串（如"DEBUG"、"INFO"）转换为日志级别枚举。
 *
 * @param str 日志级别字符串（不区分大小写）
 * @return 对应的日志级别，解析失败返回LOG_LEVEL_INFO
 */
log_level_t log_level_from_string(const char *str);


/**
 * @brief 日志输出目标枚举
 *
 * 定义日志可以输出的目标位置，支持多目标同时输出。
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
 * @brief 日志输出格式枚举
 *
 * 定义日志的格式化方式，支持多种结构化格式。
 */
typedef enum {

    LOG_FORMAT_TEXT = 0,


    LOG_FORMAT_JSON = 1,


    LOG_FORMAT_STRUCTURED = 2,


    LOG_FORMAT_BINARY = 3,


    LOG_FORMAT_COUNT = 4
} log_format_t;


/**
 * @brief 日志记录结构体
 *
 * 表示一条完整的日志记录，包含所有元数据和消息内容。
 * 用于在日志系统的不同层次之间传递。
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
 * @brief 日志配置结构体
 *
 * 配置日志系统的行为，支持运行时动态修改。
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
 * @brief 初始化日志系统
 *
 * 初始化日志系统，应用指定的配置。
 * 必须在调用任何其他日志函数之前调用（除log_set_default_config外）。
 *
 * @param manager 日志配置，为NULL时使用默认配置
 * @return 0 成功，负值表示错误（错误码定义在log_error.h）
 */
int log_init(const log_config_t *manager);

/**
 * @brief 设置默认日志配置
 *
 * 设置日志系统的默认配置，这些配置将在log_init()未提供配置时使用。
 * 可以在log_init()之前调用，用于预设配置。
 *
 * @param manager 默认日志配置
 * @return 0 成功，负值表示错误
 */
int log_set_default_config(const log_config_t *manager);

/**
 * @brief 记录日志
 *
 * 记录一条日志消息，这是日志系统的核心写入函数。
 * 根据配置，日志可能会被输出到多个目标。
 *
 * @param level 日志级别
 * @param module 模块名称（通常使用__FILE__或组件名）
 * @param line 源代码行号（通常使用__LINE__）
 * @param fmt 格式化字符串，遵循printf格式
 * @param ... 格式化参数
 */
void log_write(log_level_t level, const char *module, int line, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(__printf__, 4, 5)))
#endif
    ;

/**
 * @brief 记录可变参数日志
 *
 * log_write()的可变参数版本，用于需要传递va_list的场景。
 *
 * @param level 日志级别
 * @param module 模块名称
 * @param line 源代码行号
 * @param fmt 格式化字符串
 * @param args 可变参数列表
 */
void log_write_va(log_level_t level, const char *module, int line, const char *fmt, va_list args);

/**
 * @brief 设置当前线程的追踪ID
 *
 * 为当前线程设置追踪ID，所有后续日志都会包含此ID。
 * 用于分布式系统跟踪请求流程。
 *
 * @param trace_id 追踪ID字符串，为NULL时自动生成
 * @return 实际设置的追踪ID（内部存储，无需释放）
 */
const char *log_set_trace_id(const char *trace_id);

/**
 * @brief 获取当前线程的追踪ID
 *
 * 获取当前线程设置的追踪ID。
 *
 * @return 追踪ID字符串，未设置返回NULL
 */
const char *log_get_trace_id(void);

/**
 * @brief 设置当前线程的Span ID
 *
 * 为当前线程设置OpenTelemetry Span ID，所有后续日志都会包含此ID。
 * 用于链路追踪中的跨度标识。
 *
 * @param span_id Span ID字符串，为NULL时清除
 * @return 实际设置的Span ID（内部存储，无需释放）
 */
const char *log_set_span_id(const char *span_id);

/**
 * @brief 获取当前线程的Span ID
 *
 * @return Span ID字符串，未设置返回NULL
 */
const char *log_get_span_id(void);

/**
 * @brief 设置模块日志级别
 *
 * 为特定模块设置独立的日志级别，覆盖全局级别。
 * 支持通配符匹配（如"network.*"匹配所有network模块）。
 *
 * @param module_pattern 模块名称模式（支持通配符*）
 * @param level 日志级别
 * @return 0 成功，负值表示错误
 */
int log_set_module_level(const char *module_pattern, log_level_t level);

/**
 * @brief 模块日志级别配置信息（只读快照）
 *
 * 描述单个模块级别过滤规则，由 log_get_module_info() 填充。
 */
typedef struct {
    char pattern[128];
    log_level_t level;
} log_module_info_t;

/**
 * @brief 获取已配置模块级别过滤器的数量
 *
 * 返回通过 log_set_module_level() 注册的模块级别过滤器数量，
 * 用于日志系统内省与迁移监控。线程安全。
 *
 * @return 已注册的模块级别过滤器数量（未初始化时返回0）
 */
size_t log_get_module_count(void);

/**
 * @brief 枚举已配置的模块级别过滤器
 *
 * 将当前模块级别过滤表的内容快照复制到 out_info 数组。线程安全。
 *
 * @param[out] out_info 输出数组，可为NULL（此时返回0）
 * @param[in] max_count out_info 数组容量
 * @return 实际填充的条目数（不超过 max_count 和注册总数）
 */
size_t log_get_module_info(log_module_info_t *out_info, size_t max_count);

/**
 * @brief 重新加载日志配置
 *
 * 从配置文件重新加载日志配置，支持热重载。
 *
 * @param config_path 配置文件路径，为NULL时使用默认路径
 * @return 0 成功，负值表示错误
 */
int log_reload_config(const char *config_path);

/**
 * @brief 刷新日志缓冲
 *
 * 强制刷新所有缓冲的日志到输出目标。
 * 在程序退出前调用以确保所有日志都被写入。
 */
void log_flush(void);

/**
 * @brief 清理日志系统
 *
 * 清理日志系统资源，刷新所有缓冲的日志。
 * 程序退出前应该调用此函数。
 */
void log_cleanup(void);


/**
 * @brief 调试级别日志宏
 *
 * 记录DEBUG级别的日志，通常在开发调试时使用。
 */
#define LOG_DEBUG(fmt, ...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 信息级别日志宏
 *
 * 记录INFO级别的日志，用于记录系统正常运行状态。
 */
#define LOG_INFO(fmt, ...) log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 警告级别日志宏
 *
 * 记录WARN级别的日志，表示可能的问题但不影响系统运行。
 */
#define LOG_WARN(fmt, ...) log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 错误级别日志宏
 *
 * 记录ERROR级别的日志，表示功能错误但不导致系统崩溃。
 */
#define LOG_ERROR(fmt, ...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 致命错误级别日志宏
 *
 * 记录FATAL级别的日志，表示系统无法继续运行。
 * 记录日志后通常会调用abort()终止程序。
 */
#define LOG_FATAL(fmt, ...)                                                 \
    do {                                                                    \
        log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        log_flush();                                                        \
        abort();                                                            \
    } while (0)

/**
 * @brief 条件日志宏
 *
 * 仅当条件成立时记录日志，避免不必要的字符串格式化开销。
 */
#define LOG_IF(condition, level, fmt, ...)                            \
    do {                                                              \
        if (condition) {                                              \
            log_write(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        }                                                             \
    } while (0)


/**
 * @brief 启用日志节流
 *
 * @param enable 是否启用
 * @param max_per_sec 每秒最大相同消息数（0使用默认值100）
 */
void log_set_throttle(bool enable, uint32_t max_per_sec);

/**
 * @brief 采样日志宏
 *
 * 根据日志级别的采样率（ERROR=100%, WARN=10%, INFO=1%, DEBUG=0.1%）
 * 概率性地记录日志。
 *
 * @param level 日志级别（LOG_LEVEL_*）
 * @param fmt 格式化字符串
 * @param ... 格式化参数
 */
#define LOG_SAMPLE(level, fmt, ...)                                   \
    do {                                                              \
        if (log_should_sample(level)) {                               \
            log_write(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

/**
 * @brief 检查当前日志是否应被采样输出
 *
 * 基于日志级别的采样率进行概率性判断。
 * ERROR/FATAL 始终返回true（100%采样）。
 *
 * @param level 日志级别
 * @return true 应输出，false 跳过
 */
bool log_should_sample(log_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMON_LOGGING_H */