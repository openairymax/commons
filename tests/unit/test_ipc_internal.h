// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ipc_internal.h
 * @brief IPC 模块单元测试跨文件共享头（各域测试函数声明）
 */

#ifndef AIRY_RT_TEST_IPC_INTERNAL_H
#define AIRY_RT_TEST_IPC_INTERNAL_H

/* ============================================================================
 * 初始化 / 配置 / 通道测试（test_ipc_channel.c）
 * ============================================================================ */

void test_ipc_init_success(void **state);
void test_ipc_init_multiple(void **state);
void test_create_default_config_pipe(void **state);
void test_create_default_config_socket(void **state);
void test_create_default_config_shm(void **state);
void test_channel_create_success(void **state);
void test_channel_create_null_config(void **state);
void test_channel_destroy_null(void **state);
void test_channel_open_success(void **state);
void test_channel_close_already_closed(void **state);
void test_channel_operations_null(void **state);
void test_channel_get_name(void **state);
void test_channel_get_type(void **state);
void test_channel_set_timeout(void **state);
void test_channel_set_event_callback(void **state);
void test_channel_stats(void **state);

/* ============================================================================
 * 消息发送 / 接收测试（test_ipc_send.c）
 * ============================================================================ */

void test_send_message_success(void **state);
void test_send_data_success(void **state);
void test_send_request_response(void **state);
void test_broadcast_message(void **state);
void test_notify_message(void **state);
void test_send_error_cases(void **state);
void test_receive_message(void **state);
void test_try_receive_message(void **state);
void test_set_message_callback(void **state);

/* ============================================================================
 * 服务端 / 客户端测试（test_ipc_server.c）
 * ============================================================================ */

void test_server_create(void **state);
void test_server_create_null_config(void **state);
void test_server_start_stop(void **state);
void test_server_accept(void **state);
void test_server_connection_count(void **state);
void test_client_create(void **state);
void test_client_connect_disconnect(void **state);
void test_client_get_channel(void **state);

/* ============================================================================
 * 共享内存 / 消息队列测试（test_ipc_shm_mq.c）
 * ============================================================================ */

void test_shm_create(void **state);
void test_shm_get_size(void **state);
void test_mq_create(void **state);
void test_mq_count(void **state);
void test_mq_clear(void **state);

/* ============================================================================
 * 消息辅助 / 工具函数测试（test_ipc_message.c）
 * ============================================================================ */

void test_message_create(void **state);
void test_message_create_empty(void **state);
void test_message_free_null(void **state);
void test_message_clone(void **state);
void test_message_clone_null(void **state);
void test_message_checksum(void **state);
void test_message_verify_valid(void **state);
void test_message_verify_null(void **state);
void test_message_serialize_deserialize(void **state);
void test_serialize_buffer_too_small(void **state);
void test_get_error_message(void **state);
void test_get_error_message_null(void **state);
void test_is_valid(void **state);
void test_is_valid_null(void **state);
void test_flush(void **state);

#endif /* AIRY_RT_TEST_IPC_INTERNAL_H */
