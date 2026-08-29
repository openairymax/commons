/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * Core IPC API: init/cleanup/config/channel/send/receive/notify/broadcast.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_CORE_API_H
#define AIRY_RT_IPC_CORE_API_H

#include "ipc_types.h"

/**
 * @brief Initialize the IPC subsystem
 * @return Error code
 */
airy_err_t ipc_init(void);

/**
 * @brief Clean up the IPC subsystem
 */
void ipc_cleanup(void);

/**
 * @brief Create a default IPC configuration
 * @param type Channel type
 * @return Default configuration structure
 */
ipc_config_t ipc_create_default_config(ipc_type_t type);

/**
 * @brief Create an IPC channel
 * @param config Channel configuration
 * @return Channel handle, NULL on failure
 * @ownership Caller releases with ipc_channel_destroy
 */
ipc_channel_t *ipc_channel_create(const ipc_config_t *config);

/**
 * @brief Destroy an IPC channel
 * @param channel Channel handle
 */
void ipc_channel_destroy(ipc_channel_t *channel);

/**
 * @brief Open an IPC channel
 * @param channel Channel handle
 * @return Error code
 */
airy_err_t ipc_channel_open(ipc_channel_t *channel);

/**
 * @brief Close an IPC channel
 * @param channel Channel handle
 * @return Error code
 */
airy_err_t ipc_channel_close(ipc_channel_t *channel);

/**
 * @brief Get the channel state
 * @param channel Channel handle
 * @return Channel state
 */
ipc_state_t ipc_channel_get_state(const ipc_channel_t *channel);

/**
 * @brief Get the channel name
 * @param channel Channel handle
 * @return Channel name
 */
const char *ipc_channel_get_name(const ipc_channel_t *channel);

/**
 * @brief Get the channel type
 * @param channel Channel handle
 * @return Channel type
 */
ipc_type_t ipc_channel_get_type(const ipc_channel_t *channel);

/**
 * @brief Set the channel timeout
 * @param channel Channel handle
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
airy_err_t ipc_channel_set_timeout(ipc_channel_t *channel, uint32_t timeout_ms);

/**
 * @brief Set the event callback
 * @param channel Channel handle
 * @param callback Callback function
 * @param user_data User data
 * @return Error code
 */
airy_err_t ipc_channel_set_event_callback(ipc_channel_t *channel, ipc_event_callback_t callback,
                                          void *user_data);

/**
 * @brief Get statistics
 * @param channel Channel handle
 * @param stats [out] Statistics
 * @return Error code
 */
airy_err_t ipc_channel_get_stats(const ipc_channel_t *channel, ipc_stats_t *stats);

/**
 * @brief Reset statistics
 * @param channel Channel handle
 * @return Error code
 */
airy_err_t ipc_channel_reset_stats(ipc_channel_t *channel);

/**
 * @brief Send a message
 * @param channel Channel handle
 * @param message Message structure
 * @return Error code
 */
airy_err_t ipc_send(ipc_channel_t *channel, const ipc_message_t *message);

/**
 * @brief Send data (simplified interface)
 * @param channel Channel handle
 * @param data Data buffer
 * @param len Data length
 * @param sent [out] Actual bytes sent (optional)
 * @return Error code
 */
airy_err_t ipc_send_data(ipc_channel_t *channel, const void *data, size_t len, size_t *sent);

/**
 * @brief Send a request and wait for the response
 * @param channel Channel handle
 * @param request Request message
 * @param response [out] Response message
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
airy_err_t ipc_send_request(ipc_channel_t *channel, ipc_message_t *request, ipc_message_t *response,
                            uint32_t timeout_ms);

/**
 * @brief Send a broadcast message
 * @param channel Channel handle
 * @param message Message structure
 * @return Error code
 */
airy_err_t ipc_broadcast(ipc_channel_t *channel, const ipc_message_t *message);

/**
 * @brief Send a notification message
 * @param channel Channel handle
 * @param notification Notification data
 * @param len Data length
 * @return Error code
 */
airy_err_t ipc_notify(ipc_channel_t *channel, const void *notification, size_t len);

/**
 * @brief Receive a message
 * @param channel Channel handle
 * @param message [out] Message structure
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
airy_err_t ipc_receive(ipc_channel_t *channel, ipc_message_t *message, uint32_t timeout_ms);

/**
 * @brief Receive data (simplified interface)
 * @param channel Channel handle
 * @param buffer Receive buffer
 * @param len Buffer length
 * @param received [out] Actual bytes received
 * @return Error code
 */
airy_err_t ipc_receive_data(ipc_channel_t *channel, void *buffer, size_t len, size_t *received);

/**
 * @brief Try to receive a message (non-blocking)
 * @param channel Channel handle
 * @param message [out] Message structure
 * @return Error code, AIRY_EBUSY if no message
 */
airy_err_t ipc_try_receive(ipc_channel_t *channel, ipc_message_t *message);

/**
 * @brief Set the message callback
 * @param channel Channel handle
 * @param callback Callback function
 * @param user_data User data
 * @return Error code
 */
airy_err_t ipc_set_message_callback(ipc_channel_t *channel, ipc_message_callback_t callback,
                                    void *user_data);

#endif /* AIRY_RT_IPC_CORE_API_H */
