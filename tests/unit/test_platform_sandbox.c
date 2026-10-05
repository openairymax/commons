// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_platform_sandbox.c
 * @brief platform_sandbox 单元测试（配置契约与 no-op 路径）
 *
 * enabled=1 的真实 Landlock/seccomp 拦截行为测试位于 cupolas
 * test_sandbox.c（§203 B3 迁入 commons），此处只验证配置默认值
 * 与 no-op 契约（enabled=0 / NULL 均不得产生副作用）。
 */

#include <stdio.h>

#include "platform.h"

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

static int test_sandbox_noop(void)
{
    airy_native_sandbox_t sb;
    airy_native_sandbox_init(&sb);
    TEST_ASSERT(sb.enabled == 0, "default sandbox must be disabled");
    TEST_ASSERT(sb.deny_network == 0, "default must allow network");
    TEST_ASSERT(sb.ro_paths == NULL && sb.rw_paths == NULL,
                "default path lists must be NULL");
    TEST_ASSERT(airy_native_sandbox_apply(&sb) == 0,
                "disabled sandbox apply must be a no-op");
    return 0;
}

static int test_sandbox_null(void)
{
    TEST_ASSERT(airy_native_sandbox_apply(NULL) == 0, "NULL sandbox must be a no-op");
    return 0;
}

int main(void)
{
    printf("===========================================\n");
    printf("  agentrt/commons/platform_sandbox 单元测试\n");
    printf("===========================================\n\n");

    TEST_RUN(test_sandbox_noop);
    TEST_RUN(test_sandbox_null);

    printf("\n===========================================\n");
    printf("  测试结果: %d 通过, %d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");

    return failed_tests > 0 ? 1 : 0;
}
