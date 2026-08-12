// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ipc_server.c
 * @brief IPC 模块服务端与客户端单元测试
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../tests/utils/test_framework.h"
#include "ipc_common.h"
#include "test_ipc_internal.h"

/* ============================================================================
 * 服务端测试
 * ============================================================================ */

/**
 * @brief 测试创建服务端
 */
void test_server_create(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_server_t *server = ipc_server_create(&config);

    assert_non_null(server);

    ipc_server_destroy(server);
}

/**
 * @brief 测试创建 NULL 配置服务端
 */
void test_server_create_null_config(void **state)
{
    (void)state;

    ipc_server_t *server = ipc_server_create(NULL);
    assert_null(server);
}

/**
 * @brief 测试服务端启动和停止
 */
void test_server_start_stop(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_server_t *server = ipc_server_create(&config);

    airy_err_t err = ipc_server_start(server);
    assert_int_equal(err, AIRY_SUCCESS);

    err = ipc_server_stop(server);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_server_destroy(server);
}

/**
 * @brief 测试服务端接受连接
 */
void test_server_accept(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    config.max_connections = 5;
    ipc_server_t *server = ipc_server_create(&config);

    ipc_server_start(server);

    ipc_channel_t *client = ipc_server_accept(server, 1000);

    if (client) {
        ipc_channel_destroy(client);
    }

    ipc_server_stop(server);
    ipc_server_destroy(server);
}

/**
 * @brief 测试服务端连接计数
 */
void test_server_connection_count(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_server_t *server = ipc_server_create(&config);

    size_t count = ipc_server_connection_count(server);
    assert_int_equal(count, 0);

    ipc_server_destroy(server);
}

/* ============================================================================
 * 客户端测试
 * ============================================================================ */

/**
 * @brief 测试创建客户端
 */
void test_client_create(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_client_t *client = ipc_client_create(&config);

    assert_non_null(client);

    ipc_client_destroy(client);
}

/**
 * @brief 测试客户端连接和断开
 */
void test_client_connect_disconnect(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_client_t *client = ipc_client_create(&config);

    airy_err_t err = ipc_client_connect(client, 5000);
    if (err == AIRY_SUCCESS) {
        ipc_client_disconnect(client);
    } else {
    }

    ipc_client_destroy(client);
}

/**
 * @brief 测试客户端获取通道
 */
void test_client_get_channel(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_client_t *client = ipc_client_create(&config);

    ipc_channel_t *ch = ipc_client_get_channel(client);

    assert_null(ch);

    ipc_client_destroy(client);
}
