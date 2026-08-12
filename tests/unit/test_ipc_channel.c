// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ipc_channel.c
 * @brief IPC 模块初始化、默认配置与通道管理单元测试
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
 * 初始化与清理测试
 * ============================================================================ */

void test_ipc_init_success(void **state)
{
    (void)state;

    airy_err_t err = ipc_init();
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_cleanup();
}

void test_ipc_init_multiple(void **state)
{
    (void)state;

    assert_int_equal(ipc_init(), AIRY_SUCCESS);
    assert_int_equal(ipc_init(), AIRY_SUCCESS);

    ipc_cleanup();
}

/* ============================================================================
 * 默认配置测试
 * ============================================================================ */

void test_create_default_config_pipe(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);

    assert_int_equal(config.type, IPC_TYPE_PIPE);
    assert_int_equal(config.mode, IPC_MODE_READ_WRITE);
    assert_int_equal(config.buffer_size, IPC_DEFAULT_BUFFER_SIZE);
    assert_int_equal(config.max_message_size, IPC_MAX_MESSAGE_SIZE);
    assert_int_equal(config.timeout_ms, IPC_DEFAULT_TIMEOUT_MS);
    assert_false(config.nonblocking);
    assert_false(config.persistent);
}

void test_create_default_config_socket(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_SOCKET);

    assert_int_equal(config.type, IPC_TYPE_SOCKET);
    assert_int_equal(config.buffer_size, IPC_DEFAULT_BUFFER_SIZE);
}

/**
 * @brief 测试创建默认共享内存配置
 */
void test_create_default_config_shm(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_SHM);

    assert_int_equal(config.type, IPC_TYPE_SHM);
}

/* ============================================================================
 * 通道创建与销毁测试
 * ============================================================================ */

/**
 * @brief 测试创建通道成功
 */
void test_channel_create_success(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    assert_non_null(channel);
    assert_int_equal(ipc_channel_get_state(channel), IPC_STATE_CLOSED);

    ipc_channel_destroy(channel);
}

/**
 * @brief 测试用 NULL 配置创建通道返回 NULL
 */
void test_channel_create_null_config(void **state)
{
    (void)state;

    ipc_channel_t *channel = ipc_channel_create(NULL);
    assert_null(channel);
}

/**
 * @brief 测试销毁 NULL 通道不崩溃
 */
void test_channel_destroy_null(void **state)
{
    (void)state;

    ipc_channel_destroy(NULL);
}

/* ============================================================================
 * 通道打开与关闭测试
 * ============================================================================ */

/**
 * @brief 测试打开通道成功
 */
void test_channel_open_success(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    airy_err_t err = ipc_channel_open(channel);
    assert_int_equal(err, AIRY_SUCCESS);
    assert_int_equal(ipc_channel_get_state(channel), IPC_STATE_OPEN);

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试关闭已关闭的通道不报错
 */
void test_channel_close_already_closed(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    airy_err_t err = ipc_channel_close(channel);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_destroy(channel);
}

/**
 * @brief 测试用 NULL 参数操作通道
 */
void test_channel_operations_null(void **state)
{
    (void)state;

    assert_int_equal(ipc_channel_open(NULL), AIRY_EINVAL);
    assert_int_equal(ipc_channel_close(NULL), AIRY_EINVAL);
    assert_int_equal(ipc_channel_get_state(NULL), IPC_STATE_ERROR);
    assert_null(ipc_channel_get_name(NULL));
    assert_int_equal(ipc_channel_set_timeout(NULL, 1000), AIRY_EINVAL);
}

/* ============================================================================
 * 通道属性测试
 * ============================================================================ */

/**
 * @brief 测试获取通道名称
 */
void test_channel_get_name(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    config.name = "test_pipe";
    ipc_channel_t *channel = ipc_channel_create(&config);

    const char *name = ipc_channel_get_name(channel);
    assert_string_equal(name, "test_pipe");

    ipc_channel_destroy(channel);
}

/**
 * @brief 测试获取通道类型
 */
void test_channel_get_type(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_SOCKET);
    ipc_channel_t *channel = ipc_channel_create(&config);

    assert_int_equal(ipc_channel_get_type(channel), IPC_TYPE_SOCKET);

    ipc_channel_destroy(channel);
}

/**
 * @brief 测试设置超时时间
 */
void test_channel_set_timeout(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    airy_err_t err = ipc_channel_set_timeout(channel, 5000);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_destroy(channel);
}

/**
 * @brief 通道事件回调（文件作用域：GCC 禁止函数内 static 嵌套函数）
 */
static void test_event_cb(ipc_channel_t *ch, ipc_event_t event, const void *data, size_t len,
                          void *user_data)
{
    (void)ch;
    (void)data;
    (void)len;
    if (event == IPC_EVENT_CONNECTED && user_data) {
        (*(int *)user_data)++;
    }
}

/**
 * @brief 测试设置事件回调
 */
void test_channel_set_event_callback(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    int callback_called = 0;

    airy_err_t err = ipc_channel_set_event_callback(channel, test_event_cb, &callback_called);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_open(channel);
    assert_true(callback_called > 0);

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试统计信息
 */
void test_channel_stats(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    ipc_stats_t stats;
    airy_err_t err = ipc_channel_get_stats(channel, &stats);
    assert_int_equal(err, AIRY_SUCCESS);
    assert_int_equal(stats.messages_sent, 0);
    assert_int_equal(stats.bytes_sent, 0);

    err = ipc_channel_reset_stats(channel);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_destroy(channel);
}
