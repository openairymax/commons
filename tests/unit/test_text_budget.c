// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_text_budget.c
 * @brief 回填预算单元测试（C-23）：默认值/环境覆盖与夹取、未超预算直通、
 *        头尾保留与丢弃计数、多字节字符边界。
 */

#include "text_utils.h"

#include "safe_utf8.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

static void set_budget_env(const char *v)
{
    if (v)
        setenv("AIRY_BACKFILL_BUDGET", v, 1);
    else
        unsetenv("AIRY_BACKFILL_BUDGET");
}

static int test_budget_config(void)
{
    set_budget_env(NULL);
    TEST_ASSERT(airy_text_budget_bytes() == 8192, "默认预算应为 8 KB");

    set_budget_env("16384");
    TEST_ASSERT(airy_text_budget_bytes() == 16384, "环境值应生效");

    set_budget_env("512");
    TEST_ASSERT(airy_text_budget_bytes() == 1024, "过小值夹到 1 KiB");

    set_budget_env("10485760");
    TEST_ASSERT(airy_text_budget_bytes() == 1024 * 1024, "过大值夹到 1 MiB");

    set_budget_env("16k");
    TEST_ASSERT(airy_text_budget_bytes() == 8192, "非法值回落默认");
    set_budget_env("");
    TEST_ASSERT(airy_text_budget_bytes() == 8192, "空值回落默认");
    set_budget_env(NULL);
    return 0;
}

static int test_passthrough(void)
{
    TEST_ASSERT(airy_text_budget(NULL, 64) == NULL, "NULL 输入返回 NULL");
    char *out = airy_text_budget("hello", 64);
    TEST_ASSERT(out != NULL && strcmp(out, "hello") == 0, "未超预算原样返回");
    free(out);
    return 0;
}

static int test_ascii_cut(void)
{
    char src[4001];
    for (size_t i = 0; i < 4000; ++i)
        src[i] = (char)('a' + (int)(i % 26));
    src[4000] = '\0';

    char *out = airy_text_budget(src, 1024);
    TEST_ASSERT(out != NULL, "长串应返回新串");
    size_t n = strlen(out);
    TEST_ASSERT(n > 1024 && n < 1200, "结果长度 = 预算 + 标记开销");
    TEST_ASSERT(utf8_validate(out, n), "结果为合法 UTF-8");
    TEST_ASSERT(strncmp(out, src, 500) == 0, "头部保留原文");
    TEST_ASSERT(strcmp(out + n - 200, src + 4000 - 200) == 0, "尾部保留原文");
    TEST_ASSERT(strstr(out, "truncated 2976 bytes") != NULL, "丢弃字节数精确");

    char *zero = airy_text_budget(src, 0);
    TEST_ASSERT(zero != NULL && strlen(zero) < 8300, "预算 0 走默认 8 KB");
    TEST_ASSERT(utf8_validate(zero, strlen(zero)), "默认预算结果合法");
    free(zero);
    free(out);
    return 0;
}

/* 中文「中」为 3 字节；预算切点落在字符中间时，头尾都必须回退到完整字符。 */
static int test_utf8_boundary(void)
{
    char src[3601];
    for (int i = 0; i < 400; ++i) {
        src[i * 3] = (char)0xE4;
        src[i * 3 + 1] = (char)0xB8;
        src[i * 3 + 2] = (char)0xAD;
    }
    src[1200] = '\0';

    const size_t budgets[] = {1024, 1001, 1000, 999, 64, 33};
    for (size_t i = 0; i < sizeof(budgets) / sizeof(budgets[0]); ++i) {
        char *out = airy_text_budget(src, budgets[i]);
        TEST_ASSERT(out != NULL, "多字节预算分配失败");
        size_t n = strlen(out);
        TEST_ASSERT(utf8_validate(out, n), "多字节截断后仍为合法 UTF-8");
        TEST_ASSERT(n % 3 == 0 || strstr(out, "truncated") != NULL, "无残缺字符");
        /* 标记之前的内容必须能被 3 整除：头段落在完整「中」上。 */
        const char *mark = strstr(out, "\n…[truncated");
        if (mark) {
            TEST_ASSERT(((size_t)(mark - out)) % 3 == 0, "头部停在字符边界");
            size_t tail = n - (size_t)(mark - out);
            TEST_ASSERT(tail > 0, "尾部非空");
            TEST_ASSERT(((unsigned char)mark[0]) != 0x80, "标记未落在续字节上");
        }
        free(out);
    }
    return 0;
}

int main(void)
{
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"budget_config", test_budget_config},
        {"passthrough", test_passthrough},
        {"ascii_cut", test_ascii_cut},
        {"utf8_boundary", test_utf8_boundary},
    };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        printf("🧪 Running %s...\n", tests[i].name);
        if (tests[i].fn() != 0) {
            fprintf(stderr, "✗Test failed: %s\n", tests[i].name);
            failed_tests++;
        } else {
            printf("✔PASS: %s\n", tests[i].name);
            passed_tests++;
        }
    }
    printf("\n结果: %d 通过, %d 失败\n", passed_tests, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
