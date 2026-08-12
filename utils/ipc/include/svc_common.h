/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file svc_common.h
 * @brief Common service definitions (authoritative commons version).
 *
 * Provides definitions and interfaces shared by all services.
 *
 * P0.17 phase 3: migrated from daemons/common/include/svc_common.h into
 * commons, removing the atoms->daemons compile-time reverse dependency
 * (IRON-6). The daemons copy is kept as a re-exporting compatibility
 * header (it additionally provides daemon_errors.h with the daemons
 * extended error codes).
 *
 * Design principles (architecture design principle K-2, interface
 * contract):
 * 1. Unified service interface definitions
 * 2. Explicit lifecycle management
 * 3. Standardized error handling
 *
 * @see agentrt/daemons/common/include/svc_common.h (daemons re-export
 *      compatibility header)
 */

#ifndef AIRY_RT_SVC_COMMON_H
#define AIRY_RT_SVC_COMMON_H

#include "error.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h> /* time_t (airy_config_t.last_modified) */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Service state enumeration
 */
typedef enum {
    AIRY_SVC_STATE_NONE = 0,
    AIRY_SVC_STATE_CREATED,
    AIRY_SVC_STATE_INITIALIZING,
    AIRY_SVC_STATE_READY,
    AIRY_SVC_STATE_RUNNING,
    AIRY_SVC_STATE_PAUSED,
    AIRY_SVC_STATE_STOPPING,
    AIRY_SVC_STATE_STOPPED,
    AIRY_SVC_STATE_ZOMBIE,
    AIRY_SVC_STATE_ERROR
} airy_svc_state_t;


/**
 * @brief Service capability flags
 */
typedef enum {
    AIRY_SVC_CAP_NONE = 0,
    AIRY_SVC_CAP_ASYNC = 1 << 0,
    AIRY_SVC_CAP_STREAMING = 1 << 1,
    AIRY_SVC_CAP_CANCELABLE = 1 << 2,
    AIRY_SVC_CAP_PAUSEABLE = 1 << 3,
    AIRY_SVC_CAP_THROTTLE = 1 << 4,
    AIRY_SVC_CAP_BATCH = 1 << 5,
    AIRY_SVC_CAP_PRIORITY = 1 << 6,
    AIRY_SVC_CAP_TIMEOUT = 1 << 7,
} airy_svc_capability_t;


/**
 * @brief Service configuration structure
 */
typedef struct {
    const char *name;
    const char *version;
    uint32_t capabilities;
    uint32_t max_concurrent;
    uint32_t timeout_ms;
    int priority;
    bool auto_start;
    bool enable_metrics;
    bool enable_tracing;
} airy_svc_config_t;


/**
 * @brief Service statistics
 */
typedef struct {
    uint64_t request_count;
    uint64_t success_count;
    uint64_t error_count;
    uint64_t total_time_ms;
    uint64_t max_time_ms;
    uint64_t min_time_ms;
    uint32_t current_concurrent;
    uint32_t peak_concurrent;
    double avg_time_ms;
} airy_svc_stats_t;


/**
 * @brief Service handle type
 */
typedef struct airy_svc_s *airy_svc_t;


/**
 * @brief Service initialization function type
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param config [in] Configuration (BORROW - not stored, copied internally).
 * @return 0 on success, non-zero on failure
 */
typedef airy_err_t (*airy_svc_init_fn)(airy_svc_t service, const airy_svc_config_t *config);

/**
 * @brief Service start function type
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 */
typedef airy_err_t (*airy_svc_start_fn)(airy_svc_t service);

/**
 * @brief Service stop function type
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param force Whether to force-stop
 * @return 0 on success, non-zero on failure
 */
typedef airy_err_t (*airy_svc_stop_fn)(airy_svc_t service, bool force);

/**
 * @brief Service destroy function type
 * @param service [in] Service handle (TRANSFER - function takes ownership and frees).
 */
typedef void (*airy_svc_destroy_fn)(airy_svc_t service);

/**
 * @brief Service health-check function type
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 if healthy, non-zero if unhealthy
 */
typedef airy_err_t (*airy_svc_healthcheck_fn)(airy_svc_t service);

/**
 * @brief Service request handler function type
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param method [in] Method name (BORROW - valid for function scope only).
 * @param params_json [in] Request parameters JSON (BORROW - valid for function scope only).
 * @param response_json [out] Response JSON (OWNER - caller must free).
 * @param user_data [in] User data (BORROW - caller retains ownership).
 * @return Error code
 */
typedef airy_err_t (*airy_svc_handle_request_fn)(airy_svc_t service, const char *method,
                                                 const char *params_json, char **response_json,
                                                 void *user_data);

/**
 * @brief Service async-completion callback function type
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param method [in] Method name (BORROW - valid for callback scope only).
 * @param error_code Error code
 * @param response_json [in] Response JSON (TRANSFER - callback takes ownership and must free).
 * @param user_data [in] User data (BORROW - caller retains ownership).
 */
typedef void (*airy_svc_async_complete_fn)(airy_svc_t service, const char *method,
                                           airy_err_t error_code, char *response_json,
                                           void *user_data);

typedef struct {
    airy_svc_init_fn init;
    airy_svc_start_fn start;
    airy_svc_stop_fn stop;
    airy_svc_destroy_fn destroy;
    airy_svc_healthcheck_fn healthcheck;
    airy_svc_handle_request_fn handle_request;
} airy_svc_interface_t;


/**
 * @brief Create a service instance
 * @param service [out] Service handle output (OWNER - caller must call airy_svc_destroy).
 * @param name [in] Service name (BORROW - not stored, copied internally).
 * @param iface [in] Service interface (BORROW - copied internally, not stored by pointer).
 * @param config [in] Service configuration (BORROW - not stored, copied internally).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: OWNER, name: BORROW, iface: BORROW, config: BORROW
 */
AIRY_API airy_err_t airy_svc_create(airy_svc_t *service, const char *name,
                                    const airy_svc_interface_t *iface,
                                    const airy_svc_config_t *config);

/**
 * @brief Destroy a service instance
 * @param service [in] Service handle (TRANSFER - function takes ownership and frees).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: TRANSFER
 */
AIRY_API void airy_svc_destroy(airy_svc_t service);

/**
 * @brief Initialize a service
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe No
 * @reentrant No
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_init(airy_svc_t service);

/**
 * @brief Start a service
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe No
 * @reentrant No
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_start(airy_svc_t service);

/**
 * @brief Stop a service
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param force [in] Whether to force-stop
 * @return 0 on success, non-zero on failure
 * @threadsafe No
 * @reentrant No
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_stop(airy_svc_t service, bool force);

/**
 * @brief Set the thread pool for a service.
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param pool [in] Thread-pool pointer (BORROW - service does not take ownership, caller manages lifecycle).
 * @return Error code
 *
 * @ownership service: BORROW, pool: BORROW
 */
AIRY_API airy_err_t airy_svc_set_thread_pool(airy_svc_t service, void *pool);

/**
 * @brief Handle a service request asynchronously.
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param method [in] Method name (BORROW - valid for function scope only).
 * @param params_json [in] Request parameters JSON (BORROW - valid for function scope only).
 * @param on_complete [in] Async completion callback (BORROW - not stored by pointer, copied internally).
 * @param user_data [in] User data (BORROW - caller retains ownership, must remain valid until the callback).
 * @return Error code
 *
 * @ownership service: BORROW, method: BORROW, params_json: BORROW, on_complete: BORROW, user_data: BORROW
 */
AIRY_API int airy_svc_handle_request_async(airy_svc_t service, const char *method,
                                           const char *params_json,
                                           airy_svc_async_complete_fn on_complete, void *user_data);

/**
 * @brief Pause a service
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 * @reentrant No
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_pause(airy_svc_t service);

/**
 * @brief Resume a service
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 * @reentrant No
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_resume(airy_svc_t service);


/**
 * @brief Get the service state
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return Service state
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API airy_svc_state_t airy_svc_get_state(airy_svc_t service);

/**
 * @brief Check whether the service is ready
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return true if ready, false otherwise
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API bool airy_svc_is_ready(airy_svc_t service);

/**
 * @brief Check whether the service is running
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return true if running, false otherwise
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API bool airy_svc_is_running(airy_svc_t service);

/**
 * @brief Get the service name
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return Service name (BORROW - internal string, do not free).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW, return: BORROW
 */
AIRY_API const char *airy_svc_get_name(airy_svc_t service);

/**
 * @brief Get the service version
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return Service version (BORROW - internal string, do not free).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW, return: BORROW
 */
AIRY_API const char *airy_svc_get_version(airy_svc_t service);


/**
 * @brief Get service statistics
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param stats [out] Statistics output (BORROW - caller-owned buffer, function writes to it).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW, stats: BORROW
 */
AIRY_API airy_err_t airy_svc_get_stats(airy_svc_t service, airy_svc_stats_t *stats);

/**
 * @brief Reset service statistics
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API void airy_svc_reset_stats(airy_svc_t service);


/**
 * @brief Run a service health check
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 if healthy, non-zero if unhealthy
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_healthcheck(airy_svc_t service);


/**
 * @brief Check whether the service supports a given capability
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param capability [in] Capability flag
 * @return true if supported, false otherwise
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API bool airy_svc_has_capability(airy_svc_t service, airy_svc_capability_t capability);


/**
 * @brief Convert a service state to a string
 * @param state [in] Service state
 * @return State string (BORROW - static string, do not free).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership return: BORROW
 */
AIRY_API const char *airy_svc_state_to_string(airy_svc_state_t state);

/**
 * @brief Convert a string to a service state
 * @param str [in] State string (BORROW - not stored, copied internally).
 * @return Service state
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership str: BORROW
 */
AIRY_API airy_svc_state_t airy_svc_state_from_string(const char *str);


/**
 * @brief Register a service
 * @param service [in] Service handle (BORROW - registry stores a reference, caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_register(airy_svc_t service);

/**
 * @brief Unregister a service
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_unregister(airy_svc_t service);

/**
 * @brief Find a service by name
 * @param name [in] Service name (BORROW - not stored, copied internally).
 * @return Service handle (NULL if not found) (BORROW - belongs to the registry, do not free).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership name: BORROW, return: BORROW
 */
AIRY_API airy_svc_t airy_svc_find(const char *name);

/**
 * @brief Get the total service count
 * @return Number of services
 * @threadsafe Yes
 * @reentrant Yes
 */
AIRY_API uint32_t airy_svc_count(void);

/**
 * @brief Iterate over all services
 * @param callback [in] Callback function (BORROW - not stored, called during iteration).
 * @param user_data [in] User data (BORROW - caller retains ownership, valid during iteration).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership callback: BORROW, user_data: BORROW
 */
typedef void (*airy_svc_enum_fn)(airy_svc_t service, void *user_data);
AIRY_API void airy_svc_foreach(airy_svc_enum_fn callback, void *user_data);

/**
 * @brief Set service user data
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param user_data [in] User-data pointer (BORROW - service does not take ownership, caller manages lifecycle).
 * @return Error code
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW, user_data: BORROW
 */
AIRY_API airy_err_t airy_svc_set_user_data(airy_svc_t service, void *user_data);

/**
 * @brief Get service user data
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return User-data pointer (NULL if unset) (BORROW - belongs to the service, do not free).
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @ownership service: BORROW, return: BORROW
 */
AIRY_API void *airy_svc_get_user_data(airy_svc_t service);


#define AIRY_MAX_ENDPOINT_LEN 256
#define AIRY_MAX_SERVICE_TYPE_LEN 32
#define AIRY_MAX_TAGS_LEN 256

/**
 * @brief Service metadata structure
 *
 * Used for cross-process service registration and discovery.
 * Contains name, version, endpoint, type, tags, state, load, etc.
 */
typedef struct {
    char name[64];
    char version[32];
    char endpoint[AIRY_MAX_ENDPOINT_LEN];
    char service_type[AIRY_MAX_SERVICE_TYPE_LEN];
    char tags[AIRY_MAX_TAGS_LEN];
    airy_svc_state_t state;
    uint32_t capabilities;
    uint32_t current_load;
    uint64_t last_heartbeat;
    bool healthy;
    uint32_t instance_id;
} airy_svc_metadata_t;


/**
 * @brief Initialize the service registry client
 * @param registry_url [in] Registry URL (e.g. http:
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership registry_url: BORROW
 */
AIRY_API airy_err_t airy_cross_registry_init(const char *registry_url);

/**
 * @brief Register a service with the registry
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param metadata [in] Service metadata (BORROW - not stored, copied internally).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service: BORROW, metadata: BORROW
 */
AIRY_API airy_err_t airy_registry_register(airy_svc_t service, const airy_svc_metadata_t *metadata);

/**
 * @brief Deregister a service from the registry
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_registry_deregister(airy_svc_t service);

/**
 * @brief Discover services from the registry
 * @param service_type [in] Service type (e.g. "llm", "tool"), NULL for all types (BORROW - not stored, copied internally).
 * @param filter_tags [in] Filter tags (comma-separated), NULL for no filtering (BORROW - not stored, copied internally).
 * @param result_count [out] Number of discovered services (BORROW - caller-owned buffer, function writes to it).
 * @return Service metadata array (OWNER - caller must call airy_registry_discover_free).
 * @threadsafe Yes
 *
 * @ownership service_type: BORROW, filter_tags: BORROW, result_count: BORROW, return: OWNER
 */
AIRY_API airy_svc_metadata_t *airy_registry_discover(const char *service_type,
                                                     const char *filter_tags, size_t *result_count);

/**
 * @brief Free service discovery results
 * @param results [in] Service metadata array (TRANSFER - function takes ownership and frees).
 *
 * @ownership results: TRANSFER
 */
AIRY_API void airy_registry_discover_free(airy_svc_metadata_t *results);

/**
 * @brief Send a heartbeat to the registry
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_registry_heartbeat(airy_svc_t service);

/**
 * @brief Clean up registry client resources
 */
AIRY_API void airy_registry_cleanup(void);


#define AIRY_CONFIG_CHECKSUM_LEN 65

/**
 * @brief Configuration data structure
 */
typedef struct {
    char *raw_config;
    size_t config_size;
    uint64_t version;
    time_t last_modified;
    char checksum[AIRY_CONFIG_CHECKSUM_LEN];
} airy_config_t;

/**
 * @brief Configuration change callback function type
 * @param service_name [in] Service name (BORROW - valid for callback scope only).
 * @param old_config [in] Old configuration (BORROW - valid for callback scope only, do not free).
 * @param new_config [in] New configuration (BORROW - valid for callback scope only, do not free).
 * @param user_data [in] User data (BORROW - caller retains ownership).
 *
 * @ownership service_name: BORROW, old_config: BORROW, new_config: BORROW, user_data: BORROW
 */
typedef void (*airy_config_change_callback_t)(const char *service_name,
                                              const airy_config_t *old_config,
                                              const airy_config_t *new_config, void *user_data);

/**
 * @brief Load service configuration
 * @param service_name [in] Service name (BORROW - not stored, copied internally).
 * @param config [out] Configuration output (OWNER - caller must call airy_config_free).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service_name: BORROW, config: OWNER
 */
AIRY_API airy_err_t airy_config_load(const char *service_name, airy_config_t **config);

/**
 * @brief Watch for configuration changes
 * @param service_name [in] Service name (BORROW - not stored, copied internally).
 * @param callback [in] Change callback (BORROW - stored by reference, must remain valid until unwatched).
 * @param user_data [in] User data (BORROW - caller retains ownership, must remain valid until unwatched).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service_name: BORROW, callback: BORROW, user_data: BORROW
 */
AIRY_API airy_err_t airy_config_watch(const char *service_name,
                                      airy_config_change_callback_t callback, void *user_data);

/**
 * @brief Cancel configuration watching
 * @param service_name [in] Service name (BORROW - not stored, copied internally).
 * @param callback [in] Callback to remove, NULL removes all (BORROW - used for identification only, not stored).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service_name: BORROW, callback: BORROW
 */
AIRY_API airy_err_t airy_config_unwatch(const char *service_name,
                                        airy_config_change_callback_t callback);

/**
 * @brief Free configuration resources
 * @param config [in] Configuration pointer (TRANSFER - function takes ownership and frees).
 *
 * @ownership config: TRANSFER
 */
AIRY_API void airy_config_free(airy_config_t *config);


/**
 * @brief Monitoring configuration structure
 */
typedef struct {
    uint32_t healthcheck_interval_ms;
    uint32_t max_restart_attempts;
    uint32_t restart_backoff_base_ms;
    uint32_t restart_backoff_max_ms;
    uint32_t degradation_threshold;
    bool auto_restart;
    bool enable_degradation;
} airy_monitor_config_t;

/**
 * @brief Degradation handler function type
 * @param service [in] Service handle (BORROW - valid for callback scope only).
 * @param reason [in] Degradation reason (BORROW - valid for callback scope only).
 * @param user_data [in] User data (BORROW - caller retains ownership).
 * @return 0 if degraded successfully, non-zero on failure
 *
 * @ownership service: BORROW, reason: BORROW, user_data: BORROW
 */
typedef airy_err_t (*airy_degradation_handler_t)(airy_svc_t service, const char *reason,
                                                 void *user_data);

/**
 * @brief Start service monitoring
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param config [in] Monitoring configuration (BORROW - not stored, copied internally).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service: BORROW, config: BORROW
 */
AIRY_API airy_err_t airy_svc_monitor_start(airy_svc_t service, const airy_monitor_config_t *config);

/**
 * @brief Stop service monitoring
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service: BORROW
 */
AIRY_API airy_err_t airy_svc_monitor_stop(airy_svc_t service);

/**
 * @brief Set the service degradation handler
 * @param service [in] Service handle (BORROW - caller retains ownership).
 * @param handler [in] Degradation handler (BORROW - stored by reference, must remain valid).
 * @param user_data [in] User data (BORROW - caller retains ownership, must remain valid).
 * @return 0 on success, non-zero on failure
 * @threadsafe Yes
 *
 * @ownership service: BORROW, handler: BORROW, user_data: BORROW
 */
AIRY_API airy_err_t airy_svc_set_degradation_handler(airy_svc_t service,
                                                     airy_degradation_handler_t handler,
                                                     void *user_data);


/**
 * @brief Streaming callback function type
 * @param data [in] Data chunk (BORROW - valid for callback scope only).
 * @param data_size [in] Data size
 * @param user_data [in] User data (BORROW - caller retains ownership).
 * @return 0 to continue, non-zero to interrupt
 *
 * @ownership data: BORROW, user_data: BORROW
 */
typedef int (*airy_stream_callback_t)(const char *data, size_t data_size, void *user_data);

/**
 * @brief Communication protocol type (daemon service layer only)
 * @note Uses the SVC_ prefix to avoid conflicts with AIRY_PROTO_* in
 *       commons/types.h
 */
typedef enum {
    SVC_PROTO_HTTP = 0,
    SVC_PROTO_GRPC,
    SVC_PROTO_IPC,
    SVC_PROTO_MEMORY
} airy_svc_protocol_type_t;

/**
 * @brief Service communication client interface
 *
 * @ownership call: service_name BORROW, method BORROW, params_json BORROW, response_json OWNER (caller must free).
 * @ownership stream: service_name BORROW, method BORROW, params_json BORROW, callback BORROW, user_data BORROW.
 */
typedef struct {
    airy_err_t (*call)(const char *service_name, const char *method, const char *params_json,
                       char **response_json, uint32_t timeout_ms);
    airy_err_t (*stream)(const char *service_name, const char *method, const char *params_json,
                         airy_stream_callback_t callback, void *user_data);
    void *internal;
} airy_svc_client_t;

/**
 * @brief Create a service communication client
 * @param protocol [in] Communication protocol
 * @param config [in] Client configuration (JSON string), NULL for defaults (BORROW - not stored, copied internally).
 * @param client [out] Client output (OWNER - caller must call airy_svc_client_destroy).
 * @return 0 on success, non-zero on failure
 *
 * @ownership config: BORROW, client: OWNER
 */
AIRY_API airy_err_t airy_svc_client_create(airy_svc_protocol_type_t protocol, const char *config,
                                           airy_svc_client_t **client);

/**
 * @brief Destroy a service communication client
 * @param client [in] Client pointer (TRANSFER - function takes ownership and frees).
 *
 * @ownership client: TRANSFER
 */
AIRY_API void airy_svc_client_destroy(airy_svc_client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SVC_COMMON_H */
