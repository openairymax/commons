// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file daemon_rpc_req.c
 * @brief Daemon JSON-RPC 2.0 request wire-format builder.
 *
 * 0.1.19 §80: 请求构建机制件下沉 commons。socket 路径（daemons
 * daemon_rpc_client.c）与 L2 桥（daemon_l2_bridge.c）分属不同静态库且
 * svc_common → daemon_l1_server 单向，机制件须居两者公共下游；权威头
 * daemon_rpc_client.h 已在 commons/utils/ipc（P0.17 先例），实现随之
 * 归位，符号经 airy_common PUBLIC 链接提供。
 */

#include "airy_memory.h"
#include "error.h"

#include "daemon_rpc_client.h"

#include <cjson/cJSON.h>

char *daemon_rpc_json_req(const char *method, const char *params_json)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", method);
    if (params_json && params_json[0] != '\0') {
        cJSON *params = cJSON_Parse(params_json);
        if (params) {
            cJSON_AddItemToObject(root, "params", params);
        } else {
            cJSON_AddStringToObject(root, "params", params_json);
        }
    } else {
        cJSON_AddObjectToObject(root, "params");
    }
    cJSON_AddNumberToObject(root, "id", 1);
    char *request_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return request_str;
}
