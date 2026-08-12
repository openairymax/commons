// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file ipc_common_message.c
 * @brief IPC 消息辅助函数域（创建/释放/克隆/校验/序列化）
 *
 * 自 ipc_common.c 拆分：承载 ipc_message_create/free/clone/checksum/
 * verify/serialize/deserialize。通道生命周期与收发见 ipc_common.c，
 * 服务端/客户端、共享内存、消息队列与 RPC 见 ipc_server_client.c /
 * ipc_shm.c / ipc_mq.c / ipc_rpc.c。
 */

#include "ipc_common.h"

#include "platform.h"
#include "string_compat.h"

#include "ipc_common_internal.h"

ipc_message_t *ipc_message_create(ipc_msg_type_t type, const void *payload, size_t payload_len)
{
    ipc_message_t *msg = (ipc_message_t *)AIRY_CALLOC(1, sizeof(ipc_message_t));
    if (!msg) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    msg->header.magic = IPC_MAGIC;
    msg->header.version = 1;
    msg->header.type = (uint32_t)type;
    msg->header.flags = 0;
    msg->header.msg_id = 0;
    msg->header.correlation_id = 0;
    AIRY_MEMSET(msg->header.source, 0, sizeof(msg->header.source));
    AIRY_MEMSET(msg->header.target, 0, sizeof(msg->header.target));
    msg->header.payload_len = payload_len;
    msg->header.checksum = 0;
    msg->header.timestamp = ipc_get_timestamp_ns();
    AIRY_MEMSET(msg->header.reserved, 0, sizeof(msg->header.reserved));

    if (payload && payload_len > 0) {
        msg->payload = AIRY_MALLOC(payload_len);
        if (msg->payload) {
            __builtin_memcpy(msg->payload, payload, payload_len);
            msg->payload_size = payload_len;
        } else {
            AIRY_FREE(msg);
            AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
        }
    } else {
        msg->payload = NULL;
        msg->payload_size = 0;
    }

    msg->header.checksum = ipc_message_checksum(msg);

    return msg;
}

void ipc_message_free(ipc_message_t *message)
{
    if (!message) {
        return;
    }

    if (message->payload) {
        AIRY_FREE(message->payload);
        message->payload = NULL;
    }

    AIRY_FREE(message);
}

ipc_message_t *ipc_message_clone(const ipc_message_t *message)
{
    if (!message) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_message_t *clone = ipc_message_create((ipc_msg_type_t)message->header.type,
                                              message->payload, message->payload_size);

    if (clone) {
        clone->header = message->header;
        clone->header.correlation_id = 0;
    }

    return clone;
}

uint32_t ipc_message_checksum(const ipc_message_t *message)
{
    if (!message) {
        return 0;
    }

    /* Exclude the checksum field itself: otherwise the checksum would depend
     * on its own stored value, making verify() fail after create() stores it. */
    ipc_message_header_t header_copy = message->header;
    header_copy.checksum = 0;
    uint32_t header_crc = ipc_calc_crc32(&header_copy, sizeof(ipc_message_header_t));
    uint32_t payload_crc = 0;

    if (message->payload && message->payload_size > 0) {
        payload_crc = ipc_calc_crc32(message->payload, message->payload_size);
    }

    return header_crc ^ payload_crc;
}

bool ipc_message_verify(const ipc_message_t *message)
{
    if (!message) {
        return false;
    }

    if (message->header.magic != IPC_MAGIC) {
        return false;
    }

    uint32_t calculated = ipc_message_checksum(message);

    return calculated == message->header.checksum;
}

airy_err_t ipc_message_serialize(const ipc_message_t *message, void *buffer, size_t buffer_len,
                                 size_t *written)
{
    if (!message || !buffer) {
        return AIRY_EINVAL;
    }

    size_t total_size = sizeof(ipc_message_header_t) + message->payload_size;

    if (total_size > buffer_len) {
        return AIRY_EOVERFLOW;
    }

    __builtin_memcpy(buffer, &message->header, sizeof(ipc_message_header_t));

    if (message->payload && message->payload_size > 0) {
        __builtin_memcpy((char *)buffer + sizeof(ipc_message_header_t), message->payload,
                         message->payload_size);
    }

    if (written) {
        *written = total_size;
    }

    return AIRY_SUCCESS;
}

airy_err_t ipc_message_deserialize(const void *buffer, size_t len, ipc_message_t *message)
{
    if (!buffer || !message) {
        return AIRY_EINVAL;
    }

    if (len < sizeof(ipc_message_header_t)) {
        return AIRY_EINVAL;
    }

    __builtin_memcpy(&message->header, buffer, sizeof(ipc_message_header_t));

    if (message->header.payload_len > 0) {
        if (len < sizeof(ipc_message_header_t) + message->header.payload_len) {
            return AIRY_EINVAL;
        }

        message->payload = AIRY_MALLOC(message->header.payload_len);
        if (!message->payload) {
            return AIRY_ENOMEM;
        }

        __builtin_memcpy(message->payload, (const char *)buffer + sizeof(ipc_message_header_t),
                         message->header.payload_len);
        message->payload_size = message->header.payload_len;
    } else {
        message->payload = NULL;
        message->payload_size = 0;
    }

    return AIRY_SUCCESS;
}
