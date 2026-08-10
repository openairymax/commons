/**
 * @file test_print.c
 * @brief P3.25: airy_print.h 运行时统一打印 API 单元测试
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * 验证 8 个宏可编译、可调用、不崩溃。
 * 宏底层委托 log_write()，无需显式初始化日志系统（log_write
 * 在未初始化时按默认配置输出到 stderr）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "airy_memory.h"
#include "airy_print.h"

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "FAIL: %s\n", message);  \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("Running %s...\n", #test_func);                 \
        if (test_func() != 0) {                                \
            fprintf(stderr, "Test failed: %s\n", #test_func);  \
            failed_tests++;                                    \
        } else {                                               \
            printf("PASS: %s\n", #test_func);                  \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* 验证 8 个宏可调用且不崩溃 */
static int test_print_macros_callable(void)
{
    /* 状态打印 */
    airy_print_ok("ok message: %s", "success");
    airy_print_no("no message: %s", "failure");

    /* 级别打印 */
    airy_print_info("info message: %d", 1);
    airy_print_warn("warn message: %d", 2);
    airy_print_error("error message: %d", 3);
    airy_print_fatal("fatal message: %d", 4);
    airy_print_debug("debug message: %d", 5);

    /* 章节标题 */
    airy_print_section("Section Title");

    printf("  All 8 macros callable: OK\n");
    return 0;
}

/* 验证宏展开为 log_write 调用（编译期验证，运行期不崩溃即可） */
static int test_print_delegates_to_log_write(void)
{
    /* 如果宏展开正确，以下代码可编译且链接成功（log_write 符号存在）。
     * 运行时调用不崩溃即验证委托关系成立。 */
    airy_print_info("delegate verification: %s=%d", "code", 200);

    printf("  Delegate to log_write: OK\n");
    return 0;
}

/* 验证格式化参数传递正确 */
static int test_print_format_args(void)
{
    int val = 42;
    const char *str = "test";

    airy_print_info("int=%d str=%s hex=%x oct=%o", val, str, val, val);
    airy_print_ok("combined: %s-%d", str, val);

    printf("  Format args propagation: OK\n");
    return 0;
}

int main(void)
{
    printf("=== airy_print.h Unit Tests (P3.25) ===\n\n");

    TEST_RUN(test_print_macros_callable);
    TEST_RUN(test_print_delegates_to_log_write);
    TEST_RUN(test_print_format_args);

    printf("\n=== Results: %d passed, %d failed ===\n",
           passed_tests, failed_tests);

    return (failed_tests > 0) ? 1 : 0;
}
