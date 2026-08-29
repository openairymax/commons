/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC client API: create/destroy/connect/disconnect/get_channel.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_CLIENT_API_H
#define AIRY_RT_IPC_CLIENT_API_H

#include "ipc_types.h"

/**
 * @brief Create an IPC client
 * @param config Client configuration
 * @return Client handle, NULL on failure
 */
ipc_client_t *ipc_client_create(const ipc_config_t *config);

/**
 * @brief Destroy an IPC client
 * @param client Client handle
 */
void ipc_client_destroy(ipc_client_t *client);

/**
 * @brief Connect to a server
 * @param client Client handle
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
airy_err_t ipc_client_connect(ipc_client_t *client, uint32_t timeout_ms);

/**
 * @brief Disconnect
 * @param client Client handle
 * @return Error code
 */
airy_err_t ipc_client_disconnect(ipc_client_t *client);

/**
 * @brief Get the client channel
 * @param client Client handle
 * @return Channel handle
 */
ipc_channel_t *ipc_client_get_channel(ipc_client_t *client);

#endif /* AIRY_RT_IPC_CLIENT_API_H */
