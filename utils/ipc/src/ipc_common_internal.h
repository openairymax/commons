/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file ipc_common_internal.h
 * @brief IPC module internal shared definitions.
 *
 * After ipc_common.c was split by functional domain, this header carries
 * the shared contract between the pieces:
 *   - ipc_common.c  channel basics + message send/receive
 *   - ipc_server.c  server/client
 *   - ipc_shm.c     shared memory
 *   - ipc_mq.c      message queue
 *   - ipc_rpc.c     RPC framework
 */

#ifndef AIRY_RT_IPC_COMMON_INTERNAL_H
#define AIRY_RT_IPC_COMMON_INTERNAL_H

#include "ipc_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OFF_MAX
#ifdef LLONG_MAX
#define OFF_MAX ((off_t)(LLONG_MAX >> 1))
#else
#define OFF_MAX ((off_t)((1LL << (sizeof(off_t) * 8 - 1)) - 1))
#endif
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif
#else
#include "airy_mman.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include "airy_memory.h"
#endif

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "error.h"

/* ---- Internal opaque structs (module-internal, accessed via API) ---- */

struct ipc_channel {
    ipc_config_t config;
    ipc_state_t state;
    uint64_t msg_id_counter;
    ipc_stats_t stats;
    ipc_event_callback_t event_cb;
    void *event_user_data;
    ipc_message_callback_t msg_cb;
    void *msg_user_data;
    char error_msg[256];

#ifdef _WIN32
    HANDLE hPipe;
    HANDLE hReadEvent;
    HANDLE hWriteEvent;
#else
    int fd_read;
    int fd_write;
    int socket_fd;
#endif
    void *internal_buffer;
    size_t buffer_used;
};

struct ipc_server {
    ipc_config_t config;
    ipc_state_t state;
    size_t connection_count;
    ipc_channel_t **connections;
    size_t max_connections;
    char error_msg[256];
};

struct ipc_client {
    ipc_config_t config;
    ipc_channel_t *channel;
    ipc_state_t state;
    char error_msg[256];
};

struct ipc_shm {
    ipc_shm_config_t config;
    void *mapped_addr;
    size_t actual_size;
#ifdef _WIN32
    HANDLE hMapFile;
#else
    int shm_fd;
#endif
    bool is_mapped;
    char error_msg[256];
};

typedef struct ipc_mq_message {
    void *data;
    size_t len;
    unsigned int priority;
    uint64_t timestamp;
    struct ipc_mq_message *next;
} ipc_mq_message_t;

struct ipc_mq {
    ipc_mq_config_t config;
    size_t current_count;
    size_t total_enqueued;
    size_t total_dequeued;
    ipc_mq_message_t *head;
    ipc_mq_message_t *tail;
#ifdef _WIN32
    HANDLE hMutex;
    HANDLE hNotEmpty;
#else
    airy_mtx_t mutex;
    airy_cond_t not_empty;
#endif
    char error_msg[256];
};

typedef struct rpc_method_node {
    char *method_name;
    rpc_method_handler_t handler;
    void *user_data;
    struct rpc_method_node *next;
} rpc_method_node_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t request_id;
    uint32_t method_name_len;
    uint64_t payload_len;
    uint32_t status;
    char method_name[256];
} ipc_rpc_header_t;

#define IPC_RPC_MAGIC 0x52504300 /* "RPC\0" */
struct ipc_rpc_server {
    char *service_name;
    rpc_method_node_t *methods;
    size_t method_count;
    ipc_channel_t *transport;
    size_t max_request_size;
    size_t max_response_size;
    uint64_t request_id_counter;
    bool running;
    char error_msg[256];
};

struct ipc_rpc_client {
    ipc_channel_t *transport;
    uint32_t timeout_ms;
    uint64_t request_id_counter;
    char error_msg[256];
};

/* ---- Shared helper functions (defined in ipc_common.c) ---- */
uint64_t ipc_get_timestamp_ns(void);
uint32_t ipc_calc_crc32(const void *data, size_t len);
extern bool g_ipc_initialized;

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_COMMON_INTERNAL_H */
