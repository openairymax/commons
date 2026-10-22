// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_token.c
 * @brief token.h 单元测试
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "token.h"
#include "token_standard.h"

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                       \
    do {                                          \
        printf("🧪 Running %s...\n", #test_func); \
        if (test_func() != 0) {                   \
            failed_tests++;                       \
        } else {                                  \
            printf("✔PASS: %s\n", #test_func);    \
            passed_tests++;                       \
        }                                         \
    } while (0)

static int passed_tests = 0, failed_tests = 0;

/* 纯 CJK 文本：族系数（ACCURATE 路径）决定计数差异，
 * 同一段文本 bpe（CJK/1.5）必然多于 spm（CJK/2）。 */
static int test_token_family_scale(void)
{
    /* 上下文字台账会话（8 个汉字，UTF-8 24 字节） */
    const char *cjk = "\xe4\xb8\x8a\xe4\xb8\x8b\xe6\x96\x87\xe5\xad\x97"
                      "\xe5\x8f\xb0\xe8\xb4\xa6\xe4\xbc\x9a\xe8\xaf\x9d";

    airy_token_counter_t *bpe = airy_token_counter_create("bpe");
    airy_token_counter_t *spm = airy_token_counter_create("spm");
    TEST_ASSERT(bpe && spm, "bpe/spm counter creation failed");

    size_t bpe_count = airy_token_counter_count(bpe, cjk);
    size_t spm_count = airy_token_counter_count(spm, cjk);
    printf("  family scale: bpe=%zu spm=%zu\n", bpe_count, spm_count);

    airy_token_counter_destroy(bpe);
    airy_token_counter_destroy(spm);

    TEST_ASSERT(bpe_count != (size_t)-1 && spm_count != (size_t)-1,
                "family counting returned error");
    TEST_ASSERT(bpe_count > spm_count, "declared family had no effect on count");
    return 0;
}

static int test_token_unknown_family(void)
{
    airy_token_counter_t *counter = airy_token_counter_create("not-a-family");
    TEST_ASSERT(counter == NULL, "unknown family must be rejected");
    return 0;
}

static int test_token_generic(void)
{
    airy_token_counter_t *counter = airy_token_counter_create("generic");
    TEST_ASSERT(counter != NULL, "generic family rejected");

    size_t count = airy_token_counter_count(counter, "Hello, World!");
    printf("  generic count: %zu tokens\n", count);

    airy_token_counter_destroy(counter);
    TEST_ASSERT(count > 0 && count != (size_t)-1, "generic counting failed");
    return 0;
}

/* UTF-8 解码边界：overlong / surrogate / 截断 / 非法前导字节一律解码失败，
 * analyze_text 逐字节跳出不计入统计。表驱动统一形态（§427）的回归锚点。 */
static int test_token_utf8_decode(void)
{
    size_t cjk, alpha, total;

    /* 有效序列：3 字节 CJK、4 字节扩展 B、2 字节拉丁、U+0800 下边界 */
    airy_token_analyze_text("\xe4\xb8\xad", 3, &cjk, &alpha, &total);
    TEST_ASSERT(total == 1 && cjk == 1, "3-byte CJK miscounted");
    airy_token_analyze_text("\xf0\xa0\x80\x80", 4, &cjk, &alpha, &total);
    TEST_ASSERT(total == 1 && cjk == 1, "4-byte CJK ext-B miscounted");
    airy_token_analyze_text("\xc3\xa9", 2, &cjk, &alpha, &total);
    TEST_ASSERT(total == 1 && cjk == 0, "2-byte Latin miscounted");
    airy_token_analyze_text("\xe0\xa0\x80", 3, &cjk, &alpha, &total);
    TEST_ASSERT(total == 1, "U+0800 lower boundary rejected");

    /* 非法序列：解码失败，total=0 */
    airy_token_analyze_text("\xc0\x80", 2, &cjk, &alpha, &total);
    TEST_ASSERT(total == 0, "overlong 2-byte accepted");
    airy_token_analyze_text("\xe0\x9f\xbf", 3, &cjk, &alpha, &total);
    TEST_ASSERT(total == 0, "overlong 3-byte accepted");
    airy_token_analyze_text("\xed\xa0\x80", 3, &cjk, &alpha, &total);
    TEST_ASSERT(total == 0, "surrogate accepted");
    airy_token_analyze_text("\xf0\x8f\xbf\xbf", 4, &cjk, &alpha, &total);
    TEST_ASSERT(total == 0, "overlong 4-byte accepted");
    airy_token_analyze_text("\xf4\x90\x80\x80", 4, &cjk, &alpha, &total);
    TEST_ASSERT(total == 0, "above U+10FFFF accepted");
    airy_token_analyze_text("\xf5\x80\x80\x80", 4, &cjk, &alpha, &total);
    TEST_ASSERT(total == 0, "invalid lead 0xF5 accepted");
    airy_token_analyze_text("\xe4\xb8", 2, &cjk, &alpha, &total);
    TEST_ASSERT(total == 0, "truncated 3-byte accepted");

    /* 混合：非法字节不得泄漏进统计 */
    airy_token_analyze_text("A\xc0\x80" "B", 4, &cjk, &alpha, &total);
    TEST_ASSERT(total == 2 && alpha == 2, "invalid bytes leaked into count");
    return 0;
}

int main(void)
{
    printf("agentrt/commons/token 单元测试\n");
    TEST_RUN(test_token_family_scale);
    TEST_RUN(test_token_unknown_family);
    TEST_RUN(test_token_generic);
    TEST_RUN(test_token_utf8_decode);
    printf("测试结果：%d 通过，%d 失败\n", passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
