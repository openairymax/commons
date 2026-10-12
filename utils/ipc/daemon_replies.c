// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file daemon_replies.c
 * @brief Daemon JSON-RPC 回信机制件（health 信封 / stats 回信管道）。
 *
 * 0.1.19 §333 错置件消解取一：tool_rpc.c 与 cupolas_rpc_core.c 的
 * health/stats handler 委托本机制件回信，行为零漂移（信封字段、
 * 错误文案、时间戳公式逐字保留）。
 */

#include "airy_memory.h"
#include "error.h"

#include "daemon_replies.h"
#include "jsonrpc_helpers.h"

#include <cjson/cJSON.h>
#include <time.h>

void reply_health(airy_sock_t socket, const char *service, int healthy, int id)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        JSONRPC_SEND_ERROR(socket, JSONRPC_INTERNAL_ERROR, "Health check failed", id);
        return;
    }
    cJSON_AddStringToObject(result, "service", service);
    cJSON_AddBoolToObject(result, "healthy", healthy);
    cJSON_AddNumberToObject(result, "timestamp", (double)(uint64_t)time(NULL) * 1000);
    JSONRPC_SEND_SUCCESS(socket, result, id);
}

void reply_stats(airy_sock_t socket, char *stats_json, int id)
{
    if (!stats_json) {
        JSONRPC_SEND_ERROR(socket, JSONRPC_INTERNAL_ERROR, "Failed to collect stats", id);
        return;
    }
    cJSON *result = cJSON_Parse(stats_json);
    AIRY_FREE(stats_json);
    if (!result) {
        JSONRPC_SEND_ERROR(socket, JSONRPC_INTERNAL_ERROR, "Stats serialization failed", id);
        return;
    }
    JSONRPC_SEND_SUCCESS(socket, result, id);
}
