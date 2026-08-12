// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ipc_shm_mq.c
 * @brief IPC 模块共享内存与消息队列单元测试
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
 * 共享内存测试
 * ============================================================================ */

/**
 * @brief 测试创建共享内存对象
 */
void test_shm_create(void **state)
{
    (void)state;

    ipc_shm_config_t config = {0};
    config.name = "/test_airy_shm";
    config.size = 4096;
    config.create = true;
    config.read_only = false;

    ipc_shm_t *shm = ipc_shm_create(&config);
    assert_non_null(shm);

    ipc_shm_destroy(shm);
}

/**
 * @brief 测试共享内存大小查询
 */
void test_shm_get_size(void **state)
{
    (void)state;

    ipc_shm_config_t config = {0};
    config.name = "/test_shm_size";
    config.size = 8192;
    config.create = false;
    config.read_only = true;

    ipc_shm_t *shm = ipc_shm_create(&config);
    assert_non_null(shm);

    size_t size = ipc_shm_get_size(shm);
    assert_true(size >= 0);

    ipc_shm_destroy(shm);
}

/* ============================================================================
 * 消息队列测试
 * ============================================================================ */

/**
 * @brief 测试创建消息队列
 */
void test_mq_create(void **state)
{
    (void)state;

    ipc_mq_config_t config = {0};
    config.name = "/test_mq";
    config.max_messages = 100;
    config.max_message_size = 1024;

    ipc_mq_t *mq = ipc_mq_create(&config);
    assert_non_null(mq);

    ipc_mq_destroy(mq);
}

/**
 * @brief 测试消息队列计数
 */
void test_mq_count(void **state)
{
    (void)state;

    ipc_mq_config_t config = {0};
    config.name = "/test_mq_count";
    config.max_messages = 50;

    ipc_mq_t *mq = ipc_mq_create(&config);
    assert_non_null(mq);

    size_t count = ipc_mq_count(mq);
    assert_int_equal(count, 0);

    ipc_mq_destroy(mq);
}

/**
 * @brief 测试清空消息队列
 */
void test_mq_clear(void **state)
{
    (void)state;

    ipc_mq_config_t config = {0};
    config.name = "/test_mq_clear";
    config.max_messages = 50;

    ipc_mq_t *mq = ipc_mq_create(&config);
    assert_non_null(mq);

    airy_err_t err = ipc_mq_clear(mq);
    assert_int_equal(err, AIRY_SUCCESS);

    assert_int_equal(ipc_mq_count(mq), 0);

    ipc_mq_destroy(mq);
}
