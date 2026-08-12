/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file circuit_breaker.h
 * @brief Circuit breaker and self-healing framework (authoritative commons
 *        version).
 *
 * Implements the Circuit Breaker pattern, providing:
 * - Three-state breaker: closed (normal) -> open (tripped) -> half-open
 *   (probing)
 * - Automatic failure detection and tripping
 * - Progressive recovery probing
 * - Cascading-failure protection
 * - Automatic failover
 * - Service-discovery integration
 *
 * P0.17 phase 5: migrated from daemons/common/include/circuit_breaker.h
 * into commons, removing the atoms->daemons compile-time reverse
 * dependency (IRON-6). The daemons copy is kept as a re-exporting
 * compatibility header.
 *
 * Design principles:
 * 1. Fail fast: return errors immediately in the open state, avoiding
 *    cascading blocking
 * 2. Progressive recovery: in the half-open state, gradually allow
 *    requests to verify service recovery
 * 3. Observability: state changes trigger event notifications
 * 4. Configurability: thresholds, timeouts, and probing strategies are all
 *    configurable
 *
 * @see svc_common.h service management framework
 * @see service_discovery.h service discovery
 */

#ifndef AIRY_RT_CIRCUIT_BREAKER_H
#define AIRY_RT_CIRCUIT_BREAKER_H

#include "svc_common.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define CB_MAX_BREAKERS 64
#define CB_MAX_NAME_LEN 64
#define CB_MAX_FALLBACKS 4
#define CB_DEFAULT_FAILURE_THRESHOLD 5
#define CB_DEFAULT_SUCCESS_THRESHOLD 3
#define CB_DEFAULT_TIMEOUT_MS 30000
#define CB_DEFAULT_HALF_OPEN_MAX 1


typedef enum { CB_STATE_CLOSED = 0, CB_STATE_OPEN = 1, CB_STATE_HALF_OPEN = 2 } cb_state_t;


typedef struct {
    uint32_t failure_threshold;
    uint32_t success_threshold;
    uint32_t timeout_ms;
    uint32_t half_open_max_calls;
    uint32_t window_size_ms;
    uint32_t slow_call_duration_ms;
    uint32_t slow_call_rate_threshold;
    uint32_t failure_rate_threshold;
    bool enable_slow_call_detection;
    bool enable_auto_failover;
} cb_config_t;


typedef struct {
    uint64_t total_calls;
    uint64_t successful_calls;
    uint64_t failed_calls;
    uint64_t rejected_calls;
    uint64_t timeout_calls;
    uint64_t slow_calls;
    uint64_t state_transitions;
    uint64_t last_failure_time;
    uint64_t last_success_time;
    uint64_t last_state_change_time;
    double failure_rate;
    double slow_call_rate;
    uint32_t consecutive_failures;
    uint32_t consecutive_successes;
} cb_stats_t;


typedef enum {
    CB_EVENT_STATE_CHANGE = 1,
    CB_EVENT_FAILURE = 2,
    CB_EVENT_SUCCESS = 3,
    CB_EVENT_REJECTED = 4,
    CB_EVENT_SLOW_CALL = 5,
    CB_EVENT_TIMEOUT = 6,
    CB_EVENT_FAILOVER = 7
} cb_event_type_t;

typedef struct {
    cb_event_type_t type;
    char breaker_name[CB_MAX_NAME_LEN];
    cb_state_t old_state;
    cb_state_t new_state;
    const char *message;
    uint64_t timestamp;
} cb_event_t;

typedef void (*cb_event_callback_t)(const cb_event_t *event, void *user_data);


typedef enum {
    CB_FAILOVER_RETRY = 0,
    CB_FAILOVER_FALLBACK = 1,
    CB_FAILOVER_REDIRECT = 2,
    CB_FAILOVER_CACHE = 3
} cb_failover_strategy_t;

typedef struct {
    cb_failover_strategy_t strategy;
    char fallback_service[CB_MAX_NAME_LEN];
    uint32_t max_retries;
    uint32_t retry_delay_ms;
    uint32_t retry_backoff_factor;
} cb_failover_config_t;


typedef struct circuit_breaker_s *circuit_breaker_t;


typedef struct cb_manager_s *cb_manager_t;


/**
 * @brief Create a circuit breaker manager
 * @return Manager handle, NULL on failure
 */
AIRY_API cb_manager_t cb_manager_create(void);

/**
 * @brief Destroy a circuit breaker manager
 * @param manager Manager handle
 */
AIRY_API void cb_manager_destroy(cb_manager_t manager);


/**
 * @brief Create a circuit breaker
 * @param manager Manager handle
 * @param name Breaker name (usually the service name)
 * @param config Configuration (NULL for defaults)
 * @return Breaker handle, NULL on failure
 */
AIRY_API circuit_breaker_t cb_create(cb_manager_t manager, const char *name,
                                     const cb_config_t *config);

/**
 * @brief Destroy a circuit breaker
 * @param breaker Breaker handle
 */
AIRY_API void cb_destroy(circuit_breaker_t breaker);

/**
 * @brief Check whether a request is allowed through
 * @param breaker Breaker handle
 * @return true to allow, false to reject
 */
AIRY_API bool cb_allow_request(circuit_breaker_t breaker);

/**
 * @brief Record a successful call
 * @param breaker Breaker handle
 * @param duration_ms Call duration
 */
AIRY_API void cb_record_success(circuit_breaker_t breaker, uint32_t duration_ms);

/**
 * @brief Record a failed call
 * @param breaker Breaker handle
 * @param error_code Error code
 */
AIRY_API void cb_record_failure(circuit_breaker_t breaker, int32_t error_code);

/**
 * @brief Record a timed-out call
 * @param breaker Breaker handle
 */
AIRY_API void cb_record_timeout(circuit_breaker_t breaker);


/**
 * @brief Get the breaker's current state
 * @param breaker Breaker handle
 * @return Breaker state
 */
AIRY_API cb_state_t cb_get_state(circuit_breaker_t breaker);

/**
 * @brief Get the breaker name
 * @param breaker Breaker handle
 * @return Name
 */
AIRY_API const char *cb_get_name(circuit_breaker_t breaker);

/**
 * @brief Get breaker statistics
 * @param breaker Breaker handle
 * @param stats [out] Statistics
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t cb_get_stats(circuit_breaker_t breaker, cb_stats_t *stats);

/**
 * @brief Reset the breaker to the closed state
 * @param breaker Breaker handle
 */
AIRY_API void cb_reset(circuit_breaker_t breaker);

/**
 * @brief Force-open the breaker
 * @param breaker Breaker handle
 */
AIRY_API void cb_force_open(circuit_breaker_t breaker);

/**
 * @brief Force-close the breaker
 * @param breaker Breaker handle
 */
AIRY_API void cb_force_close(circuit_breaker_t breaker);


/**
 * @brief Configure the failover strategy
 * @param breaker Breaker handle
 * @param config Failover configuration
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t cb_set_failover_config(circuit_breaker_t breaker,
                                           const cb_failover_config_t *config);

/**
 * @brief Execute failover
 * @param breaker Breaker handle
 * @param original_error Original error code
 * @param fallback_result [out] Failover result
 * @param result_size Result buffer size
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t cb_execute_failover(circuit_breaker_t breaker, int32_t original_error,
                                        char *fallback_result, size_t result_size);


/**
 * @brief Register a breaker event callback
 * @param manager Manager handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t cb_register_event_callback(cb_manager_t manager, cb_event_callback_t callback,
                                               void *user_data);

/**
 * @brief Find a circuit breaker
 * @param manager Manager handle
 * @param name Breaker name
 * @return Breaker handle, NULL if not found
 */
AIRY_API circuit_breaker_t cb_find(cb_manager_t manager, const char *name);

/**
 * @brief Get the total breaker count
 * @param manager Manager handle
 * @return Number of breakers
 */
AIRY_API uint32_t cb_count(cb_manager_t manager);


/**
 * @brief Convert a breaker state to a string
 * @param state State
 * @return State name
 */
AIRY_API const char *cb_state_to_string(cb_state_t state);

/**
 * @brief Create the default configuration
 * @return Default configuration
 */
AIRY_API cb_config_t cb_create_default_config(void);

/**
 * @brief Create the default failover configuration
 * @return Default failover configuration
 */
AIRY_API cb_failover_config_t cb_create_default_failover_config(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CIRCUIT_BREAKER_H */
