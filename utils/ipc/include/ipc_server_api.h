/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC server API: create/destroy/start/stop/accept/broadcast.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_SERVER_API_H
#define AIRY_RT_IPC_SERVER_API_H

#include "ipc_types.h"


/**
 * @brief Create an IPC server
 * @param config Server configuration
 * @return Server handle, NULL on failure
 */
ipc_server_t *ipc_server_create(const ipc_config_t *config);

/**
 * @brief Destroy an IPC server
 * @param server Server handle
 */
void ipc_server_destroy(ipc_server_t *server);

/**
 * @brief Start an IPC server
 * @param server Server handle
 * @return Error code
 */
airy_err_t ipc_server_start(ipc_server_t *server);

/**
 * @brief Stop an IPC server
 * @param server Server handle
 * @return Error code
 */
airy_err_t ipc_server_stop(ipc_server_t *server);

/**
 * @brief Accept a client connection
 * @param server Server handle
 * @param timeout_ms Timeout in milliseconds
 * @return Client channel handle, NULL on failure
 * @ownership Server owns the connection; release with ipc_server_disconnect
 */
ipc_channel_t *ipc_server_accept(ipc_server_t *server, uint32_t timeout_ms);

/**
 * @brief Disconnect and release an accepted client connection
 *
 * Removes the connection from the server's tracking array and destroys
 * the channel. Must be used instead of ipc_channel_destroy for
 * connections obtained via ipc_server_accept, so that the server does
 * not retain a dangling reference.
 *
 * @param server Server handle
 * @param channel Connection channel returned by ipc_server_accept
 * @return Error code
 */
airy_err_t ipc_server_disconnect(ipc_server_t *server, ipc_channel_t *channel);

/**
 * @brief Get the server connection count
 * @param server Server handle
 * @return Current connection count
 */
size_t ipc_server_connection_count(const ipc_server_t *server);

/**
 * @brief Broadcast a message to all clients
 * @param server Server handle
 * @param message Message structure
 * @return Error code
 */
airy_err_t ipc_server_broadcast(ipc_server_t *server, const ipc_message_t *message);

#endif /* AIRY_RT_IPC_SERVER_API_H */
