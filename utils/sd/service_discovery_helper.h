/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file service_discovery_helper.h
 * @brief C-L08: ServiceDiscovery -> daemon auto-registration convenience
 *        layer (authoritative commons version).
 *
 * P0.17 phase 4: migrated from daemons/common/include/ into commons,
 * removing the atoms->daemons compile-time reverse dependency (IRON-6).
 * The daemons copy is kept as a re-exporting compatibility header.
 *
 * Each daemon calls the convenience APIs of this module at startup for
 * automatic registration and heartbeat. Built on the core API of
 * service_discovery.h to simplify daemon integration.
 *
 * Typical usage (in a daemon main()):
 * @code
 *   // 1. Initialize service discovery
 *   sd_helper_t *sdh = sd_helper_init(NULL);
 *
 *   // 2. Auto-register
 *   sd_helper_register(sdh, "llm_d", "llm", "127.0.0.1", 8080,
 *                      "ai,core", 10000);
 *
 *   // 3. Start the heartbeat thread
 *   sd_helper_start_heartbeat(sdh);
 *
 *   // ... daemon main loop ...
 *
 *   // 4. Auto-deregister on shutdown
 *   sd_helper_shutdown(sdh);
 * @endcode
 *
 * @see service_discovery.h
 * @see P1.7 C-L08 wiring
 */

#ifndef AIRY_RT_SERVICE_DISCOVERY_HELPER_H
#define AIRY_RT_SERVICE_DISCOVERY_HELPER_H

#include "service_discovery.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Service discovery helper handle
 *
 * Encapsulates a service_discovery instance, the heartbeat thread, and
 * registration information.
 */
typedef struct sd_helper_s sd_helper_t;


/**
 * @brief Initialize the service discovery helper
 *
 * Creates a service_discovery instance and starts background management.
 *
 * @param config Service discovery configuration (NULL for defaults)
 * @return Helper handle, NULL on failure
 */
sd_helper_t *sd_helper_init(const sd_config_t *config);

/**
 * @brief Shut down the service discovery helper
 *
 * Automatically deregisters registered services, stops the heartbeat, and
 * releases resources.
 *
 * @param sdh Helper handle
 */
void sd_helper_shutdown(sd_helper_t *sdh);


/**
 * @brief Register the current daemon with service discovery
 *
 * Auto-generates the instance_id (based on host:port), fills sd_instance_t,
 * and calls sd_register.
 *
 * @param sdh    Helper handle
 * @param name   Service name (e.g. "llm_d")
 * @param type   Service type (e.g. "llm")
 * @param host   Listen address (e.g. "127.0.0.1")
 * @param port   Listen port (e.g. 8080)
 * @param tags   Tags (comma-separated, e.g. "ai,core"; NULL for none)
 * @param ttl_ms Heartbeat TTL (ms), 0 for the default 30000
 * @return 0 on success, non-zero on failure
 */
int sd_helper_register(sd_helper_t *sdh, const char *name, const char *type, const char *host,
                       uint16_t port, const char *tags, uint32_t ttl_ms);

/**
 * @brief Register the current daemon with service discovery (Unix socket
 *        version)
 *
 * @param sdh         Helper handle
 * @param name        Service name
 * @param type        Service type
 * @param socket_path Unix socket path
 * @param tags        Tags
 * @param ttl_ms      Heartbeat TTL
 * @return 0 on success, non-zero on failure
 */
int sd_helper_register_unix(sd_helper_t *sdh, const char *name, const char *type,
                            const char *socket_path, const char *tags, uint32_t ttl_ms);


/**
 * @brief Start the background heartbeat thread
 *
 * Sends heartbeats periodically at the configured heartbeat_interval_ms.
 * The heartbeat thread stops automatically at sd_helper_shutdown.
 *
 * @param sdh Helper handle
 * @return 0 on success, non-zero on failure
 */
int sd_helper_start_heartbeat(sd_helper_t *sdh);

/**
 * @brief Stop the heartbeat thread
 *
 * @param sdh Helper handle
 */
void sd_helper_stop_heartbeat(sd_helper_t *sdh);

/**
 * @brief Manually send one heartbeat
 *
 * @param sdh Helper handle
 * @return 0 on success, non-zero on failure
 */
int sd_helper_send_heartbeat(sd_helper_t *sdh);


/**
 * @brief Discover available service instances
 *
 * Convenience wrapper around sd_discover returning the list of available
 * instances.
 *
 * @param sdh          Helper handle
 * @param service_name Service name
 * @param instances    Output instance array
 * @param max_count    Maximum count
 * @param found_count  Actual count found
 * @return 0 on success, non-zero on failure
 */
int sd_helper_find(sd_helper_t *sdh, const char *service_name, sd_instance_t *instances,
                   uint32_t max_count, uint32_t *found_count);


/**
 * @brief Select the best service instance
 *
 * Convenience wrapper around sd_select_instance using the default
 * load-balancing strategy.
 *
 * @param sdh          Helper handle
 * @param service_name Service name
 * @param instance     Output of the selected instance
 * @return 0 on success, non-zero on failure
 */
int sd_helper_select(sd_helper_t *sdh, const char *service_name, sd_instance_t *instance);

/**
 * @brief Select the best service instance (with a strategy)
 *
 * @param sdh          Helper handle
 * @param service_name Service name
 * @param strategy     Load-balancing strategy
 * @param instance     Output of the selected instance
 * @return 0 on success, non-zero on failure
 */
int sd_helper_select_with_strategy(sd_helper_t *sdh, const char *service_name,
                                   sd_lb_strategy_t strategy, sd_instance_t *instance);


/**
 * @brief Get the underlying service_discovery handle
 *
 * For scenarios needing to call the service_discovery API directly.
 *
 * @param sdh Helper handle
 * @return service_discovery handle
 */
service_discovery_t sd_helper_get_sd(sd_helper_t *sdh);

/**
 * @brief Check whether service discovery is running
 *
 * @param sdh Helper handle
 * @return true if running
 */
bool sd_helper_is_running(sd_helper_t *sdh);

/**
 * @brief Get the number of registered services
 *
 * @param sdh Helper handle
 * @return Number of services
 */
uint32_t sd_helper_service_count(sd_helper_t *sdh);

/**
 * @brief C-L08: Output a service discovery statistics summary (single-line
 *        format, suitable for periodic logging)
 *
 * @param sdh Helper handle
 */
void sd_helper_dump_stats(sd_helper_t *sdh);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SERVICE_DISCOVERY_HELPER_H */
