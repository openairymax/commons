// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_svc_common.c
 * @brief 服务生命周期（Service Lifecycle）测试域：创建/启停/状态/注册表
 *
 * 0.1.19 S74：随 svc 生命周期机制族权威实现自 daemons/common/tests 迁入
 * commons；自含测试框架（本地桩与计数宏），无跨域 harness 依赖。
 *
 * 0.1.21 S-P2-05：全部断言改为对实现的具名期望。此前的「返回值是布尔」
 * 「成功或非零」「句柄或 NULL」一类写法恒真，无论实现怎么错都变绿，
 * 等于没有测试；现在逐条钉住状态迁移、错误码与指针恒等。
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "svc_common.h"
#include "airyrt_version.h"

static int g_tests_run;
static int g_tests_passed;
static int g_tests_failed;

#define TEST_ASSERT(cond, msg)                                \
    do {                                                      \
        g_tests_run++;                                        \
        if (cond) {                                           \
            g_tests_passed++;                                 \
            printf("  [PASS] %s\n", msg);                     \
        } else {                                              \
            g_tests_failed++;                                 \
            printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
        }                                                     \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg)                                                               \
    do {                                                                                        \
        g_tests_run++;                                                                          \
        if ((a) == (b)) {                                                                       \
            g_tests_passed++;                                                                   \
            printf("  [PASS] %s\n", msg);                                                       \
        } else {                                                                                \
            g_tests_failed++;                                                                   \
            printf("  [FAIL] %s: expected %ld, got %ld (line %d)\n", msg, (long)(b), (long)(a), \
                   __LINE__);                                                                   \
        }                                                                                       \
    } while (0)

static airy_err_t svc_dummy_init(airy_svc_t svc, const airy_svc_config_t *cfg)
{
    (void)svc;
    (void)cfg;
    return AIRY_SUCCESS;
}

static airy_err_t svc_dummy_start(airy_svc_t svc)
{
    (void)svc;
    return AIRY_SUCCESS;
}

static airy_err_t svc_dummy_stop(airy_svc_t svc, bool force)
{
    (void)svc;
    (void)force;
    return AIRY_SUCCESS;
}

static void svc_dummy_destroy(airy_svc_t svc)
{
    (void)svc;
}

static airy_err_t svc_dummy_healthcheck(airy_svc_t svc)
{
    (void)svc;
    return AIRY_SUCCESS;
}

static airy_svc_interface_t make_dummy_interface(void)
{
    airy_svc_interface_t iface;
    iface.init = svc_dummy_init;
    iface.start = svc_dummy_start;
    iface.stop = svc_dummy_stop;
    iface.destroy = svc_dummy_destroy;
    iface.healthcheck = svc_dummy_healthcheck;
    return iface;
}

/* ======================================================================== */
static void test_svc_create_destroy(void)
{
    printf("\n--- [Svc] 创建与销毁 ---\n");

    airy_svc_interface_t iface = make_dummy_interface();
    airy_svc_config_t config = {.name = "test_service",
                                .version = "1.0.0",
                                .capabilities = AIRY_SVC_CAP_ASYNC | AIRY_SVC_CAP_CANCELABLE,
                                .max_concurrent = 10,
                                .timeout_ms = 5000,
                                .priority = 5,
                                .auto_start = false,
                                .enable_metrics = true,
                                .enable_tracing = true};

    airy_svc_t svc = NULL;
    airy_err_t err = airy_svc_create(&svc, "test_service", &iface, &config);
    TEST_ASSERT_EQ(err, AIRY_SUCCESS, "create 成功返回 AIRY_SUCCESS");
    TEST_ASSERT(svc != NULL, "create 交出非空句柄");
    TEST_ASSERT_EQ(airy_svc_get_state(svc), AIRY_SVC_STATE_CREATED, "新建服务处于 CREATED");
    TEST_ASSERT(!airy_svc_is_running(svc), "新建服务未运行");
    TEST_ASSERT(!airy_svc_is_ready(svc), "新建服务未就绪");
    TEST_ASSERT(strcmp(airy_svc_get_name(svc), "test_service") == 0, "get_name 与入参一致");
    TEST_ASSERT(strcmp(airy_svc_get_version(svc), "1.0.0") == 0, "get_version 与配置一致");

    airy_svc_destroy(svc);
    airy_svc_destroy(NULL); /* 空句柄销毁须安全返回 */
    TEST_ASSERT(airy_svc_find("test_service") == NULL, "销毁后注册表中不再可见");
}

static void test_svc_full_lifecycle(void)
{
    printf("\n--- [Svc] 完整生命周期 ---\n");

    airy_svc_interface_t iface = make_dummy_interface();
    airy_svc_config_t config = {.name = "lifecycle_svc",
                                .version = "2.0.0",
                                .capabilities = AIRY_SVC_CAP_PAUSEABLE,
                                .max_concurrent = 4,
                                .timeout_ms = 3000};

    airy_svc_t svc = NULL;
    TEST_ASSERT_EQ(airy_svc_create(&svc, "lifecycle_svc", &iface, &config), AIRY_SUCCESS,
                   "create 成功");
    TEST_ASSERT(svc != NULL, "create 交出非空句柄");
    if (!svc)
        return;

    TEST_ASSERT_EQ(airy_svc_init(svc), AIRY_SUCCESS, "init 成功");
    TEST_ASSERT_EQ(airy_svc_get_state(svc), AIRY_SVC_STATE_READY, "init 后进入 READY");
    TEST_ASSERT(airy_svc_is_ready(svc), "READY 即就绪");
    TEST_ASSERT(!airy_svc_is_running(svc), "READY 尚未运行");

    TEST_ASSERT_EQ(airy_svc_start(svc), AIRY_SUCCESS, "start 成功");
    TEST_ASSERT_EQ(airy_svc_get_state(svc), AIRY_SVC_STATE_RUNNING, "start 后进入 RUNNING");
    TEST_ASSERT(airy_svc_is_running(svc), "RUNNING 即运行中");
    TEST_ASSERT(!airy_svc_is_ready(svc), "RUNNING 不再报就绪");

    TEST_ASSERT_EQ(airy_svc_pause(svc), AIRY_SUCCESS, "带 PAUSEABLE 的服务可暂停");
    TEST_ASSERT_EQ(airy_svc_get_state(svc), AIRY_SVC_STATE_PAUSED, "pause 后进入 PAUSED");
    TEST_ASSERT(!airy_svc_is_running(svc), "PAUSED 不报运行中");

    TEST_ASSERT_EQ(airy_svc_resume(svc), AIRY_SUCCESS, "resume 成功");
    TEST_ASSERT_EQ(airy_svc_get_state(svc), AIRY_SVC_STATE_RUNNING, "resume 回到 RUNNING");

    TEST_ASSERT_EQ(airy_svc_stop(svc, false), AIRY_SUCCESS, "stop 成功");
    TEST_ASSERT_EQ(airy_svc_get_state(svc), AIRY_SVC_STATE_STOPPED, "stop 后进入 STOPPED");
    TEST_ASSERT(!airy_svc_is_running(svc), "STOPPED 不报运行中");

    airy_svc_destroy(svc);
}

static void test_svc_pause_requires_capability(void)
{
    printf("\n--- [Svc] 无 PAUSEABLE 时拒绝暂停 ---\n");

    airy_svc_interface_t iface = make_dummy_interface();
    airy_svc_config_t config = {.name = "nocap_svc", .version = "1.0",
                                .capabilities = AIRY_SVC_CAP_NONE};

    airy_svc_t svc = NULL;
    TEST_ASSERT_EQ(airy_svc_create(&svc, "nocap_svc", &iface, &config), AIRY_SUCCESS,
                   "create 成功");
    if (!svc)
        return;
    TEST_ASSERT_EQ(airy_svc_init(svc), AIRY_SUCCESS, "init 成功");
    TEST_ASSERT_EQ(airy_svc_start(svc), AIRY_SUCCESS, "start 成功");

    TEST_ASSERT_EQ(airy_svc_pause(svc), AIRY_EPROTONOSUPPORT, "未声明能力时返回协议不支持");
    TEST_ASSERT_EQ(airy_svc_get_state(svc), AIRY_SVC_STATE_RUNNING, "拒绝后状态不变");

    airy_svc_destroy(svc);
}

static void test_svc_state_strings(void)
{
    printf("\n--- [Svc] 状态字符串转换 ---\n");

    static const struct {
        airy_svc_state_t state;
        const char *text;
    } cases[] = {{AIRY_SVC_STATE_NONE, "NONE"},     {AIRY_SVC_STATE_CREATED, "CREATED"},
                 {AIRY_SVC_STATE_INITIALIZING, "INITIALIZING"},
                 {AIRY_SVC_STATE_READY, "READY"},   {AIRY_SVC_STATE_RUNNING, "RUNNING"},
                 {AIRY_SVC_STATE_PAUSED, "PAUSED"}, {AIRY_SVC_STATE_STOPPING, "STOPPING"},
                 {AIRY_SVC_STATE_STOPPED, "STOPPED"},
                 {AIRY_SVC_STATE_ZOMBIE, "ZOMBIE"}, {AIRY_SVC_STATE_ERROR, "ERROR"}};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *str = airy_svc_state_to_string(cases[i].state);
        TEST_ASSERT(str != NULL && strcmp(str, cases[i].text) == 0, cases[i].text);
        TEST_ASSERT_EQ(airy_svc_state_from_string(str), cases[i].state, "往返转换一致");
    }

    TEST_ASSERT(strcmp(airy_svc_state_to_string((airy_svc_state_t)(AIRY_SVC_STATE_ERROR + 1)),
                       "UNKNOWN") == 0,
                "越界状态给出 UNKNOWN");
    TEST_ASSERT_EQ(airy_svc_state_from_string("UNKNOWN_STATE_XYZ"), AIRY_SVC_STATE_NONE,
                   "未知字符串回落到 NONE");
    TEST_ASSERT_EQ(airy_svc_state_from_string(NULL), AIRY_SVC_STATE_NONE, "空串回落到 NONE");
    TEST_ASSERT_EQ(airy_svc_state_from_string("running"), AIRY_SVC_STATE_RUNNING,
                   "大小写不敏感");
}

static void test_svc_capability_checks(void)
{
    printf("\n--- [Svc] 能力标志检查 ---\n");

    airy_svc_interface_t iface = make_dummy_interface();
    airy_svc_config_t config = {.name = "cap_svc",
                                .version = "1.0",
                                .capabilities = AIRY_SVC_CAP_ASYNC | AIRY_SVC_CAP_STREAMING |
                                                AIRY_SVC_CAP_PAUSEABLE | AIRY_SVC_CAP_BATCH};

    airy_svc_t svc = NULL;
    TEST_ASSERT_EQ(airy_svc_create(&svc, "cap_svc", &iface, &config), AIRY_SUCCESS, "create 成功");
    if (!svc)
        return;

    TEST_ASSERT(airy_svc_has_capability(svc, AIRY_SVC_CAP_ASYNC), "声明过的 ASYNC 可见");
    TEST_ASSERT(airy_svc_has_capability(svc, AIRY_SVC_CAP_BATCH), "声明过的 BATCH 可见");
    TEST_ASSERT(!airy_svc_has_capability(svc, AIRY_SVC_CAP_CANCELABLE), "未声明的 CANCELABLE 不可见");

    airy_svc_destroy(svc);
}

static void test_svc_registry_operations(void)
{
    printf("\n--- [Svc] 注册表操作 ---\n");

    airy_svc_interface_t iface = make_dummy_interface();
    airy_svc_config_t config = {.name = "reg_svc", .version = "1.0"};

    airy_svc_t svc = NULL;
    TEST_ASSERT_EQ(airy_svc_create(&svc, "reg_svc", &iface, &config), AIRY_SUCCESS, "create 成功");
    if (!svc)
        return;

    TEST_ASSERT_EQ(airy_svc_register(svc), AIRY_SUCCESS, "重复注册按幂等成功");
    TEST_ASSERT(airy_svc_find("reg_svc") == svc, "find 取回同一句柄");
    TEST_ASSERT(airy_svc_find("reg_svc_absent") == NULL, "未注册名字不可见");

    TEST_ASSERT_EQ(airy_svc_unregister(svc), AIRY_SUCCESS, "unregister 成功");
    TEST_ASSERT(airy_svc_find("reg_svc") == NULL, "注销后不可见");
    TEST_ASSERT_EQ(airy_svc_unregister(svc), AIRY_ENOENT, "重复注销返回 ENOENT");

    airy_svc_destroy(svc);
}

static void test_svc_user_data_and_metadata(void)
{
    printf("\n--- [Svc] 用户数据与元数据 ---\n");

    airy_svc_interface_t iface = make_dummy_interface();
    airy_svc_config_t config = {.name = "ud_svc"};

    airy_svc_t svc = NULL;
    TEST_ASSERT_EQ(airy_svc_create(&svc, "ud_svc", &iface, &config), AIRY_SUCCESS, "create 成功");
    if (!svc)
        return;

    int my_data = 0xDEAD;
    TEST_ASSERT_EQ(airy_svc_set_user_data(svc, &my_data), AIRY_SUCCESS, "set_user_data 成功");
    TEST_ASSERT(airy_svc_get_user_data(svc) == &my_data, "get_user_data 取回同一指针");

    TEST_ASSERT(strcmp(airy_svc_get_name(svc), "ud_svc") == 0, "get_name 与入参一致");
    TEST_ASSERT(strcmp(airy_svc_get_version(svc), AIRYRT_VERSION) == 0,
                "未配版本时回落编译版本号");

    airy_svc_destroy(svc);
}

int main(void)
{
    test_svc_create_destroy();
    test_svc_full_lifecycle();
    test_svc_pause_requires_capability();
    test_svc_state_strings();
    test_svc_capability_checks();
    test_svc_registry_operations();
    test_svc_user_data_and_metadata();

    printf("\n----------------------------------------\n");
    printf("  svc_common 生命周期测试: %d/%d 通过\n", g_tests_passed, g_tests_run);
    printf("----------------------------------------\n");

    return g_tests_failed > 0 ? 1 : 0;
}
