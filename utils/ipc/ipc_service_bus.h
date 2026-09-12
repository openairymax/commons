/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file ipc_service_bus.h
 * @brief IPC service bus: unified inter-daemon communication framework
 *        (authoritative commons version).
 *
 * Provides an efficient communication abstraction layer between daemons:
 * bus bring-up (create/start/stop/destroy), channel + handler
 * registration, and request/response transport.
 *
 * P0.17 phase 3: migrated from daemons/common/include/ipc_service_bus.h
 * into commons, removing the atoms->daemons compile-time reverse
 * dependency (IRON-6). The daemons copy is kept as a re-exporting
 * compatibility header.
 *
 * 8.3.4 (0.1.15): the send/broadcast/notify family and the endpoint
 * registry (register/discover/select/health) were removed: send() never
 * delivered a message, and every real conversation flows through
 * request(). Surviving design principles:
 * 1. Unified message bus: all daemons communicate over a unified bus
 * 2. Protocol aware: messages carry a protocol type, supporting MCP/A2A/
 *    OpenAI API etc.
 * 3. Location transparency: request() addresses peers by service name
 * 4. Resilient communication: built-in retry and timeout
 *
 * @see svc_common.h service management framework
 * @see ipc_common.h IPC low-level abstraction
 */

#ifndef AIRY_RT_IPC_SERVICE_BUS_H
#define AIRY_RT_IPC_SERVICE_BUS_H

#include "svc_common.h"

#include <airymax/ipc.h> /* [SC] SSoT: AIRY_IPC_MAGIC (P0-05 convergence) */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define IPC_BUS_MAX_CHANNELS 32
#define IPC_BUS_MAX_MESSAGE_SIZE (512 * 1024)
#define IPC_BUS_DEFAULT_TIMEOUT_MS 5000
#define IPC_BUS_MAX_RETRIES 3
#define IPC_BUS_CHANNEL_NAME_LEN 128
#define IPC_BUS_SERVICE_ID_LEN 64


typedef enum {
    IPC_BUS_MSG_REQUEST = 0,
    IPC_BUS_MSG_RESPONSE = 1
} ipc_bus_msg_type_t;


typedef enum {
    IPC_BUS_PROTO_JSON_RPC = 0,
    IPC_BUS_PROTO_MCP = 1,
    IPC_BUS_PROTO_A2A = 2,
    IPC_BUS_PROTO_OPENAI = 3,
    IPC_BUS_PROTO_AUTO = 4
} ipc_bus_proto_t;


/*
 * A-IPC 对齐（Unify Design SSoT，P0-05 收敛）：
 * wire 格式前 128B 为 [SC] airy_ipc_msg_hdr（Layout C v4），与
 * agentrt-linux / AirymaxOS 内核态 fastpath 逐字节兼容；service bus
 * 语义字段（msg_type/protocol/msg_id/correlation_id/source/target 等）
 * 位于标准头之后的扩展段。payload_len/crc32 复用 [SC] 头字段。
 */
typedef struct {
    struct airy_ipc_msg_hdr aipc;   /* [SC] A-IPC 128B 标准头（offset 0） */
    /* —— A-IPC 扩展段（128B 标准头之后）—— */
    ipc_bus_msg_type_t msg_type;
    ipc_bus_proto_t protocol;
    uint64_t msg_id;
    uint64_t correlation_id;
    char source[IPC_BUS_SERVICE_ID_LEN];
    char target[IPC_BUS_SERVICE_ID_LEN];
    uint32_t flags;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t reserved[16];
} ipc_bus_message_header_t;


#define IPC_BUS_MESSAGE_MAGIC   AIRY_IPC_MAGIC
#define IPC_BUS_MESSAGE_OPCODE  AIRY_IPC_OP_SEND  /* 数据面消息统一 SEND opcode */


typedef struct {
    ipc_bus_message_header_t header;
    void *payload;
    size_t payload_size;
} ipc_bus_message_t;


typedef struct {
    char name[IPC_BUS_CHANNEL_NAME_LEN];
    ipc_bus_proto_t default_protocol;
    uint32_t timeout_ms;
    uint32_t max_retries;
    uint32_t buffer_size;
    bool enable_compression;
    bool enable_encryption;
} ipc_bus_channel_config_t;


typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t errors;
    uint64_t timeouts;
    uint64_t avg_latency_us;
    uint64_t max_latency_us;
    uint32_t active_channels;
} ipc_bus_stats_t;


typedef struct ipc_service_bus_s *ipc_service_bus_t;
typedef struct ipc_bus_channel_s *ipc_bus_channel_t;


typedef int (*ipc_bus_message_handler_t)(ipc_bus_channel_t channel,
                                         const ipc_bus_message_t *message, void *user_data);


/**
 * @brief Create a service bus instance
 * @param bus_name Bus name
 * @param config Bus configuration (NULL for defaults)
 * @return Bus handle, NULL on failure
 */
AIRY_API ipc_service_bus_t ipc_service_bus_create(const char *bus_name,
                                                  const ipc_bus_channel_config_t *config);

/**
 * @brief Destroy a service bus instance
 * @param bus Bus handle
 */
AIRY_API void ipc_service_bus_destroy(ipc_service_bus_t bus);

/**
 * @brief Start the service bus
 * @param bus Bus handle
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t ipc_service_bus_start(ipc_service_bus_t bus);

/**
 * @brief Stop the service bus
 * @param bus Bus handle
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t ipc_service_bus_stop(ipc_service_bus_t bus);


/**
 * @brief Create a communication channel
 * @param bus Bus handle
 * @param config Channel configuration
 * @return Channel handle, NULL on failure
 */
AIRY_API ipc_bus_channel_t ipc_bus_channel_create(ipc_service_bus_t bus,
                                                  const ipc_bus_channel_config_t *config);

/**
 * @brief Destroy a communication channel
 * @param channel Channel handle
 */
AIRY_API void ipc_bus_channel_destroy(ipc_bus_channel_t channel);

/**
 * @brief Get the channel name
 * @param channel Channel handle
 * @return Channel name
 */
AIRY_API const char *ipc_bus_channel_get_name(ipc_bus_channel_t channel);


/**
 * @brief Send a request and wait for the response
 * @param bus Bus handle
 * @param target_service Target service name
 * @param request Request message
 * @param response [out] Response message
 * @param timeout_ms Timeout
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t ipc_service_bus_request(ipc_service_bus_t bus, const char *target_service,
                                            const ipc_bus_message_t *request,
                                            ipc_bus_message_t *response, uint32_t timeout_ms);


/**
 * @brief Register a message handler
 * @param bus Bus handle
 * @param handler Message handler function
 * @param user_data User data
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t ipc_service_bus_register_handler(ipc_service_bus_t bus,
                                                     ipc_bus_message_handler_t handler,
                                                     void *user_data);

/**
 * @brief Unregister a message handler
 * @param bus Bus handle
 * @param handler Message handler function
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t ipc_service_bus_unregister_handler(ipc_service_bus_t bus,
                                                       ipc_bus_message_handler_t handler);


/**
 * @brief Create a service bus message
 * @param msg_type Message type
 * @param protocol Protocol type
 * @param payload Payload data
 * @param payload_size Payload size
 * @return Message structure, NULL on failure
 */
AIRY_API ipc_bus_message_t *ipc_bus_message_create(ipc_bus_msg_type_t msg_type,
                                                   ipc_bus_proto_t protocol, const void *payload,
                                                   size_t payload_size);

/**
 * @brief Free a service bus message
 * @param message Message structure
 */
AIRY_API void ipc_bus_message_free(ipc_bus_message_t *message);

/**
 * @brief Convert a protocol type to a string
 * @param proto Protocol type
 * @return Protocol name string
 */
AIRY_API const char *ipc_bus_proto_to_string(ipc_bus_proto_t proto);

/**
 * @brief Convert a string to a protocol type
 * @param str Protocol name
 * @return Protocol type
 */
AIRY_API ipc_bus_proto_t ipc_bus_proto_from_string(const char *str);


/**
 * @brief Get service bus statistics
 * @param bus Bus handle
 * @param stats [out] Statistics
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t ipc_service_bus_get_stats(ipc_service_bus_t bus, ipc_bus_stats_t *stats);

/**
 * @brief Reset statistics
 * @param bus Bus handle
 * @return 0 on success, non-zero on failure
 */
AIRY_API airy_err_t ipc_service_bus_reset_stats(ipc_service_bus_t bus);

/**
 * @brief Get the bus name
 * @param bus Bus handle
 * @return Bus name
 */
AIRY_API const char *ipc_service_bus_get_name(ipc_service_bus_t bus);

/**
 * @brief Check whether the bus is running
 * @param bus Bus handle
 * @return true if running, false otherwise
 */
AIRY_API bool ipc_service_bus_is_running(ipc_service_bus_t bus);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_SERVICE_BUS_H */
