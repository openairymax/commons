// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ipc_rpc.c
 * @brief IPC 模块 RPC 框架单元测试
 *
 * 覆盖 WS-4 阶段 1 修复：
 * - P0-3：服务端/客户端对栈上消息使用 ipc_message_release
 * - P0-4：ipc_send_request 与 ipc_send 使用一致帧格式
 * - S1：超长 method_name 被拒绝而非溢出栈上响应头
 * - T-14：服务端按 max_request_size 拒绝超限请求
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "airy_memory.h"

#include "../tests/utils/test_framework.h"
#include "ipc_common.h"
#include "ipc_common_internal.h"
#include "test_ipc_internal.h"

/* ============================================================================
 * 测试辅助
 * ============================================================================ */

/**
 * @brief echo 处理器：将请求体原样回写到响应体
 */
static airy_err_t rpc_echo_handler(const void *request, size_t request_len, void *response,
                                   size_t *response_max, void *user_data)
{
    (void)user_data;

    if (request_len > 0 && request) {
        __builtin_memcpy(response, request, request_len);
    }
    if (response_max) {
        *response_max = request_len;
    }

    return AIRY_SUCCESS;
}

/**
 * @brief 构造 RPC 请求帧：payload = method + '\0' + body
 *
 * @return 请求消息；payload 由调用方用 AIRY_FREE 释放
 */
static ipc_message_t make_request(const char *method, const void *body, size_t body_len,
                                  uint64_t msg_id)
{
    size_t name_len = strlen(method);
    size_t total = name_len + 1 + body_len;

    char *buf = (char *)AIRY_MALLOC(total);
    __builtin_memcpy(buf, method, name_len);
    buf[name_len] = '\0';
    if (body_len > 0 && body) {
        __builtin_memcpy(buf + name_len + 1, body, body_len);
    }

    ipc_message_t req = {0};
    req.header.aipc.magic = IPC_MAGIC;
    req.header.version = 1;
    req.header.type = IPC_MSG_REQUEST;
    req.header.msg_id = msg_id;
    req.header.aipc.payload_len = (uint32_t)total;
    req.payload = buf;
    req.payload_size = total;

    return req;
}

/* ============================================================================
 * RPC 测试
 * ============================================================================ */

/**
 * @brief 测试完整 RPC 往返：请求 -> 处理 -> 响应
 */
void test_rpc_roundtrip(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    assert_non_null(channel);
    ipc_channel_open(channel);

    ipc_rpc_method_t methods[1];
    methods[0].method_name = "echo";
    methods[0].handler = rpc_echo_handler;
    methods[0].user_data = NULL;

    ipc_rpc_server_config_t scfg = {0};
    scfg.transport = channel;
    scfg.service_name = "test";
    scfg.methods = methods;
    scfg.method_count = 1;
    scfg.max_request_size = 4096;
    scfg.max_response_size = 4096;

    ipc_rpc_server_t *server = ipc_rpc_server_create(&scfg);
    assert_non_null(server);
    assert_int_equal(ipc_rpc_server_start(server), AIRY_SUCCESS);

    const char *body = "hello";
    ipc_message_t req = make_request("echo", body, strlen(body), 1);
    assert_int_equal(ipc_send(channel, &req), AIRY_SUCCESS);
    AIRY_FREE(req.payload);

    assert_int_equal(ipc_rpc_server_process(server, 1000), AIRY_SUCCESS);

    ipc_message_t rsp = {0};
    assert_int_equal(ipc_receive(channel, &rsp, 1000), AIRY_SUCCESS);
    assert_true(rsp.payload_size >= sizeof(ipc_rpc_header_t));

    ipc_rpc_header_t *hdr = (ipc_rpc_header_t *)rsp.payload;
    assert_int_equal(hdr->magic, IPC_RPC_MAGIC);
    assert_int_equal(hdr->status, 0);
    assert_int_equal(hdr->payload_len, strlen(body));
    assert_memory_equal((char *)rsp.payload + sizeof(ipc_rpc_header_t), body, strlen(body));

    ipc_message_release(&rsp);
    ipc_rpc_server_destroy(server);
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试未知方法返回 404
 */
void test_rpc_method_not_found(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    assert_non_null(channel);
    ipc_channel_open(channel);

    ipc_rpc_method_t methods[1];
    methods[0].method_name = "echo";
    methods[0].handler = rpc_echo_handler;
    methods[0].user_data = NULL;

    ipc_rpc_server_config_t scfg = {0};
    scfg.transport = channel;
    scfg.service_name = "test";
    scfg.methods = methods;
    scfg.method_count = 1;
    scfg.max_request_size = 4096;
    scfg.max_response_size = 4096;

    ipc_rpc_server_t *server = ipc_rpc_server_create(&scfg);
    assert_non_null(server);
    assert_int_equal(ipc_rpc_server_start(server), AIRY_SUCCESS);

    ipc_message_t req = make_request("missing", "x", 1, 2);
    assert_int_equal(ipc_send(channel, &req), AIRY_SUCCESS);
    AIRY_FREE(req.payload);

    assert_int_equal(ipc_rpc_server_process(server, 1000), AIRY_ENOENT);

    ipc_message_t rsp = {0};
    assert_int_equal(ipc_receive(channel, &rsp, 1000), AIRY_SUCCESS);
    assert_true(rsp.payload_size >= sizeof(ipc_rpc_header_t));
    assert_int_equal(((ipc_rpc_header_t *)rsp.payload)->status, 404);

    ipc_message_release(&rsp);
    ipc_rpc_server_destroy(server);
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试超长 method_name 被拒绝（S1）
 */
void test_rpc_oversized_method_name_rejected(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    assert_non_null(channel);
    ipc_channel_open(channel);

    ipc_rpc_method_t methods[1];
    methods[0].method_name = "echo";
    methods[0].handler = rpc_echo_handler;
    methods[0].user_data = NULL;

    ipc_rpc_server_config_t scfg = {0};
    scfg.transport = channel;
    scfg.service_name = "test";
    scfg.methods = methods;
    scfg.method_count = 1;
    scfg.max_request_size = 4096;
    scfg.max_response_size = 4096;

    ipc_rpc_server_t *server = ipc_rpc_server_create(&scfg);
    assert_non_null(server);
    assert_int_equal(ipc_rpc_server_start(server), AIRY_SUCCESS);

    /* IPC_RPC_METHOD_NAME_MAX 为 256，构造恰好达到上限的名字 */
    char big_name[IPC_RPC_METHOD_NAME_MAX + 1];
    memset(big_name, 'A', IPC_RPC_METHOD_NAME_MAX);
    big_name[IPC_RPC_METHOD_NAME_MAX] = '\0';

    ipc_message_t req = make_request(big_name, "x", 1, 3);
    assert_int_equal(ipc_send(channel, &req), AIRY_SUCCESS);
    AIRY_FREE(req.payload);

    assert_int_equal(ipc_rpc_server_process(server, 1000), AIRY_EOVERFLOW);

    ipc_rpc_server_destroy(server);
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试超限请求体被拒绝（T-14）
 */
void test_rpc_oversized_request_rejected(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    assert_non_null(channel);
    ipc_channel_open(channel);

    ipc_rpc_method_t methods[1];
    methods[0].method_name = "echo";
    methods[0].handler = rpc_echo_handler;
    methods[0].user_data = NULL;

    ipc_rpc_server_config_t scfg = {0};
    scfg.transport = channel;
    scfg.service_name = "test";
    scfg.methods = methods;
    scfg.method_count = 1;
    scfg.max_request_size = 8; /* 小于下面的请求体 */
    scfg.max_response_size = 4096;

    ipc_rpc_server_t *server = ipc_rpc_server_create(&scfg);
    assert_non_null(server);
    assert_int_equal(ipc_rpc_server_start(server), AIRY_SUCCESS);

    char body[20];
    memset(body, 'B', sizeof(body));

    ipc_message_t req = make_request("echo", body, sizeof(body), 4);
    assert_int_equal(ipc_send(channel, &req), AIRY_SUCCESS);
    AIRY_FREE(req.payload);

    assert_int_equal(ipc_rpc_server_process(server, 1000), AIRY_EOVERFLOW);

    ipc_rpc_server_destroy(server);
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}
