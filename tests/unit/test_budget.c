// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_budget.c
 * @brief 超时预算树（0.1.21 C-21）单元测试
 *
 * 判据出口：预算树在任何配置下都单调（client ≥ server ≥ provider ≥ tool）
 * 且 server 对 think 留有余量；非法配置只能保持缺省，不能放宽内层。
 * 断言只看 airy_budget 的返回值，不看日志，避免「已打印」当「已生效」。
 */

#include "airy_budget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
static void env_set(const char *k, const char *v)
{
    _putenv_s(k, v);
}
#else
static void env_set(const char *k, const char *v)
{
    setenv(k, v, 1);
}
#endif

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("🧪 Running %s...\n", #test_func);              \
        if (test_func() != 0) {                                \
            fprintf(stderr, "✗Test failed: %s\n", #test_func); \
            failed_tests++;                                    \
        } else {                                               \
            printf("✔PASS: %s\n", #test_func);                 \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* 每个用例从干净环境出发：测试机/CI 上的外部变量不得改变基线。 */
static void budget_clear(void)
{
    env_set("AIRY_TIMEOUT_PROFILE", "");
    env_set("AIRY_TIMEOUT_CLIENT_MS", "");
    env_set("AIRY_TIMEOUT_SERVER_MS", "");
    env_set("AIRY_TIMEOUT_THINK_MS", "");
    env_set("AIRY_THINK_TIMEOUT_MS", "");
    env_set("AIRY_TIMEOUT_PROVIDER_MS", "");
    env_set("AIRY_TIMEOUT_TOOL_MS", "");
}

static int test_default_tree(void)
{
    budget_clear();
    TEST_ASSERT(strcmp(airy_budget_profile(), "default") == 0, "profile: default");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_CLIENT) == 600000u, "client: 600s");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_SERVER) == 150000u, "server: 150s");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_THINK) == 120000u, "think: 120s");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_PROVIDER) == 70000u, "provider: 70s");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_TOOL) == 60000u, "tool: 60s");
    TEST_ASSERT(airy_budget_verify() == 0, "tree: monotonic out of the box");
    return 0;
}

static int test_profile_switch(void)
{
    budget_clear();
    env_set("AIRY_TIMEOUT_PROFILE", "small");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_CLIENT) == 300000u, "small: client 300s");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_PROVIDER) == 60000u, "small: provider 60s");
    TEST_ASSERT(airy_budget_verify() == 0, "small: monotonic");

    env_set("AIRY_TIMEOUT_PROFILE", "large");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_SERVER) == 600000u, "large: server 600s");
    TEST_ASSERT(airy_budget_verify() == 0, "large: monotonic");
    return 0;
}

static int test_unknown_profile(void)
{
    budget_clear();
    env_set("AIRY_TIMEOUT_PROFILE", "bogus");
    TEST_ASSERT(strcmp(airy_budget_profile(), "default") == 0, "profile: bogus falls back");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_SERVER) == 150000u, "bogus: default server");
    return 0;
}

static int test_env_override_and_invalid(void)
{
    budget_clear();
    env_set("AIRY_TIMEOUT_PROVIDER_MS", "80000");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_PROVIDER) == 80000u, "env: provider override");

    env_set("AIRY_TIMEOUT_PROVIDER_MS", "abc");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_PROVIDER) == 70000u, "env: invalid ignored");
    env_set("AIRY_TIMEOUT_PROVIDER_MS", "10");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_PROVIDER) == 70000u, "env: below floor ignored");
    return 0;
}

static int test_clamp_raises_outer(void)
{
    budget_clear();
    env_set("AIRY_TIMEOUT_CLIENT_MS", "50000");
    TEST_ASSERT(airy_budget_verify() > 0, "clamp: inverted client reported");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_CLIENT) == 150000u, "clamp: client raised to server");
    return 0;
}

static int test_think_margin(void)
{
    budget_clear();
    env_set("AIRY_TIMEOUT_THINK_MS", "150000");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_THINK) == 150000u, "think: env override");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_SERVER) == 180000u, "think: server keeps 20% margin");
    TEST_ASSERT(airy_budget_ms(AIRY_BUDGET_CLIENT) >= 180000u, "think: client follows server");
    return 0;
}

static int test_outer_rounding(void)
{
    budget_clear();
    TEST_ASSERT(airy_budget_outer(0) == 0, "outer: zero stays zero");
    TEST_ASSERT(airy_budget_outer(60000) == 75000, "outer: 60s -> 75s");
    TEST_ASSERT(airy_budget_outer(70000) == 88000, "outer: 70s -> 88s aligned");
    TEST_ASSERT(airy_budget_outer(1000000) <= 86400000u, "outer: hard ceiling");
    return 0;
}

int main(void)
{
    TEST_RUN(test_default_tree);
    TEST_RUN(test_profile_switch);
    TEST_RUN(test_unknown_profile);
    TEST_RUN(test_env_override_and_invalid);
    TEST_RUN(test_clamp_raises_outer);
    TEST_RUN(test_think_margin);
    TEST_RUN(test_outer_rounding);

    budget_clear();
    printf("\n%d passed, %d failed\n", passed_tests, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
