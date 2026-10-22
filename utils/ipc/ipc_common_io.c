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

static bool ipc_push(ipc_channel_t *channel, const void *buffer, size_t len)
{
#ifdef _WIN32
    size_t left = len;
    const char *ptr = (const char *)buffer;

    if (channel->hPipe != INVALID_HANDLE_VALUE) {
        while (left > 0) {
            DWORD chunk = (left > MAXDWORD) ? MAXDWORD : (DWORD)left;
            DWORD written = 0;
            if (!WriteFile(channel->hPipe, ptr, chunk, &written, NULL) || written == 0)
                break;
            ptr += written;
            left -= written;
        }
        if (left == 0)
            return true;
    }
    snprintf(channel->error_msg, sizeof(channel->error_msg), "Write failed: %lu", GetLastError());
    return false;
#else
    int fd = (channel->fd_write >= 0) ? channel->fd_write : channel->socket_fd;
    ssize_t written;

    if (fd >= 0) {
        written = write(fd, buffer, len);
        if (written >= 0 && (size_t)written == len)
            return true;
    }
    snprintf(channel->error_msg, sizeof(channel->error_msg), "write() failed: %s",
             strerror(errno));
    return false;
#endif
}

airy_err_t ipc_send(ipc_channel_t *channel, const ipc_message_t *message)
{
    if (!channel || !message)
        return AIRY_EINVAL;

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
    if (!send_buffer)
        return AIRY_ENOMEM;

    __builtin_memcpy(send_buffer, &message->header, sizeof(ipc_message_header_t));

    if (message->payload && message->payload_size > 0) {
        __builtin_memcpy((char *)send_buffer + sizeof(ipc_message_header_t), message->payload,
                         message->payload_size);
    }

    bool sent = ipc_push(channel, send_buffer, total_size);

    AIRY_FREE(send_buffer);

    if (!sent) {
        channel->stats.errors++;
        return AIRY_EUNKNOWN;
    }

    channel->stats.messages_sent++;
    channel->stats.bytes_sent += total_size;

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
    ipc_msg_setup(&msg, IPC_MSG_DATA, ++channel->msg_id_counter, len);
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

    /* P0-4: receive with the same frame format ipc_send emits (128-byte
     * header + payload). The previous socket branch parsed a 4-byte
     * length prefix that ipc_send never writes, mis-framing every
     * response; the non-socket branch returned success without
     * receiving anything. */
    return ipc_receive(channel, response, timeout_ms);
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
    ipc_msg_setup(&msg, IPC_MSG_NOTIFICATION, ++channel->msg_id_counter, len);
    msg.payload = (void *)notification;
    msg.payload_size = len;

    return ipc_send(channel, &msg);
}

static airy_err_t ipc_read_head(ipc_channel_t *channel, ipc_message_t *message, uint32_t timeout_ms)
{
#ifdef _WIN32
    DWORD bytes_read = 0;
    if (channel->hPipe == INVALID_HANDLE_VALUE)
        return AIRY_SUCCESS;
    if (ReadFile(channel->hPipe, &message->header, sizeof(ipc_message_header_t), &bytes_read,
                 NULL) &&
        bytes_read >= sizeof(ipc_message_header_t))
        return AIRY_SUCCESS;
    if (GetLastError() == ERROR_BROKEN_PIPE) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Pipe broken");
    } else {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "ReadFile failed: %lu",
                 GetLastError());
    }
    channel->stats.errors++;
    return AIRY_EUNKNOWN;
#else
    int fd = (channel->fd_read >= 0) ? channel->fd_read : channel->socket_fd;
    struct pollfd pfd;
    ssize_t bytes_read;
    if (fd < 0)
        return AIRY_SUCCESS;
    /* Honor timeout_ms: poll before read so an empty channel returns
     * AIRY_ETIMEDOUT instead of blocking forever. */
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
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Incomplete header: got %zd bytes",
                 bytes_read);
        return AIRY_EINVAL;
    }
    return AIRY_SUCCESS;
#endif
}

static airy_err_t ipc_check_head(ipc_channel_t *channel, const ipc_message_t *message)
{
    if (message->header.aipc.magic != IPC_MAGIC) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Invalid magic: 0x%08X",
                 message->header.aipc.magic);
        channel->stats.errors++;
        return AIRY_EINVAL;
    }
    if (message->header.aipc.payload_len > channel->config.max_message_size) {
        /* T-23: fail loudly on oversized payloads. Silently skipping the
         * payload used to leave its bytes in the stream, permanently
         * desynchronizing the frame parser. */
        snprintf(channel->error_msg, sizeof(channel->error_msg), "Payload too large: %u > %u",
                 message->header.aipc.payload_len,
                 (unsigned int)channel->config.max_message_size);
        channel->stats.errors++;
        return AIRY_EOVERFLOW;
    }
    return AIRY_SUCCESS;
}

static bool ipc_fill(ipc_channel_t *channel, void *buffer, size_t len)
{
#ifdef _WIN32
    DWORD got = 0;
    if (channel->hPipe == INVALID_HANDLE_VALUE)
        return true;
    return ReadFile(channel->hPipe, buffer, (DWORD)len, &got, NULL) && got >= (DWORD)len;
#else
    int fd = (channel->fd_read >= 0) ? channel->fd_read : channel->socket_fd;
    ssize_t got = (fd < 0) ? 0 : read(fd, buffer, len);

    return fd < 0 || (got > 0 && (size_t)got >= len);
#endif
}

static airy_err_t ipc_read_body(ipc_channel_t *channel, ipc_message_t *message)
{
    uint32_t len = message->header.aipc.payload_len;
    message->payload = AIRY_MALLOC(len);
    if (!message->payload)
        return AIRY_ENOMEM;
    if (!ipc_fill(channel, message->payload, len)) {
        AIRY_FREE(message->payload);
        message->payload = NULL;
        channel->stats.errors++;
        return AIRY_EUNKNOWN;
    }
    message->payload_size = len;
    return AIRY_SUCCESS;
}

airy_err_t ipc_receive(ipc_channel_t *channel, ipc_message_t *message, uint32_t timeout_ms)
{
    if (!channel || !message)
        return AIRY_EINVAL;
    if (channel->state != IPC_STATE_OPEN)
        return AIRY_ENOTCONN;

    AIRY_MEMSET(message, 0, sizeof(ipc_message_t));

    airy_err_t err = ipc_read_head(channel, message, timeout_ms);
    if (err == AIRY_SUCCESS)
        err = ipc_check_head(channel, message);
    if (err == AIRY_SUCCESS && message->header.aipc.payload_len > 0)
        err = ipc_read_body(channel, message);
    if (err != AIRY_SUCCESS)
        return err;

    if (channel->msg_cb && channel->msg_cb(channel, message, channel->msg_user_data) != 0)
        return AIRY_ECANCELLED;

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
