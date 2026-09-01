// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file ipc_common.c
 * @brief IPC module - init/cleanup and channel lifecycle basics.
 *
 * Implements the IPC core functionality declared in ipc_common.h:
 * - Init/cleanup and default config
 * - Channel lifecycle management (create/destroy/open/close/state/type/
 *   timeout/callback/stats)
 * - Module-wide shared helpers (timestamp/crc32, declared in
 *   ipc_common_internal.h)
 *
 * Message send/receive transports are split into ipc_common_io.c;
 * server/client, shared memory, message queue and RPC frameworks are
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
 * @see agentrt/commons/utils/ipc/ipc_common_io.c
 */

#include "ipc_common.h"

#include "platform.h"
#include "string_compat.h"

#include "ipc_common_internal.h"

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
            /* P2-2：F_GETFL 失败返回 -1，直接 F_SETFL 会把 fd 所有
             * 标志位误置为全 1；仅在读取成功时设置 O_NONBLOCK。 */
            int flags = fcntl(channel->fd_read, F_GETFL, 0);
            if (flags >= 0)
                fcntl(channel->fd_read, F_SETFL, flags | O_NONBLOCK);
            flags = fcntl(channel->fd_write, F_GETFL, 0);
            if (flags >= 0)
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
            /* P2-2：F_GETFL 失败时 flags=-1，F_SETFL 会破坏 fd 标志位 */
            int flags = fcntl(channel->socket_fd, F_GETFL, 0);
            if (flags >= 0)
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
