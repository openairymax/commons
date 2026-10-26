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

#endif /* AIRY_RT_IPC_UTIL_API_H */
