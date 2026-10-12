/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file daemon_replies.h
 * @brief Daemon JSON-RPC 回信机制件（health 信封 / stats 回信管道）。
 *
 * 0.1.19 §333 错置件消解：tool_d 与 cupolas_d 的 health 信封 /
 * stats 回信为跨层同形样板（文案与信封逐字相同），按"跨层重复实现
 * 取一"归位 commons/utils/ipc，与 jsonrpc_helpers.h、daemon_rpc_client.h
 * 同域。信封字段与错误文案自此单一事实源。
 */

#ifndef AIRY_RT_DAEMON_REPLIES_H
#define AIRY_RT_DAEMON_REPLIES_H

#include <compat.h>

#include "airy_memory.h"
#include "daemon_platform_ext.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send the standard daemon health envelope (build+send in one).
 * @param socket Client socket descriptor
 * @param service Service name written to the "service" field
 * @param healthy Non-zero when the backing service is ready
 * @param id Request ID
 * @note Envelope: {"service":...,"healthy":...,"timestamp":epoch_ms};
 *       the timestamp formula (seconds * 1000) is single-sourced here.
 * @note On envelope OOM replies INTERNAL_ERROR "Health check failed"
 *       (fail-closed) instead of a silent no-response.
 */
void reply_health(airy_sock_t socket, const char *service, int healthy, int id);

/**
 * @brief Parse serialized stats and send them as the RPC result.
 * @param socket Client socket descriptor
 * @param stats_json Serialized stats; ownership moves here and the
 *        buffer is AIRY_FREEd on every path
 * @param id Request ID
 * @note Replies INTERNAL_ERROR "Failed to collect stats" when
 *       @p stats_json is NULL and "Stats serialization failed" when
 *       parsing fails; success carries the parsed object.
 */
void reply_stats(airy_sock_t socket, char *stats_json, int id);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_DAEMON_REPLIES_H */
