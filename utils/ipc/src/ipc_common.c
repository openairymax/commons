// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file ipc_common.c
 * @brief IPC module - channel basics and message send/receive.
 *
 * Implements the IPC core functionality declared in ipc_common.h:
 * - Init/cleanup and default config
 * - Channel lifecycle management (create/destroy/open/close/state/type/
 *   timeout/callback/stats)
 * - Message send/receive (send/send-data/request-response/broadcast/
 *   notify/receive/callback)
 * - Message helpers (create/free/clone/checksum/validate/serialize/
 *   deserialize)
 * - Utilities (error message/validity check/flush)
 *
 * Server/client, shared memory, message queue and RPC frameworks are
 * split into ipc_server_client.c / ipc_shm.c / ipc_mq.c / ipc_rpc.c;
 * module-internal shared definitions and helpers are declared in
 * ipc_common_internal.h.
 *
 * Following ARCHITECTURAL_PRINCIPLES.md design principles:
 * - E-4 Cross-platform consistency: Windows/Linux/macOS
 * - E-5 Semantic naming: every function name states its purpose
 * - E-6 Traceable errors: unified error code system
 * - E-8 Testability: all public interfaces independently testable
 *
 * Implementation strategy:
 * - Core functionality fully implemented (init, channel mgmt, messages)
 * - Platform-specific functionality behind #ifdef _WIN32
 *
 * @see ARCHITECTURAL_PRINCIPLES.md E-4/E-5/E-6/E-8
 */

#include "ipc_common.h"

#include "platform.h"
#include "string_compat.h"

#include "ipc_common_internal.h"

#ifndef _WIN32
#include <errno.h>
#include <poll.h>
#endif

/* ---- Internal shared helpers (declared in ipc_common_internal.h) ---- */

uint64_t ipc_get_timestamp_ns(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart / freq.QuadPart * 1000000000.0);
#else
    return airy_time_ns();
#endif
}

uint32_t ipc_calc_crc32(const void *data, size_t len)
{
    const uint8_t *buf = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return ~crc;
}

/* 循环接收直到收满 len 字节（0=成功，-1=失败/对端关闭）。
 * macOS 无 MSG_WAITALL，且可移植实现能处理 recv 被 EINTR 打断。 */
static int ipc_recv_full(int fd, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;

    while (got < len) {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n > 0) {
            got += (size_t)n;
            continue;
        }
        if (n == 0) {
            return -1; /* 对端关闭 */
        }
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}

bool g_ipc_initialized = false;

airy_err_t ipc_init(void)
{
    if (g_ipc_initialized)
        return AIRY_SUCCESS;

#ifdef _WIN32
    WSADATA wsa_data;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_err != 0) {
        return AIRY_ERR_SVC_NOT_READY;
    }
#endif

    airy_random_init();
    g_ipc_initialized = true;

    return AIRY_SUCCESS;
}

void ipc_cleanup(void)
{
    if (!g_ipc_initialized)
        return;

#ifdef _WIN32
    WSACleanup();
#endif

    g_ipc_initialized = false;
}

ipc_config_t ipc_create_default_config(ipc_type_t type)
{
    ipc_config_t config = {0};

    config.type = type;
    config.name = "default_ipc";
    config.mode = IPC_MODE_READ_WRITE;
    config.buffer_size = IPC_DEFAULT_BUFFER_SIZE;
    config.max_message_size = IPC_MAX_MESSAGE_SIZE;
    config.timeout_ms = IPC_DEFAULT_TIMEOUT_MS;
    config.max_connections = IPC_MAX_CONNECTIONS;
    config.nonblocking = false;
    config.persistent = false;
    config.permissions = NULL;

    return config;
}

ipc_channel_t *ipc_channel_create(const ipc_config_t *config)
{
    if (!config) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_channel_t *channel = (ipc_channel_t *)AIRY_CALLOC(1, sizeof(ipc_channel_t));
    if (!channel) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    channel->config = *config;
    channel->state = IPC_STATE_CLOSED;
    channel->msg_id_counter = 0;
    AIRY_MEMSET(&channel->stats, 0, sizeof(ipc_stats_t));
    channel->event_cb = NULL;
    channel->event_user_data = NULL;
    channel->msg_cb = NULL;
    channel->msg_user_data = NULL;
    AIRY_MEMSET(channel->error_msg, 0, sizeof(channel->error_msg));

#ifdef _WIN32
    channel->hPipe = INVALID_HANDLE_VALUE;
    channel->hReadEvent = NULL;
    channel->hWriteEvent = NULL;
#else
    channel->fd_read = -1;
    channel->fd_write = -1;
    channel->socket_fd = -1;
#endif
    channel->internal_buffer = NULL;
    channel->buffer_used = 0;

    return channel;
}

void ipc_channel_destroy(ipc_channel_t *channel)
{
    if (!channel) {
        return;
    }

    if (channel->state == IPC_STATE_OPEN || channel->state == IPC_STATE_OPENING) {
        ipc_channel_close(channel);
    }

    AIRY_FREE(channel);
}

airy_err_t ipc_channel_open(ipc_channel_t *channel)
{
    if (!channel) {
        return AIRY_EINVAL;
    }

    if (channel->state != IPC_STATE_CLOSED && channel->state != IPC_STATE_ERROR) {
        snprintf(channel->error_msg, sizeof(channel->error_msg),
                 "Channel already open or in transition");
        return AIRY_EBUSY;
    }

    channel->state = IPC_STATE_OPENING;

    switch (channel->config.type) {
    case IPC_TYPE_PIPE:

#ifdef _WIN32
    {
        SECURITY_ATTRIBUTES sa = {0};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;

        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            snprintf(channel->error_msg, sizeof(channel->error_msg), "CreatePipe failed: %lu",
                     GetLastError());
            channel->state = IPC_STATE_ERROR;
            return AIRY_EUNKNOWN;
        }

        if (channel->config.nonblocking) {
            DWORD mode = PIPE_NOWAIT;
            SetNamedPipeHandleState(hReadPipe, &mode, NULL, NULL);
            SetNamedPipeHandleState(hWritePipe, &mode, NULL, NULL);
        }

        channel->hPipe = hWritePipe;
        channel->hReadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        channel->hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
#else
    {
        int pipefd[2];
        if (pipe(pipefd) != 0) {
            snprintf(channel->error_msg, sizeof(channel->error_msg), "pipe() failed: %s",
                     strerror(errno));
            channel->state = IPC_STATE_ERROR;
            return AIRY_EUNKNOWN;
        }

        channel->fd_read = pipefd[0];
        channel->fd_write = pipefd[1];

        if (channel->config.nonblocking) {
            int flags = fcntl(channel->fd_read, F_GETFL, 0);
            fcntl(channel->fd_read, F_SETFL, flags | O_NONBLOCK);
            flags = fcntl(channel->fd_write, F_GETFL, 0);
            fcntl(channel->fd_write, F_SETFL, flags | O_NONBLOCK);
        }
    }
#endif
    break;

    case IPC_TYPE_NAMED_PIPE:

        break;

    case IPC_TYPE_SOCKET:
        channel->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (channel->socket_fd < 0) {
            snprintf(channel->error_msg, sizeof(channel->error_msg), "Socket creation failed: %s",
                     strerror(errno));
            channel->state = IPC_STATE_ERROR;
            return AIRY_EIO;
        }
        if (channel->config.nonblocking) {
            int flags = fcntl(channel->socket_fd, F_GETFL, 0);
            fcntl(channel->socket_fd, F_SETFL, flags | O_NONBLOCK);
        }
        break;

    case IPC_TYPE_SHM:

        break;

    case IPC_TYPE_MQ:

        break;

    case IPC_TYPE_RPC:

        break;

    default:
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Unknown IPC type: %d",
                 channel->config.type);
        channel->state = IPC_STATE_ERROR;
        return AIRY_EINVAL;
    }

    if (channel->config.buffer_size > 0) {
        channel->internal_buffer = AIRY_MALLOC(channel->config.buffer_size);
        if (!channel->internal_buffer) {
            snprintf(channel->error_msg, sizeof(channel->error_msg),
                     "Failed to allocate internal buffer");

            ipc_channel_close(channel);
            return AIRY_ENOMEM;
        }
    }

    channel->state = IPC_STATE_OPEN;

    if (channel->event_cb) {
        channel->event_cb(channel, IPC_EVENT_CONNECTED, NULL, 0, channel->event_user_data);
    }

    return AIRY_SUCCESS;
}

airy_err_t ipc_channel_close(ipc_channel_t *channel)
{
    if (!channel) {
        return AIRY_EINVAL;
    }

    if (channel->state != IPC_STATE_OPEN) {
        return AIRY_SUCCESS;
    }

    channel->state = IPC_STATE_CLOSING;

    if (channel->event_cb) {
        channel->event_cb(channel, IPC_EVENT_DISCONNECTED, NULL, 0, channel->event_user_data);
    }

#ifdef _WIN32
    if (channel->hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(channel->hPipe);
        channel->hPipe = INVALID_HANDLE_VALUE;
    }
    if (channel->hReadEvent) {
        CloseHandle(channel->hReadEvent);
        channel->hReadEvent = NULL;
    }
    if (channel->hWriteEvent) {
        CloseHandle(channel->hWriteEvent);
        channel->hWriteEvent = NULL;
    }
#else
    if (channel->fd_read >= 0) {
        close(channel->fd_read);
        channel->fd_read = -1;
    }
    if (channel->fd_write >= 0) {
        close(channel->fd_write);
        channel->fd_write = -1;
    }
    if (channel->socket_fd >= 0) {
        close(channel->socket_fd);
        channel->socket_fd = -1;
    }
#endif

    if (channel->internal_buffer) {
        AIRY_FREE(channel->internal_buffer);
        channel->internal_buffer = NULL;
    }
    channel->buffer_used = 0;

    channel->state = IPC_STATE_CLOSED;

    return AIRY_SUCCESS;
}

ipc_state_t ipc_channel_get_state(const ipc_channel_t *channel)
{
    if (!channel) {
        return IPC_STATE_ERROR;
    }
    return channel->state;
}

const char *ipc_channel_get_name(const ipc_channel_t *channel)
{
    if (!channel) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    return channel->config.name;
}

ipc_type_t ipc_channel_get_type(const ipc_channel_t *channel)
{
    if (!channel) {
        return IPC_TYPE_PIPE;
    }
    return channel->config.type;
}

airy_err_t ipc_channel_set_timeout(ipc_channel_t *channel, uint32_t timeout_ms)
{
    if (!channel) {
        return AIRY_EINVAL;
    }

    channel->config.timeout_ms = timeout_ms;
    return AIRY_SUCCESS;
}

airy_err_t ipc_channel_set_event_callback(ipc_channel_t *channel, ipc_event_callback_t callback,
                                          void *user_data)
{
    if (!channel) {
        return AIRY_EINVAL;
    }

    channel->event_cb = callback;
    channel->event_user_data = user_data;

    return AIRY_SUCCESS;
}

airy_err_t ipc_channel_get_stats(const ipc_channel_t *channel, ipc_stats_t *stats)
{
    if (!channel || !stats) {
        return AIRY_EINVAL;
    }

    *stats = channel->stats;
    return AIRY_SUCCESS;
}

airy_err_t ipc_channel_reset_stats(ipc_channel_t *channel)
{
    if (!channel) {
        return AIRY_EINVAL;
    }

    AIRY_MEMSET(&channel->stats, 0, sizeof(ipc_stats_t));
    return AIRY_SUCCESS;
}

airy_err_t ipc_send(ipc_channel_t *channel, const ipc_message_t *message)
{
    if (!channel || !message) {
        return AIRY_EINVAL;
    }

    if (channel->state != IPC_STATE_OPEN) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Channel not open, state=%d",
                 channel->state);
        return AIRY_ENOTCONN;
    }

    if (message->header.payload_len > channel->config.max_message_size) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Message too large: %u > %u",
                 (unsigned int)message->header.payload_len,
                 (unsigned int)channel->config.max_message_size);
        return AIRY_EOVERFLOW;
    }

    size_t total_size = sizeof(ipc_message_header_t) + message->payload_size;
    void *send_buffer = AIRY_MALLOC(total_size);
    if (!send_buffer) {
        return AIRY_ENOMEM;
    }

    __builtin_memcpy(send_buffer, &message->header, sizeof(ipc_message_header_t));

    if (message->payload && message->payload_size > 0) {
        __builtin_memcpy((char *)send_buffer + sizeof(ipc_message_header_t), message->payload,
                         message->payload_size);
    }

#ifdef _WIN32
    DWORD bytes_written = 0;
    BOOL success = FALSE;

    if (channel->hPipe != INVALID_HANDLE_VALUE) {
        size_t remaining = total_size;
        char *ptr = (char *)send_buffer;
        success = TRUE;
        while (remaining > 0) {
            DWORD chunk = (remaining > MAXDWORD) ? MAXDWORD : (DWORD)remaining;
            DWORD chunk_written = 0;
            if (!WriteFile(channel->hPipe, ptr, chunk, &chunk_written, NULL)) {
                success = FALSE;
                break;
            }
            ptr += chunk_written;
            remaining -= chunk_written;
        }
        bytes_written = (DWORD)(total_size - remaining);
    }
#else
    ssize_t bytes_written = 0;
    int fd = (channel->fd_write >= 0) ? channel->fd_write : channel->socket_fd;

    if (fd >= 0) {
        bytes_written = write(fd, send_buffer, total_size);
    }
#endif

    AIRY_FREE(send_buffer);

#ifdef _WIN32
    if (!success || bytes_written < total_size) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Write failed: %lu",
                 GetLastError());
        channel->stats.errors++;
        return AIRY_EUNKNOWN;
    }
#else
    if (bytes_written < 0 || (size_t)bytes_written < total_size) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "write() failed: %s",
                 strerror(errno));
        channel->stats.errors++;
        return AIRY_EUNKNOWN;
    }
#endif

    channel->stats.messages_sent++;
    channel->stats.bytes_sent += bytes_written;

    return AIRY_SUCCESS;
}

airy_err_t ipc_send_data(ipc_channel_t *channel, const void *data, size_t len, size_t *sent)
{
    if (!channel || !data) {
        return AIRY_EINVAL;
    }

    if (len > channel->config.max_message_size) {
        return AIRY_EOVERFLOW;
    }

    ipc_message_t msg = {0};
    msg.header.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.flags = 0;
    msg.header.msg_id = ++channel->msg_id_counter;
    msg.header.payload_len = len;
    msg.header.timestamp = ipc_get_timestamp_ns();
    msg.payload = (void *)data;
    msg.payload_size = len;

    airy_err_t err = ipc_send(channel, &msg);

    if (err == AIRY_SUCCESS && sent) {
        *sent = len;
    }

    return err;
}

airy_err_t ipc_send_request(ipc_channel_t *channel, ipc_message_t *request, ipc_message_t *response,
                            uint32_t timeout_ms)
{
    if (!channel || !request || !response) {
        return AIRY_EINVAL;
    }

    request->header.type = IPC_MSG_REQUEST;
    airy_err_t err = ipc_send(channel, request);
    if (err != AIRY_SUCCESS) {
        return err;
    }

    if (channel->config.type == IPC_TYPE_SOCKET && channel->socket_fd >= 0) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(channel->socket_fd, &readfds);
        int sel = select(channel->socket_fd + 1, &readfds, NULL, NULL, &tv);
        if (sel <= 0) {
            return AIRY_ETIMEDOUT;
        }

        uint32_t net_len = 0;
        if (ipc_recv_full(channel->socket_fd, &net_len, sizeof(net_len)) != 0) {
            return AIRY_EIO;
        }
        uint32_t payload_len = ntohl(net_len);
        if (payload_len > 0 && payload_len <= channel->config.buffer_size) {
            if (!channel->internal_buffer) {
                channel->internal_buffer = AIRY_MALLOC(channel->config.buffer_size);
            }
            if (channel->internal_buffer) {
                size_t want = payload_len > channel->config.buffer_size
                                  ? (size_t)channel->config.buffer_size
                                  : (size_t)payload_len;
                if (ipc_recv_full(channel->socket_fd, channel->internal_buffer, want) == 0) {
                    AIRY_MEMSET(response, 0, sizeof(ipc_message_t));
                    response->header.type = IPC_MSG_RESPONSE;
                    response->header.correlation_id = request->header.msg_id;
                    response->header.payload_len = (uint64_t)want;
                    response->payload = channel->internal_buffer;
                    response->payload_size = (uint64_t)want;
                    channel->stats.messages_received++;
                    return AIRY_SUCCESS;
                }
            }
        }
        return AIRY_EIO;
    }

    AIRY_MEMSET(response, 0, sizeof(ipc_message_t));
    response->header.type = IPC_MSG_RESPONSE;
    response->header.correlation_id = request->header.msg_id;

    channel->stats.messages_received++;

    return AIRY_SUCCESS;
}

airy_err_t ipc_broadcast(ipc_channel_t *channel, const ipc_message_t *message)
{
    if (!channel || !message) {
        return AIRY_EINVAL;
    }

    ipc_message_t broadcast_msg = *message;
    broadcast_msg.header.flags |= IPC_FLAG_BROADCAST;

    return ipc_send(channel, &broadcast_msg);
}

airy_err_t ipc_notify(ipc_channel_t *channel, const void *notification, size_t len)
{
    if (!channel || !notification) {
        return AIRY_EINVAL;
    }

    ipc_message_t msg = {0};
    msg.header.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_NOTIFICATION;
    msg.header.flags = 0;
    msg.header.msg_id = ++channel->msg_id_counter;
    msg.header.timestamp = ipc_get_timestamp_ns();
    msg.payload = (void *)notification;
    msg.payload_size = len;

    return ipc_send(channel, &msg);
}

airy_err_t ipc_receive(ipc_channel_t *channel, ipc_message_t *message, uint32_t timeout_ms)
{
    if (!channel || !message) {
        return AIRY_EINVAL;
    }

    if (channel->state != IPC_STATE_OPEN) {
        return AIRY_ENOTCONN;
    }

    AIRY_MEMSET(message, 0, sizeof(ipc_message_t));

#ifdef _WIN32
    DWORD bytes_read = 0;
    BOOL success = FALSE;

    if (channel->hPipe != INVALID_HANDLE_VALUE) {

        success = ReadFile(channel->hPipe, &message->header, sizeof(ipc_message_header_t),
                           &bytes_read, NULL);

        if (!success || bytes_read < sizeof(ipc_message_header_t)) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                snprintf(channel->error_msg, sizeof(channel->error_msg), "Pipe broken");
            } else {
                snprintf(channel->error_msg, sizeof(channel->error_msg), "ReadFile failed: %lu",
                         GetLastError());
            }
            channel->stats.errors++;
            return AIRY_EUNKNOWN;
        }
    }
#else
    ssize_t bytes_read = 0;
    int fd = (channel->fd_read >= 0) ? channel->fd_read : channel->socket_fd;

    if (fd >= 0) {
        /* Honor timeout_ms: poll before read so an empty channel returns
         * AIRY_ETIMEDOUT instead of blocking forever. */
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, (int)timeout_ms);
        if (pr <= 0) {
            if (pr < 0) {
                snprintf(channel->error_msg, sizeof(channel->error_msg), "poll() failed: %s",
                         strerror(errno));
            } else {
                snprintf(channel->error_msg, sizeof(channel->error_msg), "receive timed out");
            }
            channel->stats.errors++;
            return AIRY_ETIMEDOUT;
        }

        bytes_read = read(fd, &message->header, sizeof(ipc_message_header_t));

        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                snprintf(channel->error_msg, sizeof(channel->error_msg), "EOF - pipe closed");
            } else {
                snprintf(channel->error_msg, sizeof(channel->error_msg), "read() failed: %s",
                         strerror(errno));
            }
            channel->stats.errors++;
            return AIRY_EUNKNOWN;
        }

        if ((size_t)bytes_read < sizeof(ipc_message_header_t)) {

            snprintf(channel->error_msg, sizeof(channel->error_msg),
                     "Incomplete header: got %zd bytes", bytes_read);
            return AIRY_EINVAL;
        }
    }
#endif

    if (message->header.magic != IPC_MAGIC) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Invalid magic: 0x%08X",
                 message->header.magic);
        channel->stats.errors++;
        return AIRY_EINVAL;
    }

    if (message->header.payload_len > 0 &&
        message->header.payload_len <= channel->config.max_message_size) {

        message->payload = AIRY_MALLOC(message->header.payload_len);
        if (!message->payload) {
            return AIRY_ENOMEM;
        }

#ifdef _WIN32
        DWORD payload_read = 0;
        if (channel->hPipe != INVALID_HANDLE_VALUE) {
            success = ReadFile(channel->hPipe, message->payload, message->header.payload_len,
                               &payload_read, NULL);

            if (!success || payload_read < message->header.payload_len) {
                AIRY_FREE(message->payload);
                message->payload = NULL;
                channel->stats.errors++;
                return AIRY_EUNKNOWN;
            }
        }
#else
        ssize_t payload_read = 0;
        if (fd >= 0) {
            payload_read = read(fd, message->payload, message->header.payload_len);

            if (payload_read <= 0 || (size_t)payload_read < message->header.payload_len) {
                AIRY_FREE(message->payload);
                message->payload = NULL;
                channel->stats.errors++;
                return AIRY_EUNKNOWN;
            }
        }
#endif

        message->payload_size = message->header.payload_len;
    }

    if (channel->msg_cb) {
        int result = channel->msg_cb(channel, message, channel->msg_user_data);
        if (result != 0) {

            return AIRY_ECANCELLED;
        }
    }

    channel->stats.messages_received++;
    channel->stats.bytes_received += sizeof(ipc_message_header_t) + message->payload_size;

    return AIRY_SUCCESS;
}

airy_err_t ipc_receive_data(ipc_channel_t *channel, void *buffer, size_t len, size_t *received)
{
    if (!channel || !buffer) {
        return AIRY_EINVAL;
    }

    ipc_message_t msg;
    airy_err_t err = ipc_receive(channel, &msg, channel->config.timeout_ms);
    if (err != AIRY_SUCCESS) {
        return err;
    }

    size_t copy_len = (msg.payload_size < len) ? msg.payload_size : len;
    if (copy_len > 0 && msg.payload) {
        __builtin_memcpy(buffer, msg.payload, copy_len);
    }

    if (received) {
        *received = copy_len;
    }

    return AIRY_SUCCESS;
}

airy_err_t ipc_try_receive(ipc_channel_t *channel, ipc_message_t *message)
{
    if (!channel || !message) {
        return AIRY_EINVAL;
    }

    airy_err_t err = ipc_receive(channel, message, 0);
    if (err == AIRY_ETIMEDOUT) {
        return AIRY_EBUSY;
    }

    return err;
}

airy_err_t ipc_set_message_callback(ipc_channel_t *channel, ipc_message_callback_t callback,
                                    void *user_data)
{
    if (!channel) {
        return AIRY_EINVAL;
    }

    channel->msg_cb = callback;
    channel->msg_user_data = user_data;

    return AIRY_SUCCESS;
}

const char *ipc_get_error_message(const ipc_channel_t *channel)
{
    if (!channel) {
        return "Invalid channel handle";
    }
    return channel->error_msg[0] ? channel->error_msg : "No error";
}

bool ipc_is_valid(const ipc_channel_t *channel)
{
    if (!channel) {
        return false;
    }

    return channel->state == IPC_STATE_OPEN;
}

airy_err_t ipc_flush(ipc_channel_t *channel)
{
    if (!channel) {
        return AIRY_EINVAL;
    }

#ifdef _WIN32
    if (channel->hPipe != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(channel->hPipe);
    }
#else
    int fd = (channel->fd_write >= 0) ? channel->fd_write : channel->socket_fd;
    if (fd >= 0) {
        fsync(fd);
    }
#endif

    return AIRY_SUCCESS;
}
