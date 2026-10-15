// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file jsonrpc_helpers.c
 * @brief JSON-RPC 2.0 common helper function implementation.
 *
 * 0.1.9 0c (IRON-6 归位)：实现从 daemons/common/src/util/ 迁入 commons/utils/ipc。
 * 0.1.19 (u76)：JSON-RPC 响应构建母件归一——gateway/src/utils/jsonrpc.c
 * 的 create_success/create_error（cJSON 通用 id 版）下沉至此，build_error/
 * build_success 降为 int-id 薄包装，严格校验 validate_strict 一并归位。
 */

#include "airy_memory.h"
#include "error.h"

#include "jsonrpc_helpers.h"

#include "svc_logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* echo the request id into a response envelope */
static void jsonrpc_add_id(cJSON *response, const cJSON *id)
{
    if (id)
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    else
        cJSON_AddNullToObject(response, "id");
}

char *jsonrpc_create_success_response(const cJSON *id, cJSON *result)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        if (result)
            cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");

    if (result) {
        cJSON_AddItemToObject(response, "result", result);
    } else {
        cJSON_AddNullToObject(response, "result");
    }

    jsonrpc_add_id(response, id);

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    return json_str;
}

char *jsonrpc_create_error_response(const cJSON *id, int code, const char *message, cJSON *data)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        if (data)
            cJSON_Delete(data);
        return NULL;
    }

    cJSON *error = cJSON_CreateObject();
    if (!error) {
        cJSON_Delete(response);
        if (data)
            cJSON_Delete(data);
        return NULL;
    }

    cJSON_AddNumberToObject(error, "code", code);

    const char *msg = message;
    if (!msg)
        msg = jsonrpc_get_error_message(code);
    cJSON_AddStringToObject(error, "message", msg ? msg : "Internal error");

    if (data)
        cJSON_AddItemToObject(error, "data", data);

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddItemToObject(response, "error", error);

    jsonrpc_add_id(response, id);

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    return json_str;
}

char *jsonrpc_build_error(int code, const char *message, int id)
{
    cJSON *id_json = cJSON_CreateNumber(id);
    if (!id_json)
        AIRY_ERROR_NULL(AIRY_ERR_OUT_OF_MEMORY, "cJSON_CreateNumber failed");

    return jsonrpc_create_error_response(id_json, code, message, NULL);
}

char *jsonrpc_build_success(cJSON *result, int id)
{
    cJSON *id_json = cJSON_CreateNumber(id);
    if (!id_json) {
        if (result)
            cJSON_Delete(result);
        AIRY_ERROR_NULL(AIRY_ERR_OUT_OF_MEMORY, "cJSON_CreateNumber failed");
    }

    return jsonrpc_create_success_response(id_json, result);
}

int jsonrpc_parse_request_ptr(cJSON *req, char **out_method, cJSON **out_params, int *out_id)
{
    if (!req || !out_method || !out_params || !out_id)
        return AIRY_ERR_INVALID_PARAM;

    *out_method = NULL;
    *out_params = NULL;
    *out_id = 0;

    if (jsonrpc_validate_request(req) != 0)
        return JSONRPC_INVALID_REQUEST;

    cJSON *method_obj = cJSON_GetObjectItem(req, "method");
    if (method_obj && cJSON_IsString(method_obj)) {
        *out_method = AIRY_STRDUP(method_obj->valuestring);
    }

    cJSON *params_obj = cJSON_GetObjectItem(req, "params");
    if (params_obj) {
        *out_params = cJSON_Duplicate(params_obj, 1);
    }

    cJSON *id_obj = cJSON_GetObjectItem(req, "id");
    if (id_obj && cJSON_IsNumber(id_obj)) {
        *out_id = id_obj->valueint;
    }

    return 0;
}

int jsonrpc_validate_request(cJSON *req)
{
    if (!req)
        return AIRY_ERR_INVALID_PARAM;

    cJSON *jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0)
        return AIRY_ERR_PARSE_ERROR;

    cJSON *method = cJSON_GetObjectItem(req, "method");
    if (!method || !cJSON_IsString(method))
        return AIRY_ERR_PARSE_ERROR;

    return 0;
}

/* Strict variant: also requires an id field (number|string|null), per
 * JSON-RPC 2.0 request semantics. Notification frames (no id) must go
 * through jsonrpc_validate_request() instead. */
int jsonrpc_check_strict(const cJSON *req)
{
    if (!req)
        return AIRY_ERR_NULL_POINTER;

    if (!cJSON_HasObjectItem(req, "jsonrpc") || !cJSON_HasObjectItem(req, "method") ||
        !cJSON_HasObjectItem(req, "id"))
        return AIRY_ERR_PARSE_ERROR;

    const cJSON *jsonrpc = cJSON_GetObjectItemCaseSensitive(req, "jsonrpc");
    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0)
        return AIRY_ERR_PARSE_ERROR;

    const cJSON *method = cJSON_GetObjectItemCaseSensitive(req, "method");
    if (!cJSON_IsString(method) || strlen(method->valuestring) == 0)
        return AIRY_ERR_PARSE_ERROR;

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(req, "id");
    if (!cJSON_IsNumber(id) && !cJSON_IsString(id) && !cJSON_IsNull(id))
        return AIRY_ERR_PARSE_ERROR;

    return 0;
}

const char *jsonrpc_get_string_param(cJSON *params, const char *key, const char *default_value)
{
    if (!params || !key)
        return default_value;
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsString(item))
        return default_value;
    return item->valuestring;
}

cJSON *jsonrpc_get_object_param(cJSON *params, const char *key)
{
    if (!params || !key) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "jsonrpc_get_object_param: null parameter");
    }
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsObject(item)) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "jsonrpc_get_object_param: not an object");
    }
    return item;
}

char *jsonrpc_build_notification(const char *method, cJSON *params)
{
    if (!method) {
        if (params)
            cJSON_Delete(params);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", method);
    if (params)
        cJSON_AddItemToObject(root, "params", params);

    char *resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return resp;
}

const char *jsonrpc_get_error_message(int code)
{
    switch (code) {

    case JSONRPC_PARSE_ERROR:
        return "Parse error";
    case JSONRPC_INVALID_REQUEST:
        return "Invalid request";
    case JSONRPC_METHOD_NOT_FOUND:
        return "Method not found";
    case JSONRPC_INVALID_PARAMS:
        return "Invalid params";
    case JSONRPC_INTERNAL_ERROR:
        return "Internal error";

    case JSONRPC_RATE_LIMITED:
        return "Rate limit exceeded";
    case JSONRPC_AUTH_FAILED:
        return "Authentication failed";
    case JSONRPC_SESSION_EXPIRED:
        return "Session expired";
    case JSONRPC_SERVICE_UNAVAILABLE:
        return "Service unavailable";
    default:
        return "Unknown error";
    }
}

/* JSONRPC_SEND_ERROR / JSONRPC_SEND_SUCCESS funnel every serialized
 * response through jsonrpc_route_response(), which writes it to the
 * client socket. */
int jsonrpc_route_response(airy_sock_t socket, const char *str, size_t len)
{
    if (!str || len == 0)
        return AIRY_ERR_INVALID_PARAM;

    return (airy_sock_send(socket, str, len) == (ssize_t)len) ? AIRY_SUCCESS
                                                              : AIRY_ERR_GENERIC_FAIL;
}
