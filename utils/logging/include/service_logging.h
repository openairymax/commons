/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file service_logging.h
 * @brief 统一分层日志系统服务层API
 *
 * @details
 * 本模块提供统一的分层日志系统服务层接口，提供高级功能：
 * - 日志轮转：基于大小/时间自动轮转，支持压缩和归档
 * - 日志过滤：级别过滤、关键字过滤、正则表达式过滤
 * - 日志传输：网络传输（TCP/UDP）、Syslog集成、远程收集
 * - 监控统计：吞吐量统计、延迟监控、错误率告警
 * - 管理接口：热重载、运行时调整、查询检索
 *
 * 服务层设计原则：
 * 1. **功能丰富**：提供生产环境所需的所有高级日志功能
 * 2. **配置灵活**：支持多种配置方式和运行时调整
 * 3. **可扩展性**：插件化架构，支持自定义输出器和过滤器
 * 4. **监控完备**：全面的性能指标和健康状态监控
 *
 * 架构角色：
 * - 从原子层获取日志记录
 * - 应用过滤规则和格式化
 * - 将日志输出到多个目标（文件、网络、Syslog等）
 * - 提供管理接口和监控数据
 *
 * 注意：服务层是可选的，简单应用可以只使用核心层和原子层。
 * 服务层功能可通过条件编译禁用，以减少资源消耗。
 */

#ifndef AIRY_RT_COMMON_SERVICE_LOGGING_H
#define AIRY_RT_COMMON_SERVICE_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "atomic_logging.h"
#include "logging.h"

#include <stdbool.h>


/**
 * @brief 服务层配置结构体
 *
 * 配置服务层的各项高级功能。
 */
typedef struct {

    bool enable_rotation;


    bool enable_filtering;


    bool enable_transport;


    bool enable_monitoring;


    bool enable_management;


    int worker_threads;


    int max_outputters;


    int max_filters;


    int config_reload_interval;
} service_logging_config_t;


/**
 * @brief 日志轮转策略
 *
 * 定义何时触发日志轮转。
 */
typedef enum {

    ROTATION_STRATEGY_SIZE,


    ROTATION_STRATEGY_TIME,


    ROTATION_STRATEGY_BOTH
} rotation_strategy_t;

/**
 * @brief 时间轮转间隔
 *
 * 定义时间轮转的间隔单位。
 */
typedef enum {

    ROTATION_INTERVAL_HOURLY,


    ROTATION_INTERVAL_DAILY,


    ROTATION_INTERVAL_WEEKLY,


    ROTATION_INTERVAL_MONTHLY
} rotation_interval_t;

/**
 * @brief 日志轮转配置
 *
 * 配置日志轮转的行为。
 */
typedef struct {

    rotation_strategy_t strategy;


    size_t max_file_size;


    rotation_interval_t time_interval;


    int max_backup_files;


    bool compress_backups;


    const char *compression_algorithm;


    const char *time_format;


    const char *filename_pattern;
} log_rotation_config_t;


/**
 * @brief 过滤条件类型
 *
 * 定义过滤条件的匹配类型。
 */
typedef enum {

    FILTER_MATCH_EXACT,


    FILTER_MATCH_PREFIX,


    FILTER_MATCH_SUFFIX,


    FILTER_MATCH_CONTAINS,


    FILTER_MATCH_REGEX,


    FILTER_MATCH_WILDCARD
} filter_match_type_t;

/**
 * @brief 过滤条件
 *
 * 定义单个过滤条件。
 */
typedef struct {

    filter_match_type_t match_type;


    const char *field;


    const char *pattern;


    bool negate;


    bool case_sensitive;
} filter_condition_t;

/**
 * @brief 过滤规则
 *
 * 定义完整的过滤规则，包含多个条件和逻辑关系。
 */
typedef struct {

    const char *name;


    filter_condition_t *conditions;


    size_t condition_count;


    bool logical_and;


    bool action_keep;
} filter_rule_t;

/**
 * @brief 过滤配置
 *
 * 配置日志过滤的行为。
 */
typedef struct {

    filter_rule_t *rules;


    size_t rule_count;


    bool default_action;


    bool enable_cache;


    size_t cache_size;
} log_filter_config_t;


/**
 * @brief 传输协议
 *
 * 定义日志传输的网络协议。
 */
typedef enum {

    TRANSPORT_PROTOCOL_TCP,


    TRANSPORT_PROTOCOL_UDP,


    TRANSPORT_PROTOCOL_TLS,


    TRANSPORT_PROTOCOL_WS,


    TRANSPORT_PROTOCOL_SYSLOG
} transport_protocol_t;

/**
 * @brief 传输配置
 *
 * 配置日志传输的行为。
 */
typedef struct {

    transport_protocol_t protocol;


    const char *host;


    uint16_t port;


    int connect_timeout;


    int send_timeout;


    int max_retries;


    int retry_interval;


    size_t batch_size;


    int batch_timeout;


    size_t buffer_size;


    struct {
        const char *ca_cert_path;
        const char *client_cert_path;
        const char *client_key_path;
        bool verify_peer;
    } tls_config;
} log_transport_config_t;


/**
 * @brief 监控配置
 *
 * 配置日志系统监控的行为。
 */
typedef struct {

    bool enable_throughput;


    bool enable_latency;


    bool enable_error_rate;


    int sampling_interval;


    int history_retention;


    struct {

        uint32_t max_latency_ms;


        uint32_t min_throughput_rps;


        float max_error_rate_percent;
    } alert_thresholds;


    struct {

        bool export_to_file;


        bool export_to_network;


        const char *export_format;


        int export_interval;
    } export_config;
} log_monitoring_config_t;


/**
 * @brief 初始化服务层
 *
 * 初始化服务层内部组件，启动工作线程。
 * 必须在调用任何服务层函数之前调用。
 *
 * @param manager 服务层配置，为NULL时使用默认配置
 * @return 0 成功，负值表示错误
 */
int service_logging_init(const service_logging_config_t *manager);

/**
 * @brief 配置日志轮转
 *
 * 配置日志文件的轮转行为。
 *
 * @param manager 轮转配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_rotation(const log_rotation_config_t *manager);

/**
 * @brief 配置日志过滤
 *
 * 配置日志记录的过滤规则。
 *
 * @param manager 过滤配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_filtering(const log_filter_config_t *manager);

/**
 * @brief 配置日志传输
 *
 * 配置日志的远程传输。
 *
 * @param manager 传输配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_transport(const log_transport_config_t *manager);

/**
 * @brief 配置监控统计
 *
 * 配置日志系统的监控和统计。
 *
 * @param manager 监控配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_monitoring(const log_monitoring_config_t *manager);

/**
 * @brief 启动日志轮转
 *
 * 手动触发日志轮转，立即轮转当前日志文件。
 *
 * @param reason 轮转原因（用于日志记录）
 * @return 0 成功，负值表示错误
 */
int service_logging_rotate_now(const char *reason);

/**
 * @brief 添加过滤规则
 *
 * 动态添加过滤规则，支持运行时更新。
 *
 * @param rule 过滤规则
 * @return 规则ID，负值表示错误
 */
int service_logging_add_filter_rule(const filter_rule_t *rule);

/**
 * @brief 移除过滤规则
 *
 * 动态移除过滤规则。
 *
 * @param rule_id 规则ID
 * @return 0 成功，负值表示错误
 */
int service_logging_remove_filter_rule(int rule_id);

/**
 * @brief 更新过滤规则
 *
 * 动态更新过滤规则。
 *
 * @param rule_id 规则ID
 * @param rule 新的过滤规则
 * @return 0 成功，负值表示错误
 */
int service_logging_update_filter_rule(int rule_id, const filter_rule_t *rule);

/**
 * @brief 测试过滤规则
 *
 * 测试过滤规则是否匹配指定的日志记录。
 *
 * @param rule_id 规则ID
 * @param record 日志记录
 * @return true 匹配，false 不匹配
 */
bool service_logging_test_filter_rule(int rule_id, const log_record_t *record);

/**
 * @brief 发送日志到远程
 *
 * 立即发送缓冲的日志到远程服务器。
 *
 * @return 成功发送的记录数，负值表示错误
 */


typedef struct log_monitoring_stats log_monitoring_stats_t;


int service_logging_flush_transport(void);

/**
 * @brief 输出单条日志记录
 *
 * 通过已注册的outputter输出单条日志记录。
 * 由atomic_logging层调用，实现分层日志桥接。
 *
 * @param record 日志记录指针
 */
void service_log_output_record(const log_record_t *record);

/**
 * @brief 获取监控统计
 *
 * 获取日志系统的当前监控统计信息。
 *
 * @param out_stats 输出参数，接收统计信息
 * @return 0 成功，负值表示错误
 */
int service_logging_get_monitoring_stats(struct log_monitoring_stats *out_stats);

/**
 * @brief 重置监控统计
 *
 * 重置所有监控统计计数器。
 *
 * @return 0 成功，负值表示错误
 */
int service_logging_reset_monitoring_stats(void);

struct log_query;

/**
 * @brief 查询日志
 *
 * 查询符合条件的日志记录（需要启用日志存储功能）。
 *
 * @param query 查询条件
 * @param out_records 输出数组，接收查询结果
 * @param max_records 最大返回记录数
 * @param timeout_ms 查询超时时间（毫秒）
 * @return 实际返回的记录数，负值表示错误
 */
int service_logging_query(const struct log_query *query, log_record_t *out_records,
                          size_t max_records, int timeout_ms);

/**
 * @brief 重新加载服务层配置
 *
 * 从配置文件重新加载服务层配置。
 *
 * @param config_path 配置文件路径
 * @return 0 成功，负值表示错误
 */
int service_logging_reload_config(const char *config_path);

/**
 * @brief 清理服务层
 *
 * 清理服务层资源，停止工作线程。
 * 必须在程序退出前调用。
 */
void service_logging_cleanup(void);


/**
 * @brief 日志监控统计信息
 *
 * 日志系统的运行时监控统计信息。
 */
typedef struct log_monitoring_stats {

    struct {

        uint32_t current_rps;


        uint32_t avg_rps;


        uint32_t max_rps;


        uint64_t total_records;


        uint64_t bytes_per_second;
    } throughput;


    struct {

        uint32_t current_latency_ms;


        uint32_t avg_latency_ms;


        uint32_t max_latency_ms;


        uint32_t latency_histogram[10]; /* 0-10ms, 10-20ms, ..., 90-100ms, 100+ms */

        uint32_t p50_latency_ms;


        uint32_t p90_latency_ms;


        uint32_t p99_latency_ms;
    } latency;


    struct {

        float current_error_rate;


        float avg_error_rate;


        float max_error_rate;


        uint64_t total_errors;


        struct {
            uint64_t io_errors;
            uint64_t network_errors;
            uint64_t format_errors;
            uint64_t other_errors;
        } error_types;
    } error_rate;


    struct {

        size_t current_queue_size;


        size_t max_queue_size;


        float queue_usage;


        uint64_t dropped_records;
    } queue;


    struct {

        size_t memory_usage;


        float cpu_usage;


        int thread_count;


        int fd_count;
    } resource;
} log_monitoring_stats_t;


/**
 * @brief 日志查询条件
 *
 * 定义日志查询的条件。
 */
typedef struct log_query {

    uint64_t start_time;


    uint64_t end_time;


    log_level_t min_level;


    log_level_t max_level;


    const char *module_pattern;


    const char *trace_id;


    const char *keyword;


    bool ascending;


    size_t limit;


    size_t offset;
} log_query_t;

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMON_SERVICE_LOGGING_H */