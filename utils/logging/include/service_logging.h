/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file service_logging.h
 * @brief Unified layered logging system: service-layer API.
 *
 * @details
 * This module provides the service layer of the unified logging system:
 * - Log rotation: automatic rotation by size/time
 * - Log filtering: level-based filtering
 * - Log output: console and file outputters
 * - Monitoring and statistics: throughput counters
 * - Management interface: config reload
 *
 * The service layer is optional; simple applications can use only the
 * core and atomic layers. All functions below are implemented in
 * service_logging.c.
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
 * Configures the features of the service layer.
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
        uint32_t latency_histogram[10];
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
 * @brief Initialize the service layer
 *
 * Initializes the service-layer internal state. Must be called before any
 * other service-layer function.
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
 * @brief Configure log transport
 *
 * Configures remote log transport settings.
 *
 * @param manager Transport configuration
 * @return 0 on success, negative on error
 */
int service_logging_configure_transport(const log_transport_config_t *manager);

/**
 * @brief Register an outputter
 *
 * Registers a new outputter. type 1 = console, type 2 = file (name is the
 * file path).
 *
 * @param name Outputter name (or file path for type 2)
 * @param type Outputter type (1 = console, 2 = file)
 * @param user_data Optional outputter user data
 * @return 0 on success, negative on error
 */
int service_logging_add_outputter(const char *name, int type, void *user_data);

/**
 * @brief Register a filter
 *
 * Registers a new filter. type 1 = level filter (user_data is the minimum
 * log level as an intptr_t).
 *
 * @param name Filter name
 * @param type Filter type (1 = level filter)
 * @param user_data Optional filter user data
 * @return 0 on success, negative on error
 */
int service_logging_add_filter(const char *name, int type, void *user_data);

/**
 * @brief Process a single log record
 *
 * Runs the record through all filters and delivers it to every outputter
 * that accepts it.
 *
 * @param record Log record pointer
 * @return 0 when at least one outputter accepted the record, negative otherwise
 */
int service_logging_process_record(const log_record_t *record);

/**
 * @brief Get monitoring statistics
 *
 * Gets the current monitoring statistics of the logging system.
 *
 * @param stats Output parameter receiving the statistics
 * @return 0 on success, negative on error
 */
int service_logging_get_stats(log_monitoring_stats_t *stats);

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
 * Releases service-layer resources. Must be called before program exit.
 */
void service_logging_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMON_SERVICE_LOGGING_H */
