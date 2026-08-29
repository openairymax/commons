/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC utilities: is_valid/flush + type conversion helpers.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_UTIL_API_H
#define AIRY_RT_IPC_UTIL_API_H

#include "ipc_types.h"

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

#endif /* AIRY_RT_IPC_UTIL_API_H */
