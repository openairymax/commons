/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file daemon_bootstrap_ipc.h
 * @brief P1.8 C-L09: one-shot daemon IPC Bus bootstrap module.
 *
 * P0.17 phase 4: migrated from daemons/common/include/ to commons,
 * removing the compile-time reverse dependency atoms->daemons (IRON-6).
 * The daemons version is kept as a re-export compat header.
 *
 * A daemon calls this module at startup and it automatically performs:
 * 1. IPC Bus initialization
 * 2. Channel registration
 * 3. Endpoint registration (so other daemons can discover it)
 * 4. Default message handler registration
 * 5. Automatic protocol routing
 * 6. Automatic unregistration on shutdown
 *
 * Typical usage (daemon main()):
 * @code
 *   #include "daemon_bootstrap_ipc.h"
 *
 *   // 1. Bootstrap the IPC Bus
 *   daemon_bootstrap_ipc_t *bipc = daemon_bootstrap_ipc_start(
 *       "llm_d", "llm", "127.0.0.1", 8080, IPC_BUS_PROTO_JSON_RPC);
 *
 *   // 2. Register a custom message handler (optional)
 *   daemon_bootstrap_ipc_register_handler(bipc, my_handler, NULL);
 *
 *   // ... daemon main loop ...
 *
 *   daemon_bootstrap_ipc_stop(bipc);
 * @endcode
 *
 * @see ipc_bus_helper.h
 * @see P1.8 C-L09 wiring
 */

#ifndef AIRY_RT_DAEMON_BOOTSTRAP_IPC_H
#define AIRY_RT_DAEMON_BOOTSTRAP_IPC_H

#include "ipc_bus_helper.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct daemon_bootstrap_ipc_s daemon_bootstrap_ipc_t;


/**
 * @brief One-shot bootstrap: init IPC Bus + register channel + endpoint.
 *
 * @param daemon_name    daemon name (e.g. "llm_d")
 * @param channel_name   channel name (e.g. "llm")
 * @param host           listen address (e.g. "127.0.0.1"), NULL uses Unix socket
 * @param port           listen port, 0 uses Unix socket
 * @param protocol       default protocol
 * @return Bootstrap handle, NULL on failure
 */
daemon_bootstrap_ipc_t *daemon_bootstrap_ipc_start(const char *daemon_name,
                                                   const char *channel_name, const char *host,
                                                   uint16_t port, ipc_bus_proto_t protocol);

/**
 * @brief One-shot bootstrap (Unix socket variant).
 *
 * @param daemon_name   daemon name
 * @param channel_name  channel name
 * @param socket_path   Unix socket path
 * @param protocol      default protocol
 * @return Bootstrap handle, NULL on failure
 */
daemon_bootstrap_ipc_t *daemon_bootstrap_ipc_start_unix(const char *daemon_name,
                                                        const char *channel_name,
                                                        const char *socket_path,
                                                        ipc_bus_proto_t protocol);

/**
 * @brief Stop the IPC Bus bootstrap.
 *
 * @param bipc Bootstrap handle
 */
void daemon_bootstrap_ipc_stop(daemon_bootstrap_ipc_t *bipc);


/**
 * @brief Register a custom message handler.
 *
 * @param bipc     Bootstrap handle
 * @param handler  Message handler
 * @param user_data User data
 * @return 0 on success, nonzero on failure
 */
int daemon_bootstrap_ipc_register_handler(daemon_bootstrap_ipc_t *bipc,
                                          ipc_bus_message_handler_t handler, void *user_data);


/**
 * @brief Convenience send method (auto routing).
 *
 * @param bipc           Bootstrap handle
 * @param target_service Target service name
 * @param payload        Message payload
 * @param payload_size   Payload size
 * @return 0 on success, nonzero on failure
 */
int daemon_bootstrap_ipc_send(daemon_bootstrap_ipc_t *bipc, const char *target_service,
                              const void *payload, size_t payload_size);


/**
 * @brief Get the underlying ipc_bus_helper handle.
 *
 * @param bipc Bootstrap handle
 * @return ipc_bus_helper handle
 */
ipc_bus_helper_t *daemon_bootstrap_ipc_get_helper(daemon_bootstrap_ipc_t *bipc);

/**
 * @brief Check whether the IPC Bus is running.
 *
 * @param bipc Bootstrap handle
 * @return true if running
 */
bool daemon_bootstrap_ipc_is_running(daemon_bootstrap_ipc_t *bipc);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_DAEMON_BOOTSTRAP_IPC_H */
