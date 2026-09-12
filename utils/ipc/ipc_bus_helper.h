/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file ipc_bus_helper.h
 * @brief C-L09: IPC Bus -> daemon auto-registration convenience layer
 *        (authoritative commons version).
 *
 * P0.17 phase 4: migrated from daemons/common/include/ into commons,
 * removing the atoms->daemons compile-time reverse dependency (IRON-6).
 * The daemons copy is kept as a re-exporting compatibility header.
 *
 * Each daemon calls the convenience APIs of this module at startup to
 * bring up its bus instance and register its channel and handler.
 *
 * 8.3.4 (0.1.15): the send/broadcast/notify/route/discover/backpressure
 * wrappers were removed together with the never-delivering bus family
 * they forwarded to; the helper is registration + request only.
 *
 * Typical usage (in a daemon main()):
 * @code
 *   // 1. Initialize the IPC Bus helper
 *   ipc_bus_helper_t *ibh = ipc_bus_helper_init("llm_d", NULL);
 *
 *   // 2. Register the IPC channel (P1.8.1)
 *   ipc_bus_helper_register_channel(ibh, "llm", IPC_BUS_PROTO_JSON_RPC);
 *
 *   // 3. Register a message handler (P1.8.2)
 *   ipc_bus_helper_register_handler(ibh, my_handler, NULL);
 *
 *   // 4. Send a request to another daemon (P1.8.3)
 *   ipc_bus_helper_request(ibh, "tool_d", &req, &resp, 5000);
 *
 *   // ... daemon main loop ...
 *
 *   // 5. Shut down
 *   ipc_bus_helper_shutdown(ibh);
 * @endcode
 *
 * @see ipc_service_bus.h
 * @see P1.8 C-L09 wiring
 */

#ifndef AIRY_RT_IPC_BUS_HELPER_H
#define AIRY_RT_IPC_BUS_HELPER_H

#include "ipc_service_bus.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief IPC Bus helper handle
 *
 * Encapsulates an ipc_service_bus instance and its channel.
 */
typedef struct ipc_bus_helper_s ipc_bus_helper_t;


/**
 * @brief Initialize the IPC Bus helper
 *
 * Creates and starts an ipc_service_bus instance.
 *
 * @param daemon_name Daemon name (used as the bus name)
 * @param config      Channel configuration (NULL for defaults)
 * @return Helper handle, NULL on failure
 */
ipc_bus_helper_t *ipc_bus_helper_init(const char *daemon_name,
                                      const ipc_bus_channel_config_t *config);

/**
 * @brief Shut down the IPC Bus helper
 *
 * Automatically unregisters channels and releases resources.
 *
 * @param ibh Helper handle
 */
void ipc_bus_helper_shutdown(ipc_bus_helper_t *ibh);


/**
 * @brief Register an IPC Bus channel for the current daemon
 *
 * Creates a named communication channel through which other daemons send
 * messages.
 *
 * @param ibh             Helper handle
 * @param channel_name    Channel name (e.g. "llm")
 * @param default_protocol Default protocol
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_register_channel(ipc_bus_helper_t *ibh, const char *channel_name,
                                    ipc_bus_proto_t default_protocol);


/**
 * @brief Register a message handler
 *
 * Invoked when other daemons send messages to this daemon's channel.
 *
 * @param ibh       Helper handle
 * @param handler   Message handler
 * @param user_data User data
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_register_handler(ipc_bus_helper_t *ibh, ipc_bus_message_handler_t handler,
                                    void *user_data);

/**
 * @brief Send a request and wait for the response
 *
 * Convenience wrapper around ipc_service_bus_request.
 *
 * @param ibh            Helper handle
 * @param target_service Target service name
 * @param request        Request message
 * @param response       Response message (output)
 * @param timeout_ms     Timeout (ms), 0 for the default
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_request(ipc_bus_helper_t *ibh, const char *target_service,
                           const ipc_bus_message_t *request, ipc_bus_message_t *response,
                           uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_BUS_HELPER_H */
