/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file ipc_common.h
 * @brief Inter-process communication module: cross-platform IPC abstraction
 *        layer.
 *
 * @details
 * This module provides cross-platform IPC functionality, including:
 * - Pipe
 * - Named Pipe / FIFO
 * - Unix Domain Socket / Windows Named Pipe
 * - Shared Memory
 * - Message Queue
 * - RPC call framework
 *
 * Supported platforms:
 * - Windows (Named Pipe, Mailslot, Shared Memory)
 * - Linux (Unix Socket, POSIX MQ, Shared Memory)
 * - macOS (Unix Socket, POSIX MQ, Shared Memory)
 *
 * Design principles:
 * - Unified message format and protocol
 * - Synchronous and asynchronous communication modes
 * - Built-in timeout and retry mechanisms
 * - Thread-safe design
 *
 * @note Thread safety: all public interfaces are thread-safe
 * @see ARCHITECTURAL_PRINCIPLES.md E-4 cross-platform consistency
 */

#ifndef AIRY_RT_IPC_COMMON_H
#define AIRY_RT_IPC_COMMON_H

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
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t type;
    uint32_t flags;
    uint64_t msg_id;
    uint64_t correlation_id;
    char source[64];
    char target[64];
    uint64_t payload_len;
    uint32_t checksum;
    airy_timestamp_t timestamp;
    uint8_t reserved[32];
} ipc_message_header_t;

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
 * @ownership Caller releases with ipc_channel_destroy
 */
ipc_channel_t *ipc_server_accept(ipc_server_t *server, uint32_t timeout_ms);

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

/**
 * @brief Shared memory handle
 */
typedef struct ipc_shm ipc_shm_t;

/**
 * @brief Shared memory configuration
 */
typedef struct {
    const char *name;
    size_t size;
    bool read_only;
    bool create;
    bool exclusive;
    const char *permissions;
} ipc_shm_config_t;

/**
 * @brief Create shared memory
 * @param config Shared memory configuration
 * @return Shared memory handle, NULL on failure
 */
ipc_shm_t *ipc_shm_create(const ipc_shm_config_t *config);

/**
 * @brief Destroy shared memory
 * @param shm Shared memory handle
 */
void ipc_shm_destroy(ipc_shm_t *shm);

/**
 * @brief Map shared memory into the process address space
 * @param shm Shared memory handle
 * @return Mapped address, NULL on failure
 */
void *ipc_shm_map(ipc_shm_t *shm);

/**
 * @brief Unmap shared memory
 * @param shm Shared memory handle
 * @return Error code
 */
airy_err_t ipc_shm_unmap(ipc_shm_t *shm);

/**
 * @brief Get the shared memory size
 * @param shm Shared memory handle
 * @return Shared memory size
 */
size_t ipc_shm_get_size(const ipc_shm_t *shm);

/**
 * @brief Synchronize shared memory
 * @param shm Shared memory handle
 * @return Error code
 */
airy_err_t ipc_shm_sync(ipc_shm_t *shm);

/**
 * @brief Message queue handle
 */
typedef struct ipc_mq ipc_mq_t;

/**
 * @brief Message queue configuration
 */
typedef struct {
    const char *name;
    size_t max_messages;
    size_t max_message_size;
    bool create;
    bool exclusive;
    const char *permissions;
} ipc_mq_config_t;

/**
 * @brief Create a message queue
 * @param config Message queue configuration
 * @return Message queue handle, NULL on failure
 */
ipc_mq_t *ipc_mq_create(const ipc_mq_config_t *config);

/**
 * @brief Destroy a message queue
 * @param mq Message queue handle
 */
void ipc_mq_destroy(ipc_mq_t *mq);

/**
 * @brief Send a message to the queue
 * @param mq Message queue handle
 * @param data Message data
 * @param len Data length
 * @param priority Priority (0 is lowest)
 * @return Error code
 */
airy_err_t ipc_mq_send(ipc_mq_t *mq, const void *data, size_t len, unsigned int priority);

/**
 * @brief Receive a message from the queue
 * @param mq Message queue handle
 * @param buffer Receive buffer
 * @param len Buffer length
 * @param received [out] Actual bytes received
 * @param priority [out] Message priority (optional)
 * @param timeout_ms Timeout
 * @return Error code
 */
airy_err_t ipc_mq_receive(ipc_mq_t *mq, void *buffer, size_t len, size_t *received,
                          unsigned int *priority, uint32_t timeout_ms);

/**
 * @brief Get the current message count of the queue
 * @param mq Message queue handle
 * @return Message count
 */
size_t ipc_mq_count(const ipc_mq_t *mq);

/**
 * @brief Clear the message queue
 * @param mq Message queue handle
 * @return Error code
 */
airy_err_t ipc_mq_clear(ipc_mq_t *mq);

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

/**
 * @brief Check whether a channel is usable
 * @param channel Channel handle
 * @return true if usable, false otherwise
 */
bool ipc_is_valid(const ipc_channel_t *channel);

/**
 * @brief Flush the channel buffer
 * @param channel Channel handle
 * @return Error code
 */
airy_err_t ipc_flush(ipc_channel_t *channel);

/*
 * Type conversion API (IPC internal types <-> AgentRT unified types)
 */

/**
 * @brief Convert an AgentRT unified IPC type to an IPC-module internal type
 * @param airy_type AgentRT unified IPC type
 * @return IPC-module internal type
 */
static inline ipc_type_t ipc_type_from_agentrt(airy_ipc_type_t airy_type)
{
    switch (airy_type) {
    case AIRY_IPC_PIPE:
        return IPC_TYPE_PIPE;
    case AIRY_IPC_SOCKET:
        return IPC_TYPE_SOCKET;
    case AIRY_IPC_SHM:
        return IPC_TYPE_SHM;
    case AIRY_IPC_MQ:
        return IPC_TYPE_MQ;
    case AIRY_IPC_RPC:
        return IPC_TYPE_RPC;
    default:
        return IPC_TYPE_PIPE;
    }
}

/**
 * @brief Convert an IPC-module internal type to an AgentRT unified IPC type
 * @param ipc_type IPC-module internal type
 * @return AgentRT unified IPC type
 */
static inline airy_ipc_type_t ipc_type_to_agentrt(ipc_type_t ipc_type)
{
    switch (ipc_type) {
    case IPC_TYPE_PIPE:
        return AIRY_IPC_PIPE;
    case IPC_TYPE_NAMED_PIPE:
        return AIRY_IPC_SOCKET;
    case IPC_TYPE_SOCKET:
        return AIRY_IPC_SOCKET;
    case IPC_TYPE_SHM:
        return AIRY_IPC_SHM;
    case IPC_TYPE_MQ:
        return AIRY_IPC_MQ;
    case IPC_TYPE_RPC:
        return AIRY_IPC_RPC;
    default:
        return AIRY_IPC_PIPE;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_COMMON_H */
