// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_airy_effect.c
 * @brief 统一作用域 effect 原语（P23：注册即副作用 + disposer + 逆序回滚）单元测试
 *
 * 覆盖：生命周期 / 注册计数 / 逆序回滚 / commit 不执行 / dispose 手动提前
 * 释放单个 / destroy 未回滚自动逆序清理 / 扩容（超初始容量）。
 */

#include "airy_effect.h"
#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 恒生效断言：Release（NDEBUG）下 assert 为空，测试将失去验证能力。
 * 本项目单测在 Release 配置下运行，使用自定义 CHECK 保证始终校验。 */
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "CHECK FAILED at %s:%d: %s\n", __FILE__,       \
                    __LINE__, #cond);                                      \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

static int g_undo_seq[64];
static int g_undo_count;

static void undo_recorder(void *ctx)
{
    int id = (int)(intptr_t)ctx;
    CHECK(g_undo_count < 64);
    g_undo_seq[g_undo_count++] = id;
}

static void test_lifecycle(void)
{
    printf("  test_lifecycle...\n");
    airy_effect_t *fx = NULL;
    CHECK(airy_effect_create(&fx) == AIRY_EOK);
    CHECK(fx != NULL);
    CHECK(airy_effect_count(fx) == 0);

    /* 注册即副作用：add 只登记，不执行 */
    g_undo_count = 0;
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)1) == AIRY_EOK);
    CHECK(airy_effect_count(fx) == 1);
    CHECK(g_undo_count == 0);

    airy_effect_rollback(fx);
    CHECK(g_undo_count == 1 && g_undo_seq[0] == 1);
    CHECK(airy_effect_count(fx) == 0);

    /* scope 复用 */
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)2) == AIRY_EOK);
    CHECK(airy_effect_count(fx) == 1);
    airy_effect_destroy(fx);
    printf("    PASSED\n");
}

static void test_reverse_rollback(void)
{
    printf("  test_reverse_rollback...\n");
    airy_effect_t *fx = NULL;
    CHECK(airy_effect_create(&fx) == AIRY_EOK);

    g_undo_count = 0;
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)1) == AIRY_EOK);
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)2) == AIRY_EOK);
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)3) == AIRY_EOK);
    airy_effect_rollback(fx);

    CHECK(g_undo_count == 3);
    CHECK(g_undo_seq[0] == 3 && g_undo_seq[1] == 2 && g_undo_seq[2] == 1);
    CHECK(airy_effect_count(fx) == 0);
    airy_effect_destroy(fx);
    printf("    PASSED\n");
}

static void test_commit_skips_disposers(void)
{
    printf("  test_commit_skips_disposers...\n");
    airy_effect_t *fx = NULL;
    CHECK(airy_effect_create(&fx) == AIRY_EOK);

    g_undo_count = 0;
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)7) == AIRY_EOK);
    airy_effect_commit(fx);
    CHECK(g_undo_count == 0); /* 成功路径不执行 disposer */
    CHECK(airy_effect_count(fx) == 0);

    /* commit 后 scope 可复用 */
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)8) == AIRY_EOK);
    airy_effect_rollback(fx);
    CHECK(g_undo_count == 1 && g_undo_seq[0] == 8);
    airy_effect_destroy(fx);
    printf("    PASSED\n");
}

static void test_manual_dispose(void)
{
    printf("  test_manual_dispose...\n");
    airy_effect_t *fx = NULL;
    CHECK(airy_effect_create(&fx) == AIRY_EOK);

    g_undo_count = 0;
    /* undo_recorder 按 int 值解释 ctx，故用值型 ctx（intptr_t 编码） */
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)1) == AIRY_EOK);
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)2) == AIRY_EOK);
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)3) == AIRY_EOK);
    CHECK(airy_effect_count(fx) == 3);

    /* 提前释放中间项 ctx=2：只执行它，其余保留 */
    airy_effect_dispose(fx, (void *)(intptr_t)2);
    CHECK(g_undo_count == 1 && g_undo_seq[0] == 2);
    CHECK(airy_effect_count(fx) == 2);

    /* 剩余逆序回滚：ctx=3 然后 ctx=1 */
    airy_effect_rollback(fx);
    CHECK(g_undo_count == 3);
    CHECK(g_undo_seq[1] == 3 && g_undo_seq[2] == 1);
    CHECK(airy_effect_count(fx) == 0);
    airy_effect_destroy(fx);
    printf("    PASSED\n");
}

static void test_destroy_auto_rollback(void)
{
    printf("  test_destroy_auto_rollback...\n");
    airy_effect_t *fx = NULL;
    CHECK(airy_effect_create(&fx) == AIRY_EOK);

    g_undo_count = 0;
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)1) == AIRY_EOK);
    CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)2) == AIRY_EOK);
    airy_effect_destroy(fx); /* 未 commit/rollback → 逆序自动清理 */

    CHECK(g_undo_count == 2);
    CHECK(g_undo_seq[0] == 2 && g_undo_seq[1] == 1);
    printf("    PASSED\n");
}

static void test_growth(void)
{
    printf("  test_growth...\n");
    airy_effect_t *fx = NULL;
    CHECK(airy_effect_create(&fx) == AIRY_EOK);

    /* 初始容量 4，注册 20 个触发多次扩容 */
    g_undo_count = 0;
    for (int i = 1; i <= 20; i++)
        CHECK(airy_effect_add(fx, undo_recorder, (void *)(intptr_t)i) == AIRY_EOK);
    CHECK(airy_effect_count(fx) == 20);
    airy_effect_rollback(fx);
    CHECK(g_undo_count == 20);
    for (int i = 0; i < 20; i++)
        CHECK(g_undo_seq[i] == 20 - i);
    CHECK(airy_effect_count(fx) == 0);
    airy_effect_destroy(fx);
    printf("    PASSED\n");
}

static void test_null_safety(void)
{
    printf("  test_null_safety...\n");
    airy_effect_rollback(NULL);
    airy_effect_commit(NULL);
    airy_effect_dispose(NULL, NULL);
    airy_effect_destroy(NULL);
    CHECK(airy_effect_count(NULL) == 0);
    CHECK(airy_effect_create(NULL) == AIRY_EINVAL);
    airy_effect_t *fx = NULL;
    CHECK(airy_effect_create(&fx) == AIRY_EOK);
    CHECK(airy_effect_add(fx, NULL, NULL) == AIRY_EINVAL);
    CHECK(airy_effect_add(NULL, undo_recorder, NULL) == AIRY_EINVAL);
    airy_effect_destroy(fx);
    printf("    PASSED\n");
}

int main(void)
{
    printf("test_airy_effect...\n");
    test_lifecycle();
    test_reverse_rollback();
    test_commit_skips_disposers();
    test_manual_dispose();
    test_destroy_auto_rollback();
    test_growth();
    test_null_safety();
    printf("ALL PASSED\n");
    return 0;
}
