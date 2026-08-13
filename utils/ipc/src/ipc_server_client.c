// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file ipc_server_client.c
 * @brief IPC module - server/client implementation.
 *
 * Implements the server and client API declared in ipc_common.h:
 * - Server: create/destroy/start/stop/accept-connection/connection-count/
 *   broadcast
 * - Client: create/destroy/connect/disconnect/get-channel
 *
 * The server and client are built on the ipc_channel_t abstraction;
 * channel create/open/close and broadcast are provided by ipc_common.c
 * (ipc_channel_create/open/close, ipc_broadcast). This file only handles
 * connection management and lifecycle control.
 *
 * Following ARCHITECTURAL_PRINCIPLES.md design principles:
 * - E-5 Semantic naming: every function name states its purpose
 * - E-6 Traceable errors: unified error code system
 *
 * @see ipc_common_internal.h internal shared definitions
 */

#include "ipc_common_internal.h"

#include "ipc_common.h"

#include "platform.h"

ipc_server_t *ipc_server_create(const ipc_config_t *config)
{
    if (!config) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_server_t *server = (ipc_server_t *)AIRY_CALLOC(1, sizeof(ipc_server_t));
    if (!server) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    server->config = *config;
    server->state = IPC_STATE_CLOSED;
    server->connection_count = 0;
    server->connections = NULL;
    server->max_connections =
        config->max_connections > 0 ? config->max_connections : IPC_MAX_CONNECTIONS;
    AIRY_MEMSET(server->error_msg, 0, sizeof(server->error_msg));

    return server;
}

void ipc_server_destroy(ipc_server_t *server)
{
    if (!server) {
        return;
    }

    if (server->state == IPC_STATE_OPEN) {
        ipc_server_stop(server);
    }

    AIRY_FREE(server->connections);
    AIRY_FREE(server);
}

airy_err_t ipc_server_start(ipc_server_t *server)
{
    if (!server) {
        return AIRY_EINVAL;
    }

    if (server->state != IPC_STATE_CLOSED && server->state != IPC_STATE_ERROR) {
        return AIRY_EBUSY;
    }

    server->state = IPC_STATE_OPENING;

    server->connections =
        (ipc_channel_t **)AIRY_CALLOC(server->max_connections, sizeof(ipc_channel_t *));
    if (!server->connections && server->max_connections > 0) {
        server->state = IPC_STATE_ERROR;
        return AIRY_ENOMEM;
    }

    server->state = IPC_STATE_OPEN;

    return AIRY_SUCCESS;
}

airy_err_t ipc_server_stop(ipc_server_t *server)
{
    if (!server) {
        return AIRY_EINVAL;
    }

    server->state = IPC_STATE_CLOSING;

    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connections[i]) {
            ipc_channel_destroy(server->connections[i]);
        }
    }

    AIRY_FREE(server->connections);
    server->connections = NULL;
    server->connection_count = 0;

    server->state = IPC_STATE_CLOSED;

    return AIRY_SUCCESS;
}

airy_err_t ipc_server_disconnect(ipc_server_t *server, ipc_channel_t *channel)
{
    if (!server || !channel) {
        return AIRY_EINVAL;
    }

    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connections[i] != channel) {
            continue;
        }

        ipc_channel_destroy(channel);

        for (size_t j = i; j + 1 < server->connection_count; j++) {
            server->connections[j] = server->connections[j + 1];
        }
        server->connection_count--;
        server->connections[server->connection_count] = NULL;

        return AIRY_SUCCESS;
    }

    return AIRY_ENOTFOUND;
}

ipc_channel_t *ipc_server_accept(ipc_server_t *server, uint32_t timeout_ms)
{
    if (!server) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (server->state != IPC_STATE_OPEN) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (server->connection_count >= server->max_connections) {
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    if (server->config.type == IPC_TYPE_SOCKET) {
        ipc_channel_t *listen_channel =
            server->connections && server->connection_count > 0 ? server->connections[0] : NULL;
        int listen_fd = listen_channel ? listen_channel->socket_fd : -1;
        if (listen_fd < 0) {
            AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
        }

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        int sel = select(listen_fd + 1, &readfds, NULL, NULL, &tv);
        if (sel <= 0) {
            AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
        }

        struct sockaddr_un addr;
        socklen_t addr_len = sizeof(addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addr_len);
        if (client_fd < 0) {
            AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
        }

        ipc_channel_t *client_channel = ipc_channel_create(&server->config);
        if (!client_channel) {
            close(client_fd);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        client_channel->socket_fd = client_fd;
        client_channel->state = IPC_STATE_OPEN;

        if (server->connection_count < server->max_connections) {
            server->connections[server->connection_count] = client_channel;
            server->connection_count++;
        }

        return client_channel;
    }

    ipc_channel_t *client_channel = ipc_channel_create(&server->config);
    if (!client_channel) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_channel_open(client_channel);

    server->connections[server->connection_count] = client_channel;
    server->connection_count++;

    return client_channel;
}

size_t ipc_server_connection_count(const ipc_server_t *server)
{
    if (!server) {
        return 0;
    }
    return server->connection_count;
}

airy_err_t ipc_server_broadcast(ipc_server_t *server, const ipc_message_t *message)
{
    if (!server || !message) {
        return AIRY_EINVAL;
    }

    airy_err_t overall_err = AIRY_SUCCESS;

    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connections[i]) {
            airy_err_t err = ipc_broadcast(server->connections[i], message);
            if (err != AIRY_SUCCESS) {
                overall_err = err;
            }
        }
    }

    return overall_err;
}

ipc_client_t *ipc_client_create(const ipc_config_t *config)
{
    if (!config) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_client_t *client = (ipc_client_t *)AIRY_CALLOC(1, sizeof(ipc_client_t));
    if (!client) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    client->config = *config;
    client->channel = NULL;
    client->state = IPC_STATE_CLOSED;
    AIRY_MEMSET(client->error_msg, 0, sizeof(client->error_msg));

    return client;
}

void ipc_client_destroy(ipc_client_t *client)
{
    if (!client) {
        return;
    }

    if (client->state == IPC_STATE_OPEN) {
        ipc_client_disconnect(client);
    }

    AIRY_FREE(client);
}

airy_err_t ipc_client_connect(ipc_client_t *client, uint32_t timeout_ms)
{
    if (!client) {
        return AIRY_EINVAL;
    }

    if (client->state != IPC_STATE_CLOSED && client->state != IPC_STATE_ERROR) {
        return AIRY_EBUSY;
    }

    client->state = IPC_STATE_OPENING;

    if (client->config.type == IPC_TYPE_SOCKET) {
        int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            client->state = IPC_STATE_ERROR;
            return AIRY_EIO;
        }

        struct sockaddr_un addr;
        AIRY_MEMSET(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        const char *path = client->config.name ? client->config.name : AIRY_TMP_DIR "/ipc";
        AIRY_STRNCPY_TERM(addr.sun_path, path, sizeof(addr.sun_path));

        if (timeout_ms > 0 && client->config.nonblocking) {
            int flags = fcntl(sock_fd, F_GETFL, 0);
            fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
        }

        int ret = connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret < 0) {
            if (errno == EINPROGRESS && client->config.nonblocking) {
                struct timeval tv;
                tv.tv_sec = timeout_ms / 1000;
                tv.tv_usec = (timeout_ms % 1000) * 1000;
                fd_set writefds;
                FD_ZERO(&writefds);
                FD_SET(sock_fd, &writefds);
                int sel = select(sock_fd + 1, NULL, &writefds, NULL, &tv);
                if (sel <= 0) {
                    close(sock_fd);
                    client->state = IPC_STATE_ERROR;
                    return AIRY_ETIMEDOUT;
                }
                int err = 0;
                socklen_t err_len = sizeof(err);
                getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
                if (err != 0) {
                    close(sock_fd);
                    client->state = IPC_STATE_ERROR;
                    return AIRY_EIO;
                }
            } else {
                close(sock_fd);
                client->state = IPC_STATE_ERROR;
                return AIRY_EIO;
            }
        }

        client->channel = ipc_channel_create(&client->config);
        if (!client->channel) {
            close(sock_fd);
            client->state = IPC_STATE_ERROR;
            return AIRY_ENOMEM;
        }
        client->channel->socket_fd = sock_fd;
        client->channel->state = IPC_STATE_OPEN;
        client->state = IPC_STATE_OPEN;
        return AIRY_SUCCESS;
    }

    client->channel = ipc_channel_create(&client->config);
    if (!client->channel) {
        client->state = IPC_STATE_ERROR;
        return AIRY_ENOMEM;
    }

    airy_err_t err = ipc_channel_open(client->channel);
    if (err != AIRY_SUCCESS) {
        ipc_channel_destroy(client->channel);
        client->channel = NULL;
        client->state = IPC_STATE_ERROR;
        return err;
    }

    client->state = IPC_STATE_OPEN;

    return AIRY_SUCCESS;
}

airy_err_t ipc_client_disconnect(ipc_client_t *client)
{
    if (!client) {
        return AIRY_EINVAL;
    }

    if (client->channel) {
        ipc_channel_close(client->channel);
        ipc_channel_destroy(client->channel);
        client->channel = NULL;
    }

    client->state = IPC_STATE_CLOSED;

    return AIRY_SUCCESS;
}

ipc_channel_t *ipc_client_get_channel(ipc_client_t *client)
{
    if (!client) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    return client->channel;
}
