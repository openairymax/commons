/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file service_discovery.h
 * @brief Cross-process service discovery mechanism (authoritative commons
 *        version).
 *
 * P0.17 phase 4: migrated from daemons/common/include/ into commons,
 * removing the atoms->daemons compile-time reverse dependency (IRON-6).
 * The daemons copy is kept as a re-exporting compatibility header.
 *
 * A shared-memory based cross-process service registry, supporting:
 * - Cross-process service registration and discovery
 * - Service health-state propagation
 * - Load-balanced service selection (round-robin/weighted/least
 *   connection)
 * - Service dependency tracking
 * - Heartbeat and automatic expiry
 *
 * Design principles:
 * 1. Zero dependencies: no external registry (e.g. etcd/consul) required
 * 2. High performance: shared-memory based, discovery < 100ms
 * 3. Self-healing: automatic cleanup of expired services, tied to health
 *    checks
 * 4. Cross-platform: Windows/Linux/macOS shared-memory abstraction
 *
 * @see svc_common.h service management framework
 * @see ipc_service_bus.h IPC service bus
 */

#ifndef AIRY_RT_SERVICE_DISCOVERY_H
#define AIRY_RT_SERVICE_DISCOVERY_H

#include "svc_common.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define SD_MAX_SERVICES 128
#define SD_MAX_NAME_LEN 64
#define SD_MAX_ENDPOINT_LEN 256
#define SD_MAX_TYPE_LEN 32
#define SD_MAX_TAGS_LEN 256
#define SD_MAX_DEPS_LEN 512
#define SD_MAX_INSTANCES 8
#define SD_DEFAULT_HEARTBEAT_MS 10000
#define SD_DEFAULT_EXPIRE_MS 30000
#define SD_SHM_NAME "/airy_svc_registry"


typedef struct {
    char instance_id[SD_MAX_NAME_LEN];
    char endpoint[SD_MAX_ENDPOINT_LEN];
    airy_svc_state_t state;
    bool healthy;
    uint32_t weight;
    uint32_t active_connections;
    uint32_t max_connections;
    uint64_t last_heartbeat;
    uint64_t register_time;
    uint32_t pid;
} sd_instance_t;


typedef struct {
    char name[SD_MAX_NAME_LEN];
    char version[32];
    char service_type[SD_MAX_TYPE_LEN];
    char tags[SD_MAX_TAGS_LEN];
    char dependencies[SD_MAX_DEPS_LEN];
    uint32_t capabilities;
    sd_instance_t instances[SD_MAX_INSTANCES];
    uint32_t instance_count;
    bool active;
    uint64_t last_updated;
} sd_service_entry_t;


typedef enum {
    SD_LB_ROUND_ROBIN = 0,
    SD_LB_WEIGHTED = 1,
    SD_LB_LEAST_CONNECTION = 2,
    SD_LB_RANDOM = 3,
    SD_LB_LEAST_LOAD = 4
} sd_lb_strategy_t;


typedef struct {
    uint32_t heartbeat_interval_ms;
    uint32_t expire_timeout_ms;
    sd_lb_strategy_t default_lb_strategy;
    bool enable_auto_expire;
    bool enable_health_propagation;
    char shm_name[256];
    uint32_t shm_size;
} sd_config_t;


typedef struct {
    uint64_t registrations;
    uint64_t deregistrations;
    uint64_t discoveries;
    uint64_t heartbeats;
    uint64_t expirations;
    uint64_t lb_selections;
    uint32_t active_services;
    uint32_t active_instances;
} sd_stats_t;


typedef struct service_discovery_s *service_discovery_t;


typedef enum {
    SD_EVENT_REGISTERED = 1,
    SD_EVENT_DEREGISTERED = 2,
    SD_EVENT_HEALTH_CHANGE = 3,
    SD_EVENT_EXPIRED = 4,
    SD_EVENT_INSTANCE_UP = 5,
    SD_EVENT_INSTANCE_DOWN = 6
} sd_event_type_t;

typedef void (*sd_event_callback_t)(sd_event_type_t event, const char *service_name,
                                    const sd_instance_t *instance, void *user_data);


/**
 * @brief Create a service discovery instance
 * @param config Configuration (NULL for defaults)
 * @return Service discovery handle, NULL on failure
 */
AIRY_API service_discovery_t sd_create(const sd_config_t *config);

/**
 * @brief Destroy a service discovery instance
 * @param sd Service discovery handle
 */
AIRY_API void sd_destroy(service_discovery_t sd);

/**
 * @brief Start service discovery
 * @param sd Service discovery handle
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_start(service_discovery_t sd);

/**
 * @brief Stop service discovery
 * @param sd Service discovery handle
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_stop(service_discovery_t sd);


/**
 * @brief Register a service instance
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param service_type Service type
 * @param instance Instance information
 * @param tags Tags (comma-separated)
 * @param dependencies Dependent services (comma-separated)
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_register(service_discovery_t sd, const char *service_name,
                                const char *service_type, const sd_instance_t *instance,
                                const char *tags, const char *dependencies);

/**
 * @brief Deregister a service instance
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param instance_id Instance ID
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_deregister(service_discovery_t sd, const char *service_name,
                                  const char *instance_id);

/**
 * @brief Deregister all instances of a service
 * @param sd Service discovery handle
 * @param service_name Service name
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_deregister_all(service_discovery_t sd, const char *service_name);


/**
 * @brief Discover a service (get all healthy instances)
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param instances [out] Instance array
 * @param max_count Maximum array capacity
 * @param found_count [out] Actual count found
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_discover(service_discovery_t sd, const char *service_name,
                                sd_instance_t *instances, uint32_t max_count,
                                uint32_t *found_count);

/**
 * @brief Discover services by type
 * @param sd Service discovery handle
 * @param service_type Service type
 * @param entries [out] Service entry array
 * @param max_count Maximum array capacity
 * @param found_count [out] Actual count found
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_discover_by_type(service_discovery_t sd, const char *service_type,
                                        sd_service_entry_t *entries, uint32_t max_count,
                                        uint32_t *found_count);

/**
 * @brief Discover services by tags
 * @param sd Service discovery handle
 * @param tags Tag filter (comma-separated)
 * @param entries [out] Service entry array
 * @param max_count Maximum array capacity
 * @param found_count [out] Actual count found
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_discover_by_tags(service_discovery_t sd, const char *tags,
                                        sd_service_entry_t *entries, uint32_t max_count,
                                        uint32_t *found_count);

/**
 * @brief Select the best instance (load balancing)
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param strategy Load-balancing strategy
 * @param instance [out] Selected instance
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_select_instance(service_discovery_t sd, const char *service_name,
                                       sd_lb_strategy_t strategy, sd_instance_t *instance);


/**
 * @brief Send a heartbeat
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param instance_id Instance ID
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_heartbeat(service_discovery_t sd, const char *service_name,
                                 const char *instance_id);

/**
 * @brief Update an instance's health status
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param instance_id Instance ID
 * @param healthy Whether healthy
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_update_health(service_discovery_t sd, const char *service_name,
                                     const char *instance_id, bool healthy);

/**
 * @brief Update an instance's connection count
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param instance_id Instance ID
 * @param active_connections Current active connection count
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_update_connections(service_discovery_t sd, const char *service_name,
                                          const char *instance_id, uint32_t active_connections);


/**
 * @brief Get a service's dependency list
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param dependencies [out] Dependency list (comma-separated)
 * @param max_len Maximum buffer length
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_get_dependencies(service_discovery_t sd, const char *service_name,
                                        char *dependencies, size_t max_len);

/**
 * @brief Check whether a service's dependencies are satisfied
 * @param sd Service discovery handle
 * @param service_name Service name
 * @param missing_deps [out] Missing dependency list (comma-separated),
 *                           NULL to skip output
 * @param max_len Maximum buffer length
 * @return 0 if all dependencies satisfied, non-zero if any are missing
 */
AIRY_API airy_err_t sd_check_dependencies(service_discovery_t sd, const char *service_name,
                                          char *missing_deps, size_t max_len);


/**
 * @brief Register a service change callback
 * @param sd Service discovery handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_register_event_callback(service_discovery_t sd, sd_event_callback_t callback,
                                               void *user_data);

/**
 * @brief Get service discovery statistics
 * @param sd Service discovery handle
 * @param stats [out] Statistics
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t sd_get_stats(service_discovery_t sd, sd_stats_t *stats);

/**
 * @brief Get the number of registered services
 * @param sd Service discovery handle
 * @return Number of services
 */
AIRY_API uint32_t sd_service_count(service_discovery_t sd);

/**
 * @brief Get the service discovery run state
 * @param sd Service discovery handle
 * @return true if running, false otherwise
 */
AIRY_API bool sd_is_running(service_discovery_t sd);


/**
 * @brief Convert a load-balancing strategy to a string
 * @param strategy Strategy type
 * @return Strategy name
 */
AIRY_API const char *sd_lb_strategy_to_string(sd_lb_strategy_t strategy);

/**
 * @brief Create the default configuration
 * @return Default configuration
 */
AIRY_API sd_config_t sd_create_default_config(void);

/**
 * @brief C-L08: Output a service discovery statistics summary (single-line
 *        format, suitable for periodic logging)
 *
 * Format: "C-L08: SD-STATS services=N instances=N "
 *         "registrations=N deregistrations=N discoveries=N "
 *         "heartbeats=N expirations=N lb_selections=N"
 *
 * @param sd Service discovery handle
 */
AIRY_API void sd_dump_stats(service_discovery_t sd);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SERVICE_DISCOVERY_H */
