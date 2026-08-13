/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file daemon_bootstrap_sd.h
 * @brief P1.7 C-L08: one-shot daemon ServiceDiscovery bootstrap module.
 *
 * P0.17 phase 4: migrated from daemons/common/include/ to commons,
 * removing the compile-time reverse dependency atoms->daemons (IRON-6).
 * The daemons version is kept as a re-export compat header.
 *
 * A daemon calls this module at startup and it automatically performs:
 * 1. ServiceDiscovery initialization
 * 2. Service registration (name/type/host/port/tags/ttl)
 * 3. Heartbeat thread startup
 * 4. Automatic unregistration on shutdown
 *
 * Typical usage (daemon main()):
 * @code
 *   #include "daemon_bootstrap_sd.h"
 *
 *   // 1. Bootstrap service discovery
 *   daemon_bootstrap_sd_t *bsd = daemon_bootstrap_sd_start(
 *       "llm_d", "llm", "127.0.0.1", 8080, "ai,core", 0);
 *
 *   // ... daemon main loop ...
 *
 *   // 2. Auto-unregister on shutdown
 *   daemon_bootstrap_sd_stop(bsd);
 * @endcode
 *
 * @see service_discovery_helper.h
 * @see P1.7 C-L08 wiring
 */

#ifndef AIRY_RT_DAEMON_BOOTSTRAP_SD_H
#define AIRY_RT_DAEMON_BOOTSTRAP_SD_H

#include "service_discovery_helper.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct daemon_bootstrap_sd_s daemon_bootstrap_sd_t;


/**
 * @brief One-shot bootstrap: init SD + register service + start heartbeat.
 *
 * @param name        Service name (e.g. "llm_d")
 * @param type        Service type (e.g. "llm")
 * @param host        Listen address (e.g. "127.0.0.1"), NULL uses Unix socket
 * @param port        Listen port, 0 uses Unix socket
 * @param tags        Tags (comma-separated, e.g. "ai,core", NULL means none)
 * @param ttl_ms      Heartbeat TTL (ms), 0 uses the 30000 default
 * @return Bootstrap handle, NULL on failure
 */
daemon_bootstrap_sd_t *daemon_bootstrap_sd_start(const char *name, const char *type,
                                                 const char *host, uint16_t port, const char *tags,
                                                 uint32_t ttl_ms);

/**
 * @brief One-shot bootstrap (Unix socket variant).
 *
 * @param name        Service name
 * @param type        Service type
 * @param socket_path Unix socket path
 * @param tags        Tags
 * @param ttl_ms      Heartbeat TTL
 * @return Bootstrap handle, NULL on failure
 */
daemon_bootstrap_sd_t *daemon_bootstrap_sd_start_unix(const char *name, const char *type,
                                                      const char *socket_path, const char *tags,
                                                      uint32_t ttl_ms);

/**
 * @brief Stop the service discovery bootstrap (unregister + stop heartbeat
 * + release resources).
 *
 * @param bsd Bootstrap handle
 */
void daemon_bootstrap_sd_stop(daemon_bootstrap_sd_t *bsd);


/**
 * @brief Get the underlying sd_helper handle (for advanced ops such as
 * sd_helper_find/select).
 *
 * @param bsd Bootstrap handle
 * @return sd_helper handle, NULL if not bootstrapped
 */
sd_helper_t *daemon_bootstrap_sd_get_helper(daemon_bootstrap_sd_t *bsd);

/**
 * @brief Check whether the bootstrap is running.
 *
 * @param bsd Bootstrap handle
 * @return true if running
 */
bool daemon_bootstrap_sd_is_running(daemon_bootstrap_sd_t *bsd);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_DAEMON_BOOTSTRAP_SD_H */
