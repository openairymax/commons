// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file network_common_internal.h
 * @brief Network module internal shared definitions: connection/connection
 * pool structs and cross-file helper declarations.
 */

#ifndef AIRY_NETWORK_COMMON_INTERNAL_H
#define AIRY_NETWORK_COMMON_INTERNAL_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "../../memory/include/airy_memory.h"
#include "network_common.h"

#include <stddef.h>
#include <stdint.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "error.h"

struct network_connection {
    network_config_t config;
    network_status_t status;
    char error_msg[256];
    network_event_callback_t event_cb;
    void *event_user_data;
    network_stats_t stats;
#ifdef _WIN32
    SOCKET sock;
#else
    int fd;
#endif
    struct sockaddr_in addr;
};

struct network_pool {
    network_config_t base_config;
    size_t max_size;
    size_t current_size;
    struct network_connection **connections;
};

int network_init_winsock(void);

void set_nonblocking_mode(void *handle);

void set_socket_timeout(void *handle, int timeout_ms, int is_recv);

int af_to_native(network_af_t af);

int socktype_to_native(network_sock_type_t st);

#endif /* AIRY_NETWORK_COMMON_INTERNAL_H */
