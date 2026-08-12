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
 * implement IPC Bus auto-registration, message routing, and transparent
 * protocol forwarding.
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
 *   // 4. Send messages to other daemons (P1.8.3)
 *   ipc_bus_helper_send(ibh, "tool_d", payload, len);
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
#include "ipc_backpressure.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief IPC Bus helper handle
 *
 * Encapsulates an ipc_service_bus instance, channels, endpoints, and
 * protocol routing.
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
 * @brief Register an IPC Bus endpoint (so other daemons can discover it)
 *
 * @param ibh          Helper handle
 * @param service_name Service name
 * @param endpoint     Endpoint address (e.g. "127.0.0.1:8080")
 * @param protocols    Supported protocol list
 * @param proto_count  Number of protocols
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_register_endpoint(ipc_bus_helper_t *ibh, const char *service_name,
                                     const char *endpoint, const ipc_bus_proto_t *protocols,
                                     uint32_t proto_count);


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
 * @brief Register an event handler
 *
 * Listens for IPC Bus events (e.g. endpoint online/offline).
 *
 * @param ibh        Helper handle
 * @param event_name Event name
 * @param handler    Event handler
 * @param user_data  User data
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_register_event_handler(ipc_bus_helper_t *ibh, const char *event_name,
                                          ipc_bus_event_handler_t handler, void *user_data);


/**
 * @brief Send a message to a target daemon
 *
 * Convenience wrapper around ipc_service_bus_send that creates and sends
 * the message automatically.
 *
 * @param ibh            Helper handle
 * @param target_service Target service name
 * @param msg_type       Message type
 * @param protocol       Protocol type
 * @param payload        Message payload
 * @param payload_size   Payload size
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_send(ipc_bus_helper_t *ibh, const char *target_service,
                        ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol, const void *payload,
                        size_t payload_size);

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

/**
 * @brief Broadcast a message to all daemons
 *
 * @param ibh     Helper handle
 * @param message Broadcast message
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_broadcast(ipc_bus_helper_t *ibh, const ipc_bus_message_t *message);

/**
 * @brief Send a notification (fire-and-forget)
 *
 * @param ibh            Helper handle
 * @param target_service Target service name
 * @param payload        Notification payload
 * @param payload_size   Payload size
 * @param protocol       Protocol type
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_notify(ipc_bus_helper_t *ibh, const char *target_service, const void *payload,
                          size_t payload_size, ipc_bus_proto_t protocol);


/**
 * @brief Auto-select the protocol and route a message
 *
 * Picks the best protocol supported by the target service, enabling
 * transparent JSON-RPC/MCP/A2A routing. If the target supports multiple
 * protocols, priority is: JSON-RPC > MCP > A2A > OpenAI.
 *
 * @param ibh            Helper handle
 * @param target_service Target service name
 * @param payload        Message payload
 * @param payload_size   Payload size
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_route_auto(ipc_bus_helper_t *ibh, const char *target_service,
                              const void *payload, size_t payload_size);

/**
 * @brief Discover service endpoints supporting a specific protocol
 *
 * @param ibh          Helper handle
 * @param service_name Service name (NULL for all services)
 * @param protocol     Protocol type
 * @param endpoints    Output endpoint array
 * @param max_count    Maximum count
 * @param found_count  Actual count found
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_discover(ipc_bus_helper_t *ibh, const char *service_name,
                            ipc_bus_proto_t protocol, ipc_bus_endpoint_t *endpoints,
                            uint32_t max_count, uint32_t *found_count);


/**
 * @brief Get the underlying ipc_service_bus handle
 *
 * @param ibh Helper handle
 * @return ipc_service_bus handle
 */
ipc_service_bus_t ipc_bus_helper_get_bus(ipc_bus_helper_t *ibh);

/**
 * @brief Check whether the IPC Bus is running
 *
 * @param ibh Helper handle
 * @return true if running
 */
bool ipc_bus_helper_is_running(ipc_bus_helper_t *ibh);


/**
 * @brief P1.24: Enable backpressure control for the IPC Bus helper
 *
 * Creates a backpressure controller and attaches it to the helper. Once
 * enabled, send/notify calls automatically check the backpressure level,
 * dropping droppable messages or rejecting sends.
 *
 * @param ibh    Helper handle
 * @param config Backpressure configuration (NULL for defaults)
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_enable_backpressure(ipc_bus_helper_t *ibh, const ipc_bp_config_t *config);

/**
 * @brief P1.24: Update the queue depth (called periodically by the daemon)
 *
 * The daemon should call this every 5s with the current message queue
 * depth. The backpressure controller adjusts its level accordingly.
 *
 * @param ibh           Helper handle
 * @param current_depth Current queue depth (message count)
 * @return Current backpressure level
 */
ipc_bp_level_t ipc_bus_helper_update_backpressure(ipc_bus_helper_t *ibh, size_t current_depth);

/**
 * @brief P1.24: Send a message with automatic backpressure checks
 *
 * Replacement for ipc_bus_helper_send: checks the backpressure level
 * before sending. If the level is DROP and the message is droppable, the
 * message is dropped. If the level is REJECT and the message is not
 * critical, the send is rejected.
 *
 * @param ibh          Helper handle
 * @param target       Target service
 * @param msg_type     Message type
 * @param protocol     Protocol
 * @param payload      Payload
 * @param payload_size Payload size
 * @param is_droppable Whether the message is droppable (logs/metrics and
 *                     other low-priority messages)
 * @return 0 on success, -1 on failure, 1 if dropped by backpressure
 */
int ipc_bus_helper_send_with_bp(ipc_bus_helper_t *ibh, const char *target,
                                ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol,
                                const void *payload, size_t payload_size, bool is_droppable);

/**
 * @brief P1.24: Check whether new connections should be accepted
 *
 * @param ibh Helper handle
 * @return true to accept, false to reject (backpressure REJECT level)
 */
bool ipc_bus_helper_should_accept_connection(ipc_bus_helper_t *ibh);

/**
 * @brief P1.24: Get backpressure statistics
 *
 * @param ibh       Helper handle
 * @param out_stats Statistics output
 * @return 0 on success, non-zero on failure (backpressure not enabled or
 *         invalid parameters)
 */
int ipc_bus_helper_get_bp_stats(ipc_bus_helper_t *ibh, ipc_bp_stats_t *out_stats);

/**
 * @brief P1.24: Get the current backpressure level
 *
 * @param ibh Helper handle
 * @return Backpressure level (IPC_BP_NORMAL if backpressure is not enabled)
 */
ipc_bp_level_t ipc_bus_helper_get_bp_level(ipc_bus_helper_t *ibh);


/**
 * @brief P1.8: Get IPC Bus routing statistics
 *
 * Returns cumulative message-routing statistics: total sends, auto-routes,
 * route fallbacks, failures, and backpressure drops/rejects. All output
 * parameters may be NULL (skipping the corresponding statistic).
 *
 * @param ibh                  Helper handle
 * @param out_total_sends      Total send count (may be NULL)
 * @param out_total_routes     Total auto-route count (may be NULL)
 * @param out_route_fallbacks  Route fallback count (may be NULL)
 * @param out_send_failures    Send failure count (may be NULL)
 * @param out_bp_drops         Backpressure drop count (may be NULL)
 * @param out_bp_rejects       Backpressure reject count (may be NULL)
 * @return 0 on success, non-zero on failure
 */
int ipc_bus_helper_get_routing_stats(ipc_bus_helper_t *ibh, uint64_t *out_total_sends,
                                     uint64_t *out_total_routes, uint64_t *out_route_fallbacks,
                                     uint64_t *out_send_failures, uint64_t *out_bp_drops,
                                     uint64_t *out_bp_rejects);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_BUS_HELPER_H */
