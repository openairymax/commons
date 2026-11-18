// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_config_attest.c
 * @brief 配置生效自证（0.1.21 A-8）单元测试
 *
 * 判据出口：三值判定与退出码必须一一对应，指纹必须可复现且可区分。
 * 本用例不看日志文案，只看返回值、计数与指纹，避免「已打印」当「已闭合」。
 */

#include "config_attest.h"

#include <stdio.h>
#include <string.h>

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

static int test_pass_closed(void)
{
    airy_attest_item_t items[2] = {
        {.key = "default_model", .declared = "m1", .effective = "m1",
         .in_registry = AIRY_ATTEST_REG_HIT},
        {.key = "think.enabled", .declared = "1", .effective = "1",
         .in_registry = AIRY_ATTEST_REG_NA},
    };
    airy_attest_opts_t opts = {.owner = "ut", .allow_unknown = 0};
    airy_attest_result_t res;

    TEST_ASSERT(airy_attest_run(&opts, items, 2, &res) == AIRY_SUCCESS, "closed set must pass");
    TEST_ASSERT(res.verdict == AIRY_ATTEST_PASS, "verdict must be PASS");
    TEST_ASSERT(res.checked == 2 && res.ok == 2, "both items counted as ok");
    TEST_ASSERT(res.failed == 0 && res.warned == 0, "no failures on closed set");
    TEST_ASSERT(strlen(res.fingerprint) == 16, "fingerprint is 16 hex chars");
    return 0;
}

static int test_reject_registry_miss(void)
{
    airy_attest_item_t item = {.key = "default_model", .declared = "本地模型",
                               .effective = "本地模型", .in_registry = AIRY_ATTEST_REG_MISS};
    airy_attest_opts_t opts = {.owner = "ut", .allow_unknown = 0};
    airy_attest_result_t res;

    TEST_ASSERT(airy_attest_run(&opts, &item, 1, &res) == AIRY_ERR_SVC_CONFIG,
                "registry miss must return config error");
    TEST_ASSERT(res.verdict == AIRY_ATTEST_REJECT, "verdict must be REJECT");
    TEST_ASSERT(res.failed == 1 && res.ok == 0, "miss counted as failure");
    return 0;
}

static int test_escape_turns_warn(void)
{
    airy_attest_item_t item = {.key = "default_model", .declared = "ghost",
                               .effective = "ghost", .in_registry = AIRY_ATTEST_REG_MISS};
    airy_attest_opts_t opts = {.owner = "ut", .allow_unknown = 1};
    airy_attest_result_t res;

    TEST_ASSERT(airy_attest_run(&opts, &item, 1, &res) == AIRY_SUCCESS,
                "escape switch must release startup");
    TEST_ASSERT(res.verdict == AIRY_ATTEST_ESCAPED, "verdict must be ESCAPED");
    TEST_ASSERT(res.warned == 1 && res.failed == 0, "failure downgraded to warning");
    return 0;
}

static int test_reject_mismatch(void)
{
    airy_attest_item_t item = {.key = "think.t2", .declared = "slot-a",
                               .effective = "slot-b", .in_registry = AIRY_ATTEST_REG_HIT};
    airy_attest_opts_t opts = {.owner = "ut", .allow_unknown = 0};
    airy_attest_result_t res;

    TEST_ASSERT(airy_attest_run(&opts, &item, 1, &res) == AIRY_ERR_SVC_CONFIG,
                "declared != effective must reject");
    TEST_ASSERT(res.verdict == AIRY_ATTEST_REJECT, "verdict must be REJECT");
    return 0;
}

static int test_empty_declared_open(void)
{
    airy_attest_item_t item = {.key = "think.t2", .declared = "", .effective = "fallback-model",
                               .in_registry = AIRY_ATTEST_REG_NA};
    airy_attest_opts_t opts = {.owner = "ut", .allow_unknown = 0};
    airy_attest_result_t res;

    TEST_ASSERT(airy_attest_run(&opts, &item, 1, &res) == AIRY_SUCCESS,
                "empty declaration is fallback, not mismatch");
    TEST_ASSERT(res.verdict == AIRY_ATTEST_PASS, "verdict must be PASS");
    return 0;
}

static int test_fingerprint_stable(void)
{
    airy_attest_item_t a = {.key = "k", .declared = "v", .effective = "v",
                            .in_registry = AIRY_ATTEST_REG_HIT};
    airy_attest_item_t b = a;
    airy_attest_opts_t opts = {.owner = "ut", .allow_unknown = 0};
    airy_attest_result_t r1, r2;

    TEST_ASSERT(airy_attest_run(&opts, &a, 1, &r1) == AIRY_SUCCESS, "first run ok");
    TEST_ASSERT(airy_attest_run(&opts, &b, 1, &r2) == AIRY_SUCCESS, "second run ok");
    TEST_ASSERT(strcmp(r1.fingerprint, r2.fingerprint) == 0, "same input same fingerprint");

    b.effective = "other";
    TEST_ASSERT(airy_attest_run(&opts, &b, 1, &r2) == AIRY_ERR_SVC_CONFIG,
                "changed effective value must reject");
    TEST_ASSERT(strcmp(r1.fingerprint, r2.fingerprint) != 0,
                "different effective value must change fingerprint");
    return 0;
}

static int test_invalid_params(void)
{
    airy_attest_item_t item = {.key = "k", .declared = "v", .effective = "v",
                               .in_registry = AIRY_ATTEST_REG_NA};
    airy_attest_item_t no_key = {.key = NULL, .declared = "v", .effective = "v",
                                 .in_registry = AIRY_ATTEST_REG_NA};
    airy_attest_opts_t opts = {.owner = "ut", .allow_unknown = 0};

    TEST_ASSERT(airy_attest_run(&opts, &item, 1, NULL) == AIRY_ERR_INVALID_PARAM,
                "NULL result must be rejected");
    TEST_ASSERT(airy_attest_run(&opts, NULL, 1, NULL) == AIRY_ERR_INVALID_PARAM,
                "NULL items with n>0 must be rejected");
    airy_attest_result_t res;
    TEST_ASSERT(airy_attest_run(&opts, &no_key, 1, &res) == AIRY_ERR_INVALID_PARAM,
                "item without key must be rejected");
    TEST_ASSERT(att_verdict((airy_attest_verdict_t)99) != NULL,
                "verdict name never NULL");
    return 0;
}

int main(void)
{
    TEST_RUN(test_pass_closed);
    TEST_RUN(test_reject_registry_miss);
    TEST_RUN(test_escape_turns_warn);
    TEST_RUN(test_reject_mismatch);
    TEST_RUN(test_empty_declared_open);
    TEST_RUN(test_fingerprint_stable);
    TEST_RUN(test_invalid_params);

    printf("\n%d passed, %d failed\n", passed_tests, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
