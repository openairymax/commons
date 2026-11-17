// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_text_keywords.c
 * @brief 关键词切分单元测试（0.1.21 C-13）：ASCII/CJK 分隔、码点计数、
 *        单字虚词与纯符号词出局、截断 UTF-8 不越界。
 */

#include "text_utils.h"

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

static bool kw_has(char **keywords, size_t count, const char *want)
{
    for (size_t i = 0; i < count; i++) {
        if (keywords[i] && strcmp(keywords[i], want) == 0)
            return true;
    }
    return false;
}

static int test_ascii_entities(void)
{
    char *kw[16];
    size_t n = airy_text_keywords("Deploy the web service, then verify.", kw, 16);

    TEST_ASSERT(kw_has(kw, n, "deploy"), "大写应小写化入集");
    TEST_ASSERT(kw_has(kw, n, "web"), "三字母词应保留");
    TEST_ASSERT(kw_has(kw, n, "service"), "普通实体应保留");
    TEST_ASSERT(kw_has(kw, n, "the"), "ASCII 虚词按原口径保留（仅中文单字虚词出局）");
    airy_text_kw_free(kw, n);
    return 0;
}

static int test_cjk_sentence(void)
{
    char *kw[16];
    size_t n = airy_text_keywords("在285H目标机上安装 tree，并验证版本。", kw, 16);

    TEST_ASSERT(n > 0, "中文句应切出词项");
    TEST_ASSERT(kw_has(kw, n, "tree"), "英文实体应保留");
    TEST_ASSERT(!kw_has(kw, n, "在"), "单字虚词不得入集");
    TEST_ASSERT(!kw_has(kw, n, "，"), "标点不得入集");

    bool digit = false;
    for (size_t i = 0; i < n; i++) {
        if (kw[i] && strstr(kw[i], "285h"))
            digit = true;
    }
    TEST_ASSERT(digit, "数字与字母构成的词项应保留");
    airy_text_kw_free(kw, n);
    return 0;
}

static int test_stopwords_and_symbols(void)
{
    char *kw[16];
    size_t n = airy_text_keywords("的，了。和、与；用：把！将？", kw, 16);

    TEST_ASSERT(n == 0, "单字虚词与纯标点不得产生关键词");
    airy_text_kw_free(kw, n);
    return 0;
}

static int test_limits(void)
{
    char *kw[2];
    size_t n = airy_text_keywords("alpha beta gamma", kw, 1);

    TEST_ASSERT(n == 1, "max_keywords 应封顶");
    TEST_ASSERT(strcmp(kw[0], "alpha") == 0, "截断应保留先出现的词");
    airy_text_kw_free(kw, n);
    TEST_ASSERT(airy_text_keywords(NULL, kw, 2) == 0, "NULL 输入返回 0");
    TEST_ASSERT(airy_text_keywords("alpha", NULL, 2) == 0, "NULL 输出返回 0");
    TEST_ASSERT(airy_text_keywords("alpha", kw, 0) == 0, "零容量返回 0");
    return 0;
}

static int test_truncated_utf8(void)
{
    char *kw[4];
    size_t n = airy_text_keywords("ab\xE4\xB8", kw, 4);

    TEST_ASSERT(n <= 1, "截断序列不得膨胀出多个词项");
    airy_text_kw_free(kw, n);
    return 0;
}

int main(void)
{
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"ascii_entities", test_ascii_entities},
        {"cjk_sentence", test_cjk_sentence},
        {"stopwords_and_symbols", test_stopwords_and_symbols},
        {"limits", test_limits},
        {"truncated_utf8", test_truncated_utf8},
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
