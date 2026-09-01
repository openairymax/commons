// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file ipc_common_io.c
 * @brief IPC module - message send/receive transports.
 *
 * Phase 2.3a split from ipc_common.c: all message transport paths:
 * - send / send-data / request-response / broadcast / notify
 * - receive / receive-data / try-receive + message callback
 * - error message / validity check / flush
 *
 * Channel lifecycle (create/destroy/open/close/state/accessors) and the
 * module-wide shared helpers stay in ipc_common.c; module-internal shared
 * definitions are declared in ipc_common_internal.h.
 *
 * Following ARCHITECTURAL_PRINCIPLES.md design principles:
 * - E-4 Cross-platform consistency: Windows/Linux/macOS
 * - E-5 Semantic naming: every function name states its purpose
 * - E-6 Traceable errors: unified error code system
 *
 * @see agentrt/commons/utils/ipc/ipc_common.c
 */

#include "ipc_common.h"

#include "platform.h"
#include "string_compat.h"

#include "ipc_common_internal.h"

#ifndef _WIN32
#include <errno.h>
#include <poll.h>
#endif

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

    if (message->header.aipc.payload_len > channel->config.max_message_size) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Message too large: %u > %u",
                 (unsigned int)message->header.aipc.payload_len,
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
    msg.header.aipc.magic = IPC_MAGIC; /* [SC] 128B 头 magic */
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.flags = 0;
    msg.header.msg_id = ++channel->msg_id_counter;
    msg.header.aipc.payload_len = (uint32_t)len;
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
                    response->header.aipc.payload_len = (uint32_t)want;
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
    msg.header.aipc.magic = IPC_MAGIC; /* [SC] 128B 头 magic */
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

    if (message->header.aipc.magic != IPC_MAGIC) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Invalid magic: 0x%08X",
                 message->header.aipc.magic);
        channel->stats.errors++;
        return AIRY_EINVAL;
    }

    if (message->header.aipc.payload_len > 0 &&
        message->header.aipc.payload_len <= channel->config.max_message_size) {

        message->payload = AIRY_MALLOC(message->header.aipc.payload_len);
        if (!message->payload) {
            return AIRY_ENOMEM;
        }

#ifdef _WIN32
        DWORD payload_read = 0;
        if (channel->hPipe != INVALID_HANDLE_VALUE) {
            success = ReadFile(channel->hPipe, message->payload,
                               (DWORD)message->header.aipc.payload_len, &payload_read, NULL);

            if (!success || payload_read < message->header.aipc.payload_len) {
                AIRY_FREE(message->payload);
                message->payload = NULL;
                channel->stats.errors++;
                return AIRY_EUNKNOWN;
            }
        }
#else
        ssize_t payload_read = 0;
        if (fd >= 0) {
            payload_read = read(fd, message->payload, message->header.aipc.payload_len);

            if (payload_read <= 0 || (size_t)payload_read < message->header.aipc.payload_len) {
                AIRY_FREE(message->payload);
                message->payload = NULL;
                channel->stats.errors++;
                return AIRY_EUNKNOWN;
            }
        }
#endif

        message->payload_size = message->header.aipc.payload_len;
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
