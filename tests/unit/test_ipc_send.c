// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ipc_send.c
 * @brief IPC 模块消息发送、广播、通知与接收单元测试
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "airy_memory.h"

#include "../tests/utils/test_framework.h"
#include "ipc_common.h"
#include "test_ipc_internal.h"

/* ============================================================================
 * 消息发送测试
 * ============================================================================ */

/**
 * @brief 测试发送消息到打开的通道
 */
void test_send_message_success(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    ipc_channel_open(channel);

    ipc_message_t msg = {0};
    msg.header.aipc.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.msg_id = 1;
    msg.header.aipc.payload_len = 10;
    msg.payload = AIRY_MALLOC(10);
    msg.payload_size = 10;

    airy_err_t err = ipc_send(channel, &msg);
    assert_int_equal(err, AIRY_SUCCESS);

    AIRY_FREE(msg.payload);
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试发送数据便捷方法
 */
void test_send_data_success(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    ipc_channel_open(channel);

    const char *data = "Hello, IPC!";
    size_t sent = 0;

    airy_err_t err = ipc_send_data(channel, data, strlen(data), &sent);
    assert_int_equal(err, AIRY_SUCCESS);
    assert_int_equal(sent, strlen(data));

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试发送请求并等待响应
 */
void test_send_request_response(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    ipc_channel_open(channel);

    ipc_message_t request = {0};
    request.header.aipc.magic = IPC_MAGIC;
    request.header.version = 1;
    request.header.type = IPC_MSG_DATA;
    request.header.msg_id = 100;
    request.payload = NULL;
    request.payload_size = 0;

    ipc_message_t response = {0};
    airy_err_t err = ipc_send_request(channel, &request, &response, 5000);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试广播消息
 */
void test_broadcast_message(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    ipc_channel_open(channel);

    ipc_message_t msg = {0};
    msg.header.aipc.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.msg_id = 200;

    airy_err_t err = ipc_broadcast(channel, &msg);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试发送通知
 */
void test_notify_message(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    ipc_channel_open(channel);

    const char *notification = "Test notification";
    airy_err_t err = ipc_notify(channel, notification, strlen(notification));
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试错误参数处理
 */
void test_send_error_cases(void **state)
{
    (void)state;

    ipc_message_t msg = {0};

    assert_int_equal(ipc_send(NULL, &msg), AIRY_EINVAL);
    assert_int_equal(ipc_send_data(NULL, "data", 4, NULL), AIRY_EINVAL);

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *closed_channel = ipc_channel_create(&config);

    assert_int_equal(ipc_send(closed_channel, &msg), AIRY_ENOTCONN);

    ipc_channel_destroy(closed_channel);
}

/* ============================================================================
 * 消息接收测试
 * ============================================================================ */

/**
 * @brief 测试接收消息（先发送后接收，自环）
 */
void test_receive_message(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    ipc_channel_open(channel);

    ipc_message_t msg = {0};
    msg.header.aipc.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.msg_id = 1;
    msg.header.aipc.payload_len = 0;
    assert_int_equal(ipc_send(channel, &msg), AIRY_SUCCESS);

    ipc_message_t received_msg;
    airy_err_t err = ipc_receive(channel, &received_msg, 1000);
    assert_int_equal(err, AIRY_SUCCESS);
    assert_int_equal(received_msg.header.msg_id, 1);

    if (received_msg.payload) {
        AIRY_FREE(received_msg.payload);
    }

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试非阻塞接收
 */
void test_try_receive_message(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);
    ipc_channel_open(channel);

    ipc_message_t msg;
    airy_err_t err = ipc_try_receive(channel, &msg);

    assert_true(err == AIRY_SUCCESS || err == AIRY_EBUSY);

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 消息回调（文件作用域：GCC 禁止函数内 static 嵌套函数）
 */
static int test_msg_cb(ipc_channel_t *ch, ipc_message_t *msg, void *user_data)
{
    (void)ch;
    (void)msg;
    (void)user_data;
    return 0;
}

/**
 * @brief 测试设置消息回调
 */
void test_set_message_callback(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    airy_err_t err = ipc_set_message_callback(channel, test_msg_cb, NULL);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_destroy(channel);
}
