/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC message API: create/clone/checksum/verify/serialize/deserialize.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_MESSAGE_API_H
#define AIRY_RT_IPC_MESSAGE_API_H

#include "ipc_types.h"

/**
 * @brief Create a message
 * @param type Message type
 * @param payload Payload data
 * @param payload_len Payload length
 * @return Message structure, NULL on failure
 * @ownership Caller releases with ipc_message_free
 */
ipc_message_t *ipc_message_create(ipc_msg_type_t type, const void *payload, size_t payload_len);

/**
 * @brief Free a message
 * @param message Message structure
 */
void ipc_message_free(ipc_message_t *message);

/**
 * @brief Clone a message
 * @param message Source message
 * @return New message, NULL on failure
 */
ipc_message_t *ipc_message_clone(const ipc_message_t *message);

/**
 * @brief Compute the message checksum
 * @param message Message structure
 * @return CRC32 checksum
 */
uint32_t ipc_message_checksum(const ipc_message_t *message);

/**
 * @brief Verify the message checksum
 * @param message Message structure
 * @return true if valid, false if invalid
 */
bool ipc_message_verify(const ipc_message_t *message);

/**
 * @brief Serialize a message into a byte stream
 * @param message Message structure
 * @param buffer Output buffer
 * @param buffer_len Buffer length
 * @param written [out] Actual bytes written
 * @return Error code
 */
airy_err_t ipc_message_serialize(const ipc_message_t *message, void *buffer, size_t buffer_len,
                                 size_t *written);

/**
 * @brief Deserialize a message from a byte stream
 * @param buffer Input buffer
 * @param len Buffer length
 * @param message [out] Message structure
 * @return Error code
 */
airy_err_t ipc_message_deserialize(const void *buffer, size_t len, ipc_message_t *message);

#endif /* AIRY_RT_IPC_MESSAGE_API_H */
