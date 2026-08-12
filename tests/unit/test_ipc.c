// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file test_ipc.c
 * @brief 进程间通信模块单元测试（主入口）
 *
 * @details
 * 测试用例按功能域拆分如下：
 * - test_ipc_channel.c：初始化/配置/通道管理
 * - test_ipc_send.c：消息发送/广播/通知/接收
 * - test_ipc_server.c：服务端/客户端
 * - test_ipc_shm_mq.c：共享内存/消息队列
 * - test_ipc_message.c：消息辅助/工具函数
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-04-02
 */

#include <stdio.h>
#include <string.h>

#include "../tests/utils/test_framework.h"
#include "test_ipc_internal.h"

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(void)
{
    const struct CMUnitTest tests[] = {

        cmocka_unit_test(test_ipc_init_success),
        cmocka_unit_test(test_ipc_init_multiple),

        cmocka_unit_test(test_create_default_config_pipe),
        cmocka_unit_test(test_create_default_config_socket),
        cmocka_unit_test(test_create_default_config_shm),

        cmocka_unit_test(test_channel_create_success),
        cmocka_unit_test(test_channel_create_null_config),
        cmocka_unit_test(test_channel_destroy_null),

        cmocka_unit_test(test_channel_open_success),
        cmocka_unit_test(test_channel_close_already_closed),
        cmocka_unit_test(test_channel_operations_null),

        cmocka_unit_test(test_channel_get_name),
        cmocka_unit_test(test_channel_get_type),
        cmocka_unit_test(test_channel_set_timeout),
        cmocka_unit_test(test_channel_set_event_callback),
        cmocka_unit_test(test_channel_stats),

        cmocka_unit_test(test_send_message_success),
        cmocka_unit_test(test_send_data_success),
        cmocka_unit_test(test_send_request_response),
        cmocka_unit_test(test_broadcast_message),
        cmocka_unit_test(test_notify_message),
        cmocka_unit_test(test_send_error_cases),

        cmocka_unit_test(test_receive_message),
        cmocka_unit_test(test_try_receive_message),
        cmocka_unit_test(test_set_message_callback),

        cmocka_unit_test(test_server_create),
        cmocka_unit_test(test_server_create_null_config),
        cmocka_unit_test(test_server_start_stop),
        cmocka_unit_test(test_server_accept),
        cmocka_unit_test(test_server_connection_count),

        cmocka_unit_test(test_client_create),
        cmocka_unit_test(test_client_connect_disconnect),
        cmocka_unit_test(test_client_get_channel),

        cmocka_unit_test(test_shm_create),
        cmocka_unit_test(test_shm_get_size),

        cmocka_unit_test(test_mq_create),
        cmocka_unit_test(test_mq_count),
        cmocka_unit_test(test_mq_clear),

        cmocka_unit_test(test_message_create),
        cmocka_unit_test(test_message_create_empty),
        cmocka_unit_test(test_message_free_null),
        cmocka_unit_test(test_message_clone),
        cmocka_unit_test(test_message_clone_null),
        cmocka_unit_test(test_message_checksum),
        cmocka_unit_test(test_message_verify_valid),
        cmocka_unit_test(test_message_verify_null),
        cmocka_unit_test(test_message_serialize_deserialize),
        cmocka_unit_test(test_serialize_buffer_too_small),

        cmocka_unit_test(test_get_error_message),
        cmocka_unit_test(test_get_error_message_null),
        cmocka_unit_test(test_is_valid),
        cmocka_unit_test(test_is_valid_null),
        cmocka_unit_test(test_flush),
    };

    return cmocka_run_group_tests(tests, sizeof(tests) / sizeof(tests[0]), NULL, NULL);
}
