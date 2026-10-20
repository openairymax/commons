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

int main(void)
{
    printf("agentrt/commons/token 单元测试\n");
    TEST_RUN(test_token_family_scale);
    TEST_RUN(test_token_unknown_family);
    TEST_RUN(test_token_generic);
    printf("测试结果：%d 通过，%d 失败\n", passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
