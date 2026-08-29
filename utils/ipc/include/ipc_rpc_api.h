/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC RPC API: rpc server/client/method registration.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_RPC_API_H
#define AIRY_RT_IPC_RPC_API_H

#include "ipc_types.h"

/*
 * RPC channel API (remote procedure call over a transport channel)
 */

/**
 * @brief RPC method handler function type
 * @param request Request data
 * @param request_len Request length
 * @param response [out] Response buffer
 * @param response_max [in/out] Response buffer size / actual response length
 * @param user_data User data
 * @return Error code
 */
typedef airy_err_t (*rpc_method_handler_t)(const void *request, size_t request_len, void *response,
                                           size_t *response_max, void *user_data);

/**
 * @brief RPC server handle
 */
typedef struct ipc_rpc_server ipc_rpc_server_t;

/**
 * @brief RPC client handle
 */
typedef struct ipc_rpc_client ipc_rpc_client_t;

/**
 * @brief RPC method registration information
 */
typedef struct {
    const char *method_name;
    rpc_method_handler_t handler;
    void *user_data;
} ipc_rpc_method_t;

/**
 * @brief RPC server configuration
 */
typedef struct {
    ipc_channel_t *transport;
    const char *service_name;
    ipc_rpc_method_t *methods;
    size_t method_count;
    size_t max_request_size;
    size_t max_response_size;
} ipc_rpc_server_config_t;

/**
 * @brief Create an RPC server
 * @param config Server configuration
 * @return RPC server handle, NULL on failure
 */
ipc_rpc_server_t *ipc_rpc_server_create(const ipc_rpc_server_config_t *config);

/**
 * @brief Destroy an RPC server
 * @param server RPC server handle
 */
void ipc_rpc_server_destroy(ipc_rpc_server_t *server);

/**
 * @brief Start an RPC server (begins processing requests)
 * @param server RPC server handle
 * @return Error code
 */
airy_err_t ipc_rpc_server_start(ipc_rpc_server_t *server);

/**
 * @brief Stop an RPC server
 * @param server RPC server handle
 * @return Error code
 */
airy_err_t ipc_rpc_server_stop(ipc_rpc_server_t *server);

/**
 * @brief Process a single RPC request (called by the event loop)
 * @param server RPC server handle
 * @param timeout_ms Timeout
 * @return AIRY_SUCCESS on success, AIRY_ETIMEDOUT if no request, other
 *         values are errors
 */
airy_err_t ipc_rpc_server_process(ipc_rpc_server_t *server, uint32_t timeout_ms);

/**
 * @brief RPC client configuration
 */
typedef struct {
    ipc_channel_t *transport;
    uint32_t timeout_ms;
} ipc_rpc_client_config_t;

/**
 * @brief Create an RPC client
 * @param config Client configuration
 * @return RPC client handle, NULL on failure
 */
ipc_rpc_client_t *ipc_rpc_client_create(const ipc_rpc_client_config_t *config);

/**
 * @brief Destroy an RPC client
 * @param client RPC client handle
 */
void ipc_rpc_client_destroy(ipc_rpc_client_t *client);

/**
 * @brief Synchronous RPC call
 * @param client RPC client handle
 * @param method_name Method name
 * @param request Request data
 * @param request_len Request length
 * @param response [out] Response buffer (caller-allocated)
 * @param response_max [in] Maximum response buffer size
 * @param response_len [out] Actual response length
 * @return Error code
 */
airy_err_t ipc_rpc_call_sync(ipc_rpc_client_t *client, const char *method_name, const void *request,
                             size_t request_len, void *response, size_t response_max,
                             size_t *response_len);

/**
 * @brief Register a single RPC method (added at runtime on the server)
 * @param server RPC server handle
 * @param method Method registration information
 * @return Error code
 */
airy_err_t ipc_rpc_server_register_method(ipc_rpc_server_t *server, const ipc_rpc_method_t *method);

/**
 * @brief Find a registered RPC method
 * @param server RPC server handle
 * @param method_name Method name
 * @return Method handler, NULL if not found
 */
rpc_method_handler_t ipc_rpc_server_find_method(ipc_rpc_server_t *server, const char *method_name);

/**
 * @brief Get the error message
 * @param channel Channel handle
 * @return Error message string
 */
const char *ipc_get_error_message(const ipc_channel_t *channel);

#endif /* AIRY_RT_IPC_RPC_API_H */
