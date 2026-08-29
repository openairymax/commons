/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC module internal types: constants / enums / structs / callbacks.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_TYPES_H
#define AIRY_RT_IPC_TYPES_H

#include <error.h>
#include <types.h>
#include <airymax/ipc.h> /* [SC] SSoT: AIRY_IPC_MAGIC (P0-05 convergence) */

#ifdef __cplusplus
extern "C" {
#endif
/*
 * This module (ipc_common.h) defines two type systems:
 *
 * 1. IPC-module internal types (defined here, ipc_ prefix)
 *    - Used for the IPC module's internal implementation
 *    - Provide finer-grained control (e.g. a dedicated IPC_TYPE_NAMED_PIPE)
 *    - Cover the full IPC feature set (server/client/SHM/MQ)
 *
 * 2. AgentRT unified types (defined in types.h, airy_ prefix)
 *    - Used for cross-module interface contracts
 *    - Provide a simplified abstraction layer
 *    - Stay consistent with the other AgentRT components
 *
 * Usage:
 * - Use ipc_* types inside the IPC module implementation
 * - Use airy_ipc_* types at cross-module interfaces
 * - The two are interconvertible (see the conversion API at the end)
 */

#define IPC_MAGIC AIRY_IPC_MAGIC

#define IPC_DEFAULT_TIMEOUT_MS 5000

#define IPC_MAX_MESSAGE_SIZE (1024 * 1024) /* 1MB */

#define IPC_DEFAULT_BUFFER_SIZE 65536

#define IPC_MAX_NAME_LEN 256

#define IPC_MAX_CONNECTIONS 128

#define IPC_MESSAGE_ALIGN 8

/**
 * @brief IPC channel type enumeration
 */
typedef enum {
    IPC_TYPE_PIPE = 0,
    IPC_TYPE_NAMED_PIPE = 1,
    IPC_TYPE_SOCKET = 2, /**< Unix Socket / Windows Named Pipe */
    IPC_TYPE_SHM = 3,
    IPC_TYPE_MQ = 4,
    IPC_TYPE_RPC = 5
} ipc_type_t;

/**
 * @brief IPC mode enumeration
 */
typedef enum { IPC_MODE_READ = 1, IPC_MODE_WRITE = 2, IPC_MODE_READ_WRITE = 3 } ipc_mode_t;

/**
 * @brief IPC state enumeration
 */
typedef enum {
    IPC_STATE_CLOSED = 0,
    IPC_STATE_OPENING = 1,
    IPC_STATE_OPEN = 2,
    IPC_STATE_CLOSING = 3,
    IPC_STATE_ERROR = 4
} ipc_state_t;

/**
 * @brief IPC message flags
 */
typedef enum {
    IPC_FLAG_NONE = 0,
    IPC_FLAG_NONBLOCK = 1,
    IPC_FLAG_PRIORITY = 2,
    IPC_FLAG_BROADCAST = 4,
    IPC_FLAG_EXCLUSIVE = 8,
    IPC_FLAG_PERSISTENT = 16
} ipc_flag_t;

/**
 * @brief IPC message type
 */
typedef enum {
    IPC_MSG_DATA = 0,
    IPC_MSG_REQUEST = 1,
    IPC_MSG_RESPONSE = 2,
    IPC_MSG_NOTIFICATION = 3,
    IPC_MSG_ERROR = 4,
    IPC_MSG_CONTROL = 5
} ipc_msg_type_t;

/**
 * @brief IPC event type
 */
typedef enum {
    IPC_EVENT_CONNECTED = 1,
    IPC_EVENT_DISCONNECTED = 2,
    IPC_EVENT_MESSAGE = 3,
    IPC_EVENT_ERROR = 4,
    IPC_EVENT_TIMEOUT = 5,
    IPC_EVENT_BUFFER_FULL = 6,
    IPC_EVENT_BUFFER_EMPTY = 7
} ipc_event_t;

/**
 * @brief IPC message header structure
 *
 * A-IPC 对齐（Unify Design SSoT，P0-05 收敛）：wire 格式前 128B 为 [SC]
 * airy_ipc_msg_hdr（Layout C v4），与 agent-linux / AirymaxOS 内核态
 * fastpath 逐字节兼容；IPC 模块内部语义字段（version/type/flags/msg_id/
 * correlation_id/source/target/checksum/timestamp 等）位于标准头之后的
 * 扩展段。magic/payload_len/crc32 复用 [SC] 头字段。
 */
typedef struct {
    struct airy_ipc_msg_hdr aipc; /* [SC] A-IPC 128B 标准头（offset 0） */
    /* —— IPC 模块内部扩展段（128B 标准头之后）—— */
    uint32_t version;             /* IPC 版本 */
    uint32_t type;                /* IPC 消息类型（IPC_MSG_*） */
    uint32_t flags;               /* IPC 消息标志（IPC_FLAG_*） */
    uint64_t msg_id;              /* 消息 ID */
    uint64_t correlation_id;      /* 关联 ID（请求/响应配对） */
    char source[64];              /* 源标识 */
    char target[64];              /* 目标标识 */
    uint32_t checksum;            /* 消息校验和（header_crc ^ payload_crc） */
    airy_timestamp_t timestamp;   /* 时间戳（ns） */
    uint8_t reserved[32];         /* 保留 */
} ipc_message_header_t;

/* 兼容访问宏：magic/payload_len/crc32 复用 [SC] 头字段 */
#define IPC_HDR_MAGIC(h)        ((h)->aipc.magic)
#define IPC_HDR_PAYLOAD_LEN(h)  ((h)->aipc.payload_len)
#define IPC_HDR_CRC32(h)        ((h)->aipc.crc32)

/**
 * @brief IPC message structure
 */
typedef struct {
    ipc_message_header_t header;
    void *payload;
    size_t payload_size;
} ipc_message_t;

/**
 * @brief IPC channel configuration
 */
typedef struct {
    ipc_type_t type;
    const char *name;
    ipc_mode_t mode;
    size_t buffer_size;
    size_t max_message_size;
    uint32_t timeout_ms;
    uint32_t max_connections;
    bool nonblocking;
    bool persistent;
    const char *permissions;
} ipc_config_t;

/**
 * @brief IPC statistics
 */
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t errors;
    uint64_t timeouts;
    uint64_t avg_latency_us;
    uint64_t max_latency_us;
} ipc_stats_t;

/**
 * @brief IPC channel handle (opaque pointer)
 */
typedef struct ipc_channel ipc_channel_t;

/**
 * @brief IPC server handle (opaque pointer)
 */
typedef struct ipc_server ipc_server_t;

/**
 * @brief IPC client handle (opaque pointer)
 */
typedef struct ipc_client ipc_client_t;

/**
 * @brief IPC event callback function type
 * @param channel Channel handle
 * @param event Event type
 * @param data Event data
 * @param data_len Data length
 * @param user_data User data
 */
typedef void (*ipc_event_callback_t)(ipc_channel_t *channel, ipc_event_t event, const void *data,
                                     size_t data_len, void *user_data);

/**
 * @brief IPC message callback function type
 * @param channel Channel handle
 * @param message Message structure
 * @param user_data User data
 * @return 0 to continue processing, non-zero to stop
 */
typedef int (*ipc_message_callback_t)(ipc_channel_t *channel, const ipc_message_t *message,
                                      void *user_data);
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_TYPES_H */
