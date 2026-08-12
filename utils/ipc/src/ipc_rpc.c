// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file ipc_rpc.c
 * @brief 进程间通信模块 - RPC 远程过程调用实现
 *
 * @details
 * 本文件实现了 ipc_common.h 中声明的 RPC 框架 API：
 * - 服务端：创建/销毁/启动/停止/注册方法/查找方法/处理请求
 * - 客户端：创建/销毁/同步调用
 *
 * 请求/响应协议基于 ipc_channel_t 传输通道承载的 ipc_message_t 消息：
 * - 请求负载 = 方法名字符串 + '\0' + 请求体
 * - 响应负载 = ipc_rpc_header_t（含状态码）+ 响应体
 *
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的设计原则：
 * - E-5 命名语义化：所有函数名精确表达用途
 * - E-6 错误可追溯：统一的错误码体系
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-08-11
 * @version 1.0
 *
 * @see ipc_common_internal.h 内部共享定义
 */

#include "ipc_common_internal.h"

#include "ipc_common.h"

static rpc_method_node_t *rpc_find_method_node(ipc_rpc_server_t *server, const char *name)
{
    rpc_method_node_t *node = server->methods;
    while (node) {
        if (strcmp(node->method_name, name) == 0)
            return node;
        node = node->next;
    }
    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
}

ipc_rpc_server_t *ipc_rpc_server_create(const ipc_rpc_server_config_t *config)
{
    if (!config || !config->transport) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_rpc_server_t *server = (ipc_rpc_server_t *)AIRY_CALLOC(1, sizeof(ipc_rpc_server_t));
    if (!server) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    server->transport = config->transport;
    server->max_request_size =
        config->max_request_size > 0 ? config->max_request_size : (64 * 1024);
    server->max_response_size =
        config->max_response_size > 0 ? config->max_response_size : (64 * 1024);

    if (config->service_name) {
        server->service_name = AIRY_STRDUP(config->service_name);
    }

    for (size_t i = 0; i < config->method_count; i++) {
        rpc_method_node_t *node = (rpc_method_node_t *)AIRY_CALLOC(1, sizeof(rpc_method_node_t));
        if (!node)
            continue;
        node->method_name = AIRY_STRDUP(config->methods[i].method_name);
        node->handler = config->methods[i].handler;
        node->user_data = config->methods[i].user_data;
        node->next = server->methods;
        server->methods = node;
        server->method_count++;
    }

    return server;
}

void ipc_rpc_server_destroy(ipc_rpc_server_t *server)
{
    if (!server)
        return;

    rpc_method_node_t *node = server->methods;
    while (node) {
        rpc_method_node_t *next = node->next;
        AIRY_FREE(node->method_name);
        AIRY_FREE(node);
        node = next;
    }

    AIRY_FREE(server->service_name);
    AIRY_FREE(server);
}

airy_err_t ipc_rpc_server_start(ipc_rpc_server_t *server)
{
    if (!server)
        return AIRY_EINVAL;
    if (server->running)
        return AIRY_EBUSY;
    server->running = true;
    return AIRY_SUCCESS;
}

airy_err_t ipc_rpc_server_stop(ipc_rpc_server_t *server)
{
    if (!server)
        return AIRY_EINVAL;
    server->running = false;
    return AIRY_SUCCESS;
}

airy_err_t ipc_rpc_server_register_method(ipc_rpc_server_t *server, const ipc_rpc_method_t *method)
{
    if (!server || !method || !method->method_name || !method->handler)
        return AIRY_EINVAL;

    rpc_method_node_t *existing = rpc_find_method_node(server, method->method_name);
    if (existing) {
        existing->handler = method->handler;
        existing->user_data = method->user_data;
        return AIRY_SUCCESS;
    }

    rpc_method_node_t *node = (rpc_method_node_t *)AIRY_CALLOC(1, sizeof(rpc_method_node_t));
    if (!node)
        return AIRY_ENOMEM;

    node->method_name = AIRY_STRDUP(method->method_name);
    node->handler = method->handler;
    node->user_data = method->user_data;
    node->next = server->methods;
    server->methods = node;
    server->method_count++;

    return AIRY_SUCCESS;
}

rpc_method_handler_t ipc_rpc_server_find_method(ipc_rpc_server_t *server, const char *method_name)
{
    if (!server || !method_name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    rpc_method_node_t *node = rpc_find_method_node(server, method_name);
    return node ? node->handler : NULL;
}

airy_err_t ipc_rpc_server_process(ipc_rpc_server_t *server, uint32_t timeout_ms)
{
    if (!server || !server->running || !server->transport)
        return AIRY_EINVAL;
    if (ipc_channel_get_state(server->transport) != IPC_STATE_OPEN)
        return AIRY_ENOTCONN;

    ipc_message_t msg = {0};
    airy_err_t err = ipc_receive(server->transport, &msg, timeout_ms);
    if (err != AIRY_SUCCESS)
        return err;

    if (msg.header.magic != IPC_MAGIC) {
        ipc_message_free(&msg);
        return AIRY_EINVAL;
    }

    if (msg.payload == NULL || msg.payload_size == 0) {
        ipc_message_free(&msg);
        return AIRY_EINVAL;
    }

    char *method_name = (char *)msg.payload;
    size_t name_len = strnlen(method_name, msg.payload_size);
    if (name_len >= msg.payload_size) {
        ipc_message_free(&msg);
        return AIRY_EINVAL;
    }

    void *request_payload = (char *)msg.payload + name_len + 1;
    size_t request_len = msg.payload_size - name_len - 1;

    rpc_method_node_t *node = rpc_find_method_node(server, method_name);
    if (!node) {

        ipc_rpc_header_t rsp_hdr = {0};
        rsp_hdr.magic = IPC_RPC_MAGIC;
        rsp_hdr.version = 1;
        rsp_hdr.request_id = msg.header.msg_id;
        rsp_hdr.status = 404; /* Method not found */
        snprintf(rsp_hdr.method_name, sizeof(rsp_hdr.method_name), "ERROR: method '%s' not found",
                 method_name);

        ipc_message_t rsp_msg = {0};
        rsp_msg.header = msg.header;
        rsp_msg.header.type = IPC_MSG_RESPONSE;
        rsp_msg.header.payload_len = sizeof(rsp_hdr);
        rsp_msg.payload = &rsp_hdr;
        rsp_msg.payload_size = sizeof(rsp_hdr);

        ipc_send(server->transport, &rsp_msg);
        ipc_message_free(&msg);
        return AIRY_ENOENT;
    }

    size_t response_max = server->max_response_size;
    void *response_buf = AIRY_CALLOC(1, response_max);
    if (!response_buf) {
        ipc_message_free(&msg);
        return AIRY_ENOMEM;
    }

    size_t response_len = 0;
    airy_err_t handler_err =
        node->handler(request_payload, request_len, response_buf, &response_len, node->user_data);

    ipc_rpc_header_t rsp_hdr = {0};
    rsp_hdr.magic = IPC_RPC_MAGIC;
    rsp_hdr.version = 1;
    rsp_hdr.request_id = msg.header.msg_id;
    rsp_hdr.status = (handler_err == AIRY_SUCCESS) ? 0 : (uint32_t)handler_err;
    rsp_hdr.method_name_len = (uint32_t)strlen(method_name);
    rsp_hdr.payload_len = response_len;
    __builtin_memcpy(rsp_hdr.method_name, method_name, strlen(method_name));

    ipc_message_t rsp_msg = {0};
    rsp_msg.header = msg.header;
    rsp_msg.header.type = IPC_MSG_RESPONSE;
    rsp_msg.header.payload_len = sizeof(rsp_hdr) + response_len;

    size_t total_payload = sizeof(rsp_hdr) + response_len;
    void *combined_payload = AIRY_MALLOC(total_payload);
    if (combined_payload) {
        __builtin_memcpy(combined_payload, &rsp_hdr, sizeof(rsp_hdr));
        if (response_len > 0) {
            __builtin_memcpy((char *)combined_payload + sizeof(rsp_hdr), response_buf,
                             response_len);
        }
        rsp_msg.payload = combined_payload;
        rsp_msg.payload_size = total_payload;
        ipc_send(server->transport, &rsp_msg);
        AIRY_FREE(combined_payload);
    }

    AIRY_FREE(response_buf);
    ipc_message_free(&msg);
    return AIRY_SUCCESS;
}

ipc_rpc_client_t *ipc_rpc_client_create(const ipc_rpc_client_config_t *config)
{
    if (!config || !config->transport) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_rpc_client_t *client = (ipc_rpc_client_t *)AIRY_CALLOC(1, sizeof(ipc_rpc_client_t));
    if (!client) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    client->transport = config->transport;
    client->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : IPC_DEFAULT_TIMEOUT_MS;

    return client;
}

void ipc_rpc_client_destroy(ipc_rpc_client_t *client)
{
    if (!client)
        return;
    AIRY_FREE(client);
}

airy_err_t ipc_rpc_call_sync(ipc_rpc_client_t *client, const char *method_name, const void *request,
                             size_t request_len, void *response, size_t response_max,
                             size_t *response_len)
{
    if (!client || !method_name || !request)
        return AIRY_EINVAL;
    if (ipc_channel_get_state(client->transport) != IPC_STATE_OPEN)
        return AIRY_ENOTCONN;

    size_t name_len = strlen(method_name);
    size_t total_payload = name_len + 1 + request_len;

    if (total_payload > UINT32_MAX)
        return AIRY_EOVERFLOW;

    void *request_buf = AIRY_MALLOC(total_payload);
    if (!request_buf)
        return AIRY_ENOMEM;

    __builtin_memcpy(request_buf, method_name, name_len);
    ((char *)request_buf)[name_len] = '\0';
    if (request_len > 0) {
        __builtin_memcpy((char *)request_buf + name_len + 1, request, request_len);
    }

    ipc_message_t req_msg = {0};
    req_msg.header.magic = IPC_MAGIC;
    req_msg.header.version = 1;
    req_msg.header.type = IPC_MSG_REQUEST;
    req_msg.header.msg_id = ++client->request_id_counter;
    req_msg.header.payload_len = total_payload;
    req_msg.header.timestamp = 0;
    req_msg.payload = request_buf;
    req_msg.payload_size = total_payload;

    airy_err_t err = ipc_send(client->transport, &req_msg);
    AIRY_FREE(request_buf);
    if (err != AIRY_SUCCESS)
        return err;

    ipc_message_t rsp_msg = {0};
    err = ipc_receive(client->transport, &rsp_msg, client->timeout_ms);
    if (err != AIRY_SUCCESS)
        return err;

    if (rsp_msg.payload == NULL || rsp_msg.payload_size < sizeof(ipc_rpc_header_t)) {
        ipc_message_free(&rsp_msg);
        return AIRY_EINVAL;
    }

    ipc_rpc_header_t *rsp_hdr = (ipc_rpc_header_t *)rsp_msg.payload;
    if (rsp_hdr->magic != IPC_RPC_MAGIC) {
        ipc_message_free(&rsp_msg);
        return AIRY_EINVAL;
    }

    if (rsp_hdr->status != 0) {

        ipc_message_free(&rsp_msg);
        return (airy_err_t)rsp_hdr->status;
    }

    size_t actual_response_len = rsp_hdr->payload_len;
    if (actual_response_len > response_max) {
        actual_response_len = response_max;
    }

    if (actual_response_len > 0 && response) {
        void *resp_payload = (char *)rsp_msg.payload + sizeof(ipc_rpc_header_t);
        __builtin_memcpy(response, resp_payload, actual_response_len);
    }

    if (response_len) {
        *response_len = rsp_hdr->payload_len;
    }

    ipc_message_free(&rsp_msg);
    return AIRY_SUCCESS;
}
