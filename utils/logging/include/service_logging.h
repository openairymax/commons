/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file service_logging.h
 * @brief Unified layered logging system: service-layer API.
 *
 * @details
 * This module provides the unified layered logging system service-layer
 * interface with advanced features:
 * - Log rotation: automatic rotation by size/time, with compression and
 *   archiving
 * - Log filtering: level filtering, keyword filtering, regex filtering
 * - Log transport: network transport (TCP/UDP), Syslog integration,
 *   remote collection
 * - Monitoring and statistics: throughput, latency monitoring, error-rate
 *   alerts
 * - Management interface: hot reload, runtime adjustment, query/retrieval
 *
 * Service-layer design principles:
 * 1. Feature rich: all advanced logging features needed in production
 * 2. Flexible configuration: multiple configuration methods and runtime
 *    adjustment
 * 3. Extensibility: plug-in architecture supporting custom outputters and
 *    filters
 * 4. Complete monitoring: comprehensive performance metrics and health
 *    state monitoring
 *
 * Architecture role:
 * - Receives log records from the atomic layer
 * - Applies filter rules and formatting
 * - Outputs logs to multiple targets (file, network, Syslog, etc.)
 * - Provides management interfaces and monitoring data
 *
 * Note: the service layer is optional; simple applications can use only
 * the core and atomic layers. Service-layer features can be disabled via
 * conditional compilation to reduce resource usage.
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
 * @brief Service-layer configuration structure
 *
 * Configures the advanced features of the service layer.
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
 * @brief Log rotation strategy
 *
 * Defines when log rotation is triggered.
 */
typedef enum {

    ROTATION_STRATEGY_SIZE,


    ROTATION_STRATEGY_TIME,


    ROTATION_STRATEGY_BOTH
} rotation_strategy_t;

/**
 * @brief Time-rotation interval
 *
 * Defines the interval unit for time-based rotation.
 */
typedef enum {

    ROTATION_INTERVAL_HOURLY,


    ROTATION_INTERVAL_DAILY,


    ROTATION_INTERVAL_WEEKLY,


    ROTATION_INTERVAL_MONTHLY
} rotation_interval_t;

/**
 * @brief Log rotation configuration
 *
 * Configures the log rotation behavior.
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
 * @brief Filter condition types
 *
 * Defines the matching type of a filter condition.
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
 * @brief Filter condition
 *
 * Defines a single filter condition.
 */
typedef struct {

    filter_match_type_t match_type;


    const char *field;


    const char *pattern;


    bool negate;


    bool case_sensitive;
} filter_condition_t;

/**
 * @brief Filter rule
 *
 * Defines a complete filter rule with multiple conditions and their
 * logical relationship.
 */
typedef struct {

    const char *name;


    filter_condition_t *conditions;


    size_t condition_count;


    bool logical_and;


    bool action_keep;
} filter_rule_t;

/**
 * @brief Filter configuration
 *
 * Configures the log filtering behavior.
 */
typedef struct {

    filter_rule_t *rules;


    size_t rule_count;


    bool default_action;


    bool enable_cache;


    size_t cache_size;
} log_filter_config_t;


/**
 * @brief Transport protocols
 *
 * Defines the network protocols for log transport.
 */
typedef enum {

    TRANSPORT_PROTOCOL_TCP,


    TRANSPORT_PROTOCOL_UDP,


    TRANSPORT_PROTOCOL_TLS,


    TRANSPORT_PROTOCOL_WS,


    TRANSPORT_PROTOCOL_SYSLOG
} transport_protocol_t;

/**
 * @brief Transport configuration
 *
 * Configures the log transport behavior.
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
 * @brief Monitoring configuration
 *
 * Configures the logging system's monitoring behavior.
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
 * @brief Initialize the service layer
 *
 * Initializes the service-layer internal components and starts the worker
 * threads. Must be called before any service-layer function.
 *
 * @param manager Service-layer configuration, NULL for defaults
 * @return 0 on success, negative on error
 */
int service_logging_init(const service_logging_config_t *manager);

/**
 * @brief Configure log rotation
 *
 * Configures the log file rotation behavior.
 *
 * @param manager Rotation configuration
 * @return 0 on success, negative on error
 */
int service_logging_configure_rotation(const log_rotation_config_t *manager);

/**
 * @brief Configure log filtering
 *
 * Configures the log record filter rules.
 *
 * @param manager Filter configuration
 * @return 0 on success, negative on error
 */
int service_logging_configure_filtering(const log_filter_config_t *manager);

/**
 * @brief Configure log transport
 *
 * Configures remote log transport.
 *
 * @param manager Transport configuration
 * @return 0 on success, negative on error
 */
int service_logging_configure_transport(const log_transport_config_t *manager);

/**
 * @brief Configure monitoring and statistics
 *
 * Configures the logging system's monitoring and statistics.
 *
 * @param manager Monitoring configuration
 * @return 0 on success, negative on error
 */
int service_logging_configure_monitoring(const log_monitoring_config_t *manager);

/**
 * @brief Trigger log rotation now
 *
 * Manually triggers log rotation, rotating the current log file
 * immediately.
 *
 * @param reason Rotation reason (for logging)
 * @return 0 on success, negative on error
 */
int service_logging_rotate_now(const char *reason);

/**
 * @brief Add a filter rule
 *
 * Dynamically adds a filter rule, supporting runtime updates.
 *
 * @param rule Filter rule
 * @return Rule ID, negative on error
 */
int service_logging_add_filter_rule(const filter_rule_t *rule);

/**
 * @brief Remove a filter rule
 *
 * Dynamically removes a filter rule.
 *
 * @param rule_id Rule ID
 * @return 0 on success, negative on error
 */
int service_logging_remove_filter_rule(int rule_id);

/**
 * @brief Update a filter rule
 *
 * Dynamically updates a filter rule.
 *
 * @param rule_id Rule ID
 * @param rule New filter rule
 * @return 0 on success, negative on error
 */
int service_logging_update_filter_rule(int rule_id, const filter_rule_t *rule);

/**
 * @brief Test a filter rule
 *
 * Tests whether a filter rule matches the given log record.
 *
 * @param rule_id Rule ID
 * @param record Log record
 * @return true if matched, false otherwise
 */
bool service_logging_test_filter_rule(int rule_id, const log_record_t *record);

/**
 * @brief Send buffered logs to the remote
 *
 * Immediately sends buffered logs to the remote server.
 *
 * @return Number of records sent, negative on error
 */


typedef struct log_monitoring_stats log_monitoring_stats_t;


int service_logging_flush_transport(void);

/**
 * @brief Output a single log record
 *
 * Outputs a single log record through the registered outputters. Called
 * by the atomic-logging layer to bridge the layered logging.
 *
 * @param record Log record pointer
 */
void service_log_output_record(const log_record_t *record);

/**
 * @brief Get monitoring statistics
 *
 * Gets the current monitoring statistics of the logging system.
 *
 * @param out_stats Output parameter receiving the statistics
 * @return 0 on success, negative on error
 */
int service_logging_get_monitoring_stats(struct log_monitoring_stats *out_stats);

/**
 * @brief Reset monitoring statistics
 *
 * Resets all monitoring statistic counters.
 *
 * @return 0 on success, negative on error
 */
int service_logging_reset_monitoring_stats(void);

struct log_query;

/**
 * @brief Query logs
 *
 * Queries log records matching the criteria (requires the log storage
 * feature).
 *
 * @param query Query criteria
 * @param out_records Output array receiving the results
 * @param max_records Maximum number of records to return
 * @param timeout_ms Query timeout (ms)
 * @return Number of records returned, negative on error
 */
int service_logging_query(const struct log_query *query, log_record_t *out_records,
                          size_t max_records, int timeout_ms);

/**
 * @brief Reload the service-layer configuration
 *
 * Reloads the service-layer configuration from a config file.
 *
 * @param config_path Config file path
 * @return 0 on success, negative on error
 */
int service_logging_reload_config(const char *config_path);

/**
 * @brief Clean up the service layer
 *
 * Releases service-layer resources and stops the worker threads. Must be
 * called before program exit.
 */
void service_logging_cleanup(void);


/**
 * @brief Log monitoring statistics
 *
 * Runtime monitoring statistics of the logging system.
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
 * @brief Log query criteria
 *
 * Defines the criteria for querying logs.
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
