// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file test_types.c
 * @brief 统一类型定义模块单元测试
 *
 * @details
 * 锁定 types.h / airymax/error.h / airymax/ipc.h 的现行契约：
 * - 错误码（用户态 POSIX errno 负值语义）与成功码
 * - 基础类型（时间戳、UUID、优先级、结果结构体）
 * - 任务类型（状态、类型枚举，任务配置结构体）
 * - 会话类型（状态枚举、配置/上下文结构体）
 * - Agent 类型（等级枚举、能力/契约结构体）
 * - 可观测性类型（指标/跨度枚举与结构体）
 * - IPC 类型（通道/标志枚举、配置结构体、128B 头契约常量）
 * - 网络类型（协议/连接状态枚举、端点/连接/HTTP 结构体）
 * - 辅助宏（ARRAY_SIZE, MIN/MAX, ALIGN_UP, 时间换算）
 * - 版本 SSoT（airyrt_version.h 的 AIRYRT_VERSION + airy_version_string）
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-09-27
 */

#include <stdio.h>
#include <string.h>

#include "../tests/utils/test_framework.h"
#include "airyrt_version.h"
#include "types.h"
#include "compat.h"
#include <airymax/ipc.h>

/* ============================================================================
 * 基础类型
 * ============================================================================ */

static void test_err_codes(void **state)
{
    (void)state;

    assert_int_equal(AIRY_SUCCESS, 0);
    assert_int_equal(AIRY_EOK, 0);

    /* 用户态错误码为 POSIX errno 负值（airy_types.h 权威） */
    assert_int_equal(AIRY_EINVAL, -22);
    assert_int_equal(AIRY_ENOMEM, -12);
    assert_int_equal(AIRY_EBUSY, -16);
    assert_true(AIRY_ETIMEDOUT < 0);
    assert_true(AIRY_ECANCELLED < 0);
    assert_true(AIRY_ENOTCONN < 0);
    assert_true(AIRY_EOVERFLOW < 0);
}

static void test_result_struct(void **state)
{
    (void)state;

    airy_result_t result = {0};

    result.code = AIRY_SUCCESS;
    result.message = "ok";
    result.detail = NULL;

    assert_int_equal(result.code, AIRY_SUCCESS);
    assert_string_equal(result.message, "ok");
    assert_null(result.detail);
}

static void test_prio_enums(void **state)
{
    (void)state;

    assert_int_equal(AIRY_PRIORITY_LOW, 0);
    assert_int_equal(AIRY_PRIORITY_NORMAL, 1);
    assert_int_equal(AIRY_PRIORITY_HIGH, 2);
    assert_int_equal(AIRY_PRIORITY_CRITICAL, 3);
}

static void test_id_types(void **state)
{
    (void)state;

    assert_int_equal(sizeof(airy_timestamp_t), 8);
    assert_int_equal(sizeof(airy_millis_t), 8);
    assert_int_equal(sizeof(airy_uuid_t), 37);
}

/* ============================================================================
 * 任务类型
 * ============================================================================ */

static void test_task_status(void **state)
{
    (void)state;

    assert_int_equal(AIRY_TASK_PENDING, 0);
    assert_int_equal(AIRY_TASK_RUNNING, 1);
    assert_int_equal(AIRY_TASK_SUCCEEDED, 2);
    assert_int_equal(AIRY_TASK_FAILED, 3);
    assert_int_equal(AIRY_TASK_CANCELLED, 4);
    assert_int_equal(AIRY_TASK_TIMEOUT, 5);
    assert_int_equal(AIRY_TASK_RETRYING, 6);
}

static void test_task_type(void **state)
{
    (void)state;

    assert_int_equal(AIRY_TASKTYPE_ONESHOT, 0);
    assert_int_equal(AIRY_TASKTYPE_RECURRING, 1);
    assert_int_equal(AIRY_TASKTYPE_CONDITIONAL, 2);
}

static void test_task_cfg(void **state)
{
    (void)state;

    airy_task_config_t config = {0};

    config.input = "hello";
    config.input_len = 5;
    config.timeout_ms = 5000;
    config.priority = AIRY_PRIORITY_NORMAL;
    config.type = AIRY_TASKTYPE_ONESHOT;
    config.agent_id = "agent_001";
    config.session_id = "session_abc";
    config.parent_task_id = NULL;

    assert_string_equal(config.input, "hello");
    assert_int_equal(config.input_len, 5);
    assert_int_equal(config.timeout_ms, 5000);
    assert_int_equal(config.priority, AIRY_PRIORITY_NORMAL);
    assert_int_equal(config.type, AIRY_TASKTYPE_ONESHOT);
    assert_string_equal(config.agent_id, "agent_001");
    assert_null(config.parent_task_id);
}

/* ============================================================================
 * 会话类型
 * ============================================================================ */

static void test_sess_status(void **state)
{
    (void)state;

    assert_int_equal(AIRY_SESSION_ACTIVE, 0);
    assert_int_equal(AIRY_SESSION_IDLE, 1);
    assert_int_equal(AIRY_SESSION_CLOSED, 2);
    assert_int_equal(AIRY_SESSION_EXPIRED, 3);
}

static void test_sess_cfg(void **state)
{
    (void)state;

    airy_session_config_t config = {0};

    config.user_id = "user_1";
    config.project_id = "proj_a";
    config.context = "ctx";
    config.ttl_seconds = 3600;
    config.priority = AIRY_PRIORITY_HIGH;

    assert_string_equal(config.user_id, "user_1");
    assert_string_equal(config.project_id, "proj_a");
    assert_int_equal(config.ttl_seconds, 3600);
    assert_int_equal(config.priority, AIRY_PRIORITY_HIGH);
}

static void test_ctx_struct(void **state)
{
    (void)state;

    airy_context_t ctx = {0};

    ctx.agent_id = "agent_001";
    ctx.session_id = "session_abc";
    ctx.trace_id = "tr-0011223344556677";
    ctx.parent_span_id = NULL;
    ctx.timestamp = 1234567890ULL;
    ctx.priority = AIRY_PRIORITY_CRITICAL;

    assert_string_equal(ctx.agent_id, "agent_001");
    assert_string_equal(ctx.session_id, "session_abc");
    assert_string_equal(ctx.trace_id, "tr-0011223344556677");
    assert_null(ctx.parent_span_id);
    assert_true(ctx.timestamp == 1234567890ULL);
    assert_int_equal(ctx.priority, AIRY_PRIORITY_CRITICAL);
}

/* ============================================================================
 * Agent 类型
 * ============================================================================ */

static void test_agent_level(void **state)
{
    (void)state;

    assert_int_equal(AIRY_AGENT_COMMUNITY, 0);
    assert_int_equal(AIRY_AGENT_VERIFIED, 1);
    assert_int_equal(AIRY_AGENT_OFFICIAL, 2);
}

static void test_cap_struct(void **state)
{
    (void)state;

    char cap_name[] = "tool_use";

    airy_capability_t cap = {0};

    cap.name = cap_name;
    cap.estimated_tokens = 4096;
    cap.avg_duration_ms = 250;

    assert_string_equal(cap.name, "tool_use");
    assert_int_equal(cap.estimated_tokens, 4096);
    assert_int_equal(cap.avg_duration_ms, 250);
}

static void test_contract(void **state)
{
    (void)state;

    airy_agent_contract_t contract = {0};

    contract.agent_id = "agent_001";
    contract.agent_name = "demo";
    contract.capability_count = 3;
    contract.models.system1 = "small";
    contract.cost.token_per_task_avg = 1200;
    contract.cost.level = AIRY_AGENT_VERIFIED;
    contract.trust.verified_provider = true;

    assert_string_equal(contract.agent_id, "agent_001");
    assert_string_equal(contract.agent_name, "demo");
    assert_int_equal(contract.capability_count, 3);
    assert_string_equal(contract.models.system1, "small");
    assert_int_equal(contract.cost.token_per_task_avg, 1200);
    assert_int_equal(contract.cost.level, AIRY_AGENT_VERIFIED);
    assert_true(contract.trust.verified_provider);
}

/* ============================================================================
 * 可观测性类型
 * ============================================================================ */

static void test_metric_type(void **state)
{
    (void)state;

    assert_int_equal(AIRY_METRIC_COUNTER_E, 0);
    assert_int_equal(AIRY_METRIC_GAUGE_E, 1);
    assert_int_equal(AIRY_METRIC_HISTOGRAM_E, 2);
    assert_int_equal(AIRY_METRIC_SUMMARY_E, 3);
}

static void test_span_enums(void **state)
{
    (void)state;

    assert_int_equal(AIRY_SPAN_INTERNAL, 0);
    assert_int_equal(AIRY_SPAN_CLIENT, 1);
    assert_int_equal(AIRY_SPAN_SERVER, 2);
    assert_int_equal(AIRY_SPAN_PRODUCER, 3);
    assert_int_equal(AIRY_SPAN_CONSUMER, 4);

    assert_int_equal(AIRY_SPAN_UNSET, 0);
    assert_int_equal(AIRY_SPAN_OK, 1);
    assert_int_equal(AIRY_SPAN_ERROR, 2);
}

static void test_metric_struct(void **state)
{
    (void)state;

    char metric_name[] = "latency";

    airy_metric_t metric = {0};

    metric.name = metric_name;
    metric.type = AIRY_METRIC_GAUGE_E;
    metric.unit = "ms";
    metric.value = 42.5;
    metric.label_count = 2;

    assert_string_equal(metric.name, "latency");
    assert_int_equal(metric.type, AIRY_METRIC_GAUGE_E);
    assert_string_equal(metric.unit, "ms");
    assert_float_equal(42.5f, (float)metric.value, 0.001f);
    assert_int_equal(metric.label_count, 2);
}

/* ============================================================================
 * IPC 类型
 * ============================================================================ */

static void test_ipc_type(void **state)
{
    (void)state;

    assert_int_equal(AIRY_IPC_PIPE, 0);
    assert_int_equal(AIRY_IPC_SOCKET, 1);
    assert_int_equal(AIRY_IPC_SHM, 2);
    assert_int_equal(AIRY_IPC_MQ, 3);
    assert_int_equal(AIRY_IPC_RPC, 4);
}

static void test_ipc_flag(void **state)
{
    (void)state;

    assert_int_equal(AIRY_IPC_FLAG_NONE, 0);
    assert_int_equal(AIRY_IPC_FLAG_NONBLOCK, 1);
    assert_int_equal(AIRY_IPC_FLAG_PRIORITY, 2);
    assert_int_equal(AIRY_IPC_FLAG_BROADCAST, 4);
}

static void test_ipc_cfg(void **state)
{
    (void)state;

    airy_ipc_config_t config = {0};

    config.type = AIRY_IPC_SOCKET;
    config.name = "chan0";
    config.buffer_size = 8192;
    config.timeout_ms = 5000;
    config.nonblocking = true;

    assert_int_equal(config.type, AIRY_IPC_SOCKET);
    assert_string_equal(config.name, "chan0");
    assert_int_equal(config.buffer_size, 8192);
    assert_int_equal(config.timeout_ms, 5000);
    assert_true(config.nonblocking);
}

static void test_ipc_hdr(void **state)
{
    (void)state;

    /* [SC] 128B 定长头契约（跨态字节级一致） */
    assert_true(AIRY_IPC_MAGIC == 0x41524531u);
    assert_int_equal(AIRY_IPC_HDR_SIZE, 128);
    assert_int_equal(AIRY_IPC_OP_SEND, 0x0001);
    assert_int_equal(AIRY_IPC_OP_CAP_RESPONSE, 0x0011);
    assert_int_equal(AIRY_IPC_FLAG_ZEROCOPY, 0x0001);
    assert_int_equal(AIRY_IPC_FLAG_RESERVED, 0xFFE0);
}

/* ============================================================================
 * 网络类型
 * ============================================================================ */

static void test_proto_enums(void **state)
{
    (void)state;

    assert_int_equal(AIRY_PROTO_TCP, 0);
    assert_int_equal(AIRY_PROTO_UDP, 1);
    assert_int_equal(AIRY_PROTO_HTTP, 2);
    assert_int_equal(AIRY_PROTO_HTTPS, 3);
    assert_int_equal(AIRY_PROTO_WS, 4);
    assert_int_equal(AIRY_PROTO_WSS, 5);
}

static void test_conn_state(void **state)
{
    (void)state;

    assert_int_equal(AIRY_CONN_DISCONNECTED, 0);
    assert_int_equal(AIRY_CONN_CONNECTING, 1);
    assert_int_equal(AIRY_CONN_CONNECTED, 2);
    assert_int_equal(AIRY_CONN_CLOSING, 3);
    assert_int_equal(AIRY_CONN_ERROR, 4);
}

static void test_endpoint(void **state)
{
    (void)state;

    char host[] = "localhost";

    airy_endpoint_t ep = {0};

    ep.host = host;
    ep.port = 8080;
    ep.protocol = AIRY_PROTO_HTTP;
    ep.path = "/api";

    assert_string_equal(ep.host, "localhost");
    assert_int_equal(ep.port, 8080);
    assert_int_equal(ep.protocol, AIRY_PROTO_HTTP);
    assert_string_equal(ep.path, "/api");
}

static void test_conn_cfg(void **state)
{
    (void)state;

    airy_conn_config_t config = {0};

    config.timeout_ms = 5000;
    config.read_timeout_ms = 10000;
    config.max_retries = 3;
    config.retry_delay_ms = 1000;
    config.keepalive = true;
    config.verify_ssl = true;

    assert_int_equal(config.timeout_ms, 5000);
    assert_int_equal(config.read_timeout_ms, 10000);
    assert_int_equal(config.max_retries, 3);
    assert_int_equal(config.retry_delay_ms, 1000);
    assert_true(config.keepalive);
    assert_true(config.verify_ssl);
}

static void test_http_req(void **state)
{
    (void)state;

    airy_http_request_t req = {0};

    req.method = "GET";
    req.path = "/data";
    req.header_count = 1;
    req.body_len = 0;
    req.timeout_ms = 5000;

    assert_string_equal(req.method, "GET");
    assert_string_equal(req.path, "/data");
    assert_int_equal(req.header_count, 1);
    assert_int_equal(req.body_len, 0);
    assert_int_equal(req.timeout_ms, 5000);
}

static void test_http_resp(void **state)
{
    (void)state;

    airy_http_response_t resp = {0};

    resp.status_code = 200;
    resp.body_len = 15;
    resp.error = AIRY_SUCCESS;

    assert_int_equal(resp.status_code, 200);
    assert_int_equal(resp.body_len, 15);
    assert_int_equal(resp.error, AIRY_SUCCESS);
}

/* ============================================================================
 * 辅助宏
 * ============================================================================ */

static void test_macro_size(void **state)
{
    (void)state;

    int array[] = {1, 2, 3, 4, 5};

    assert_int_equal(AIRY_ARRAY_SIZE(array), 5);
}

static void test_macro_minmax(void **state)
{
    (void)state;

    assert_int_equal(AIRY_MIN(3, 7), 3);
    assert_int_equal(AIRY_MIN(-1, 5), -1);
    assert_int_equal(AIRY_MAX(3, 7), 7);
    assert_int_equal(AIRY_MAX(-1, 5), 5);
    assert_int_equal(AIRY_MAX(100, 100), 100);
}

static void test_macro_align(void **state)
{
    (void)state;

    assert_int_equal(AIRY_ALIGN_UP(0, 16), 0);
    assert_int_equal(AIRY_ALIGN_UP(15, 16), 16);
    assert_int_equal(AIRY_ALIGN_UP(16, 16), 16);
    assert_int_equal(AIRY_ALIGN_UP(17, 16), 32);
    assert_int_equal(AIRY_ALIGN_UP(31, 16), 32);
}

static void test_ver_ssot(void **state)
{
    (void)state;

    /*
     * 版本 SSoT 为 AIRYRT_VERSION（airyrt_version.h），由构建系统从根
     * VERSION 文件注入；未注入时回退 "0.0.0-dev" 标识非发布构建。
     * 此处锁定两点契约：
     *   1) 注入管线生效（AIRYRT_VERSION 非空且非 marker）；
     *   2) airy_version_string() 与 AIRYRT_VERSION 同源（无第二副本）。
     */
    assert_true(AIRYRT_VERSION[0] != '\0');
    assert_string_equal(AIRYRT_VERSION, airy_version_string());

    /* 若构建未注入 VERSION，则退化为 marker，版本报告失真 → 大声失败 */
    assert_true(strcmp(AIRYRT_VERSION, "0.0.0-dev") != 0);
}

static void test_macro_time(void **state)
{
    (void)state;

    assert_true(AIRY_MS_TO_NS(1500) == 1500000000ULL);
    assert_true(AIRY_SEC_TO_MS(2) == 2000ULL);
    assert_true(AIRY_SEC_TO_NS(2) == 2000000000ULL);
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(void)
{
    const struct CMUnitTest tests[] = {

        cmocka_unit_test(test_err_codes),
        cmocka_unit_test(test_result_struct),
        cmocka_unit_test(test_prio_enums),
        cmocka_unit_test(test_id_types),

        cmocka_unit_test(test_task_status),
        cmocka_unit_test(test_task_type),
        cmocka_unit_test(test_task_cfg),

        cmocka_unit_test(test_sess_status),
        cmocka_unit_test(test_sess_cfg),
        cmocka_unit_test(test_ctx_struct),

        cmocka_unit_test(test_agent_level),
        cmocka_unit_test(test_cap_struct),
        cmocka_unit_test(test_contract),

        cmocka_unit_test(test_metric_type),
        cmocka_unit_test(test_span_enums),
        cmocka_unit_test(test_metric_struct),

        cmocka_unit_test(test_ipc_type),
        cmocka_unit_test(test_ipc_flag),
        cmocka_unit_test(test_ipc_cfg),
        cmocka_unit_test(test_ipc_hdr),

        cmocka_unit_test(test_proto_enums),
        cmocka_unit_test(test_conn_state),
        cmocka_unit_test(test_endpoint),
        cmocka_unit_test(test_conn_cfg),
        cmocka_unit_test(test_http_req),
        cmocka_unit_test(test_http_resp),

        cmocka_unit_test(test_macro_size),
        cmocka_unit_test(test_macro_minmax),
        cmocka_unit_test(test_macro_align),
        cmocka_unit_test(test_ver_ssot),
        cmocka_unit_test(test_macro_time),
    };

    return cmocka_run_group_tests(tests, sizeof(tests) / sizeof(tests[0]), NULL, NULL);
}
