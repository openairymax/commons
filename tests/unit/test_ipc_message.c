// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ipc_message.c
 * @brief IPC 模块消息辅助与工具函数单元测试
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
 * 消息辅助函数测试
 * ============================================================================ */

/**
 * @brief 测试创建消息
 */
void test_message_create(void **state)
{
    (void)state;

    const char *payload = "Test payload data";
    ipc_message_t *msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));

    assert_non_null(msg);
    assert_int_equal(msg->header.magic, IPC_MAGIC);
    assert_int_equal(msg->header.version, 1);
    assert_int_equal(msg->header.type, IPC_MSG_DATA);
    assert_non_null(msg->payload);
    assert_int_equal(msg->payload_size, strlen(payload));

    ipc_message_free(msg);
}

/**
 * @brief 测试创建空消息
 */
void test_message_create_empty(void **state)
{
    (void)state;

    ipc_message_t *msg = ipc_message_create(IPC_MSG_CONTROL, NULL, 0);

    assert_non_null(msg);
    assert_null(msg->payload);
    assert_int_equal(msg->payload_size, 0);

    ipc_message_free(msg);
}

/**
 * @brief 测试释放消息
 */
void test_message_free_null(void **state)
{
    (void)state;

    ipc_message_free(NULL);
}

/**
 * @brief 测试克隆消息
 */
void test_message_clone(void **state)
{
    (void)state;

    const char *original_payload = "Original data";
    ipc_message_t *original =
        ipc_message_create(IPC_MSG_DATA, original_payload, strlen(original_payload));

    ipc_message_t *clone = ipc_message_clone(original);

    assert_non_null(clone);
    assert_int_equal(clone->header.magic, original->header.magic);
    assert_int_equal(clone->header.type, original->header.type);
    assert_int_equal(clone->payload_size, original->payload_size);

    ipc_message_free(original);
    ipc_message_free(clone);
}

/**
 * @brief 测试克隆 NULL 消息
 */
void test_message_clone_null(void **state)
{
    (void)state;

    ipc_message_t *result = ipc_message_clone(NULL);
    assert_null(result);
}

/**
 * @brief 测试计算校验和
 */
void test_message_checksum(void **state)
{
    (void)state;

    const char *payload = "Checksum test data";
    ipc_message_t *msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));

    uint32_t checksum = ipc_message_checksum(msg);
    assert_true(checksum != 0);

    ipc_message_free(msg);
}

/**
 * @brief 测试验证有效消息
 */
void test_message_verify_valid(void **state)
{
    (void)state;

    const char *payload = "Valid message";
    ipc_message_t *msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));

    bool valid = ipc_message_verify(msg);
    assert_true(valid);

    ipc_message_free(msg);
}

/**
 * @brief 测试验证 NULL 消息
 */
void test_message_verify_null(void **state)
{
    (void)state;

    bool valid = ipc_message_verify(NULL);
    assert_false(valid);
}

/**
 * @brief 测试序列化和反序列化
 */
void test_message_serialize_deserialize(void **state)
{
    (void)state;

    const char *original_payload = "Serialize test";
    ipc_message_t *original =
        ipc_message_create(IPC_MSG_DATA, original_payload, strlen(original_payload));

    size_t buffer_size = sizeof(ipc_message_header_t) + original->payload_size + 1024;
    void *buffer = AIRY_MALLOC(buffer_size);
    size_t written = 0;

    airy_err_t err = ipc_message_serialize(original, buffer, buffer_size, &written);
    assert_int_equal(err, AIRY_SUCCESS);
    assert_true(written > 0);

    ipc_message_t deserialized;
    err = ipc_message_deserialize(buffer, written, &deserialized);
    assert_int_equal(err, AIRY_SUCCESS);
    assert_int_equal(deserialized.header.magic, original->header.magic);
    assert_int_equal(deserialized.header.type, original->header.type);
    assert_int_equal(deserialized.payload_size, original->payload_size);

    AIRY_FREE(deserialized.payload);
    AIRY_FREE(buffer);
    ipc_message_free(original);
}

/**
 * @brief 测试序列化到小缓冲区失败
 */
void test_serialize_buffer_too_small(void **state)
{
    (void)state;

    const char *payload = "Data that is too large for small buffer";
    ipc_message_t *msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));

    char small_buffer[10];
    size_t written = 0;

    airy_err_t err = ipc_message_serialize(msg, small_buffer, sizeof(small_buffer), &written);
    assert_int_equal(err, AIRY_EOVERFLOW);

    ipc_message_free(msg);
}

/* ============================================================================
 * 工具函数测试
 * ============================================================================ */

/**
 * @brief 测试获取错误消息
 */
void test_get_error_message(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    const char *error_msg = ipc_get_error_message(channel);
    assert_non_null(error_msg);

    ipc_channel_destroy(channel);
}

/**
 * @brief 测试获取 NULL 通道的错误消息
 */
void test_get_error_message_null(void **state)
{
    (void)state;

    const char *error_msg = ipc_get_error_message(NULL);
    assert_non_null(error_msg);
}

/**
 * @brief 测试有效性检查
 */
void test_is_valid(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    assert_false(ipc_is_valid(channel));

    ipc_channel_open(channel);
    assert_true(ipc_is_valid(channel));

    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试 NULL 通道有效性
 */
void test_is_valid_null(void **state)
{
    (void)state;

    assert_false(ipc_is_valid(NULL));
}

/**
 * @brief 测试刷新操作
 */
void test_flush(void **state)
{
    (void)state;

    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t *channel = ipc_channel_create(&config);

    airy_err_t err = ipc_flush(channel);
    assert_int_equal(err, AIRY_SUCCESS);

    ipc_channel_destroy(channel);
}
