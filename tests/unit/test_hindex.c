// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_hindex.c
 * @brief hindex（字符串→下标开放寻址哈希索引 SSoT）单元测试
 *
 * 覆盖：生命周期 / put-get / 更新 / 未命中 / 删除 / 删后探测链不截断
 * （mem_d 无墓碑缺陷回归）/ 墓碑压实 / 触顶 fail-closed / 非法参数。
 *
 * 依据：0.1.19 架构改进方案 §4.3。
 */

#include "hindex.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 测试侧 djb2 副本：仅用于构造可复现的探测簇，不校验实现取值 */
static unsigned long ref_hash(const char *s)
{
    unsigned long h = 5381;
    int c;

    while ((c = (unsigned char)*s++) != 0)
        h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

/* 在 cap 下找出 3 个同桶键，用于强制构造线性探测簇 */
static void pick_cluster(char out[3][32], size_t cap)
{
    char first[64][32];
    int seen[64] = {0};

    for (int i = 0; i < 200000; i++) {
        char buf[32];
        unsigned long b;

        snprintf(buf, sizeof(buf), "col_%d", i);
        b = (unsigned long)(ref_hash(buf) % 64);
        if (!seen[b]) {
            seen[b] = 1;
            snprintf(first[b], sizeof(first[b]), "%s", buf);
            continue;
        }
        if (b < cap) {
            snprintf(out[0], 32, "%s", first[b]);
            snprintf(out[1], 32, "%s", buf);
            out[2][0] = '\0';
            return;
        }
    }
}

static void test_lifecycle(void)
{
    hindex_t h;

    printf("  test_lifecycle...\n");
    memset(&h, 0, sizeof(h));
    assert(hindex_init(NULL, 8) == AIRY_ERR_INVALID_PARAM);
    assert(hindex_init(&h, 0) == AIRY_ERR_INVALID_PARAM);
    assert(hindex_init(&h, 8) == AIRY_SUCCESS);
    assert(h.cap == 8 && h.cnt == 0 && h.used == 0);
    assert(hindex_get(&h, "nope") == -1);
    hindex_del(&h, "nope"); /* 空表删除为无操作 */
    hindex_free(&h);
    assert(h.ents == NULL && h.cap == 0);
    hindex_free(&h); /* 幂等 */
    printf("    PASSED\n");
}

static void test_put_get(void)
{
    hindex_t h;

    printf("  test_put_get...\n");
    memset(&h, 0, sizeof(h));
    assert(hindex_init(&h, 16) == AIRY_SUCCESS);

    assert(hindex_put(&h, "alpha", 7) == AIRY_SUCCESS);
    assert(hindex_put(&h, "beta", 3) == AIRY_SUCCESS);
    assert(hindex_get(&h, "alpha") == 7);
    assert(hindex_get(&h, "beta") == 3);
    assert(hindex_get(&h, "gamma") == -1);

    /* 同键覆盖：只更新下标，不产生副本 */
    assert(hindex_put(&h, "alpha", 11) == AIRY_SUCCESS);
    assert(hindex_get(&h, "alpha") == 11);
    assert(h.cnt == 2);

    hindex_free(&h);
    printf("    PASSED\n");
}

static void test_del_chain(void)
{
    hindex_t h;
    char cluster[3][32];

    printf("  test_del_chain...\n");
    memset(&h, 0, sizeof(h));
    assert(hindex_init(&h, 16) == AIRY_SUCCESS);

    pick_cluster(cluster, 16);
    assert(cluster[0][0] && cluster[1][0]);

    /* 强制同桶：后插键的探测路径穿过前插键的槽位 */
    assert(hindex_put(&h, cluster[0], 100) == AIRY_SUCCESS);
    assert(hindex_put(&h, cluster[1], 200) == AIRY_SUCCESS);
    assert(hindex_get(&h, cluster[1]) == 200);

    /* 删除簇首后，簇内后继键必须仍可命中（无墓碑实现会在此误判 -1） */
    hindex_del(&h, cluster[0]);
    assert(hindex_get(&h, cluster[0]) == -1);
    assert(hindex_get(&h, cluster[1]) == 200);
    assert(h.cnt == 1);

    /* 复插被删键并再次验证 */
    assert(hindex_put(&h, cluster[0], 300) == AIRY_SUCCESS);
    assert(hindex_get(&h, cluster[0]) == 300);
    assert(hindex_get(&h, cluster[1]) == 200);

    hindex_free(&h);
    printf("    PASSED\n");
}

static void test_pack(void)
{
    hindex_t h;
    char key[32];

    printf("  test_pack...\n");
    memset(&h, 0, sizeof(h));
    assert(hindex_init(&h, 8) == AIRY_SUCCESS);

    for (int i = 0; i < 6; i++) {
        snprintf(key, sizeof(key), "pack_%d", i);
        assert(hindex_put(&h, key, (size_t)i) == AIRY_SUCCESS);
    }
    /* 删除 3 个制造墓碑，再插入新键触发同容量压实 */
    for (int i = 0; i < 3; i++) {
        snprintf(key, sizeof(key), "pack_%d", i);
        hindex_del(&h, key);
    }
    assert(h.cnt == 3 && h.used == 6);

    for (int i = 6; i < 9; i++) {
        snprintf(key, sizeof(key), "pack_%d", i);
        assert(hindex_put(&h, key, (size_t)i) == AIRY_SUCCESS);
    }
    assert(h.cnt == 6);
    assert(h.used == 6); /* 压实已回收墓碑 */

    for (int i = 0; i < 3; i++) {
        snprintf(key, sizeof(key), "pack_%d", i);
        assert(hindex_get(&h, key) == -1);
    }
    for (int i = 3; i < 9; i++) {
        snprintf(key, sizeof(key), "pack_%d", i);
        assert(hindex_get(&h, key) == (ssize_t)i);
    }

    hindex_free(&h);
    printf("    PASSED\n");
}

static void test_full(void)
{
    hindex_t h;

    printf("  test_full...\n");
    memset(&h, 0, sizeof(h));
    assert(hindex_init(&h, 4) == AIRY_SUCCESS);

    assert(hindex_put(&h, "f0", 0) == AIRY_SUCCESS);
    assert(hindex_put(&h, "f1", 1) == AIRY_SUCCESS);
    assert(hindex_put(&h, "f2", 2) == AIRY_SUCCESS);
    /* cap×3/4 = 3 为装载上限，第 4 键 fail-closed，不静默丢键 */
    assert(hindex_put(&h, "f3", 3) == AIRY_ERR_OUT_OF_MEMORY);
    assert(hindex_get(&h, "f0") == 0);
    assert(hindex_get(&h, "f1") == 1);
    assert(hindex_get(&h, "f2") == 2);
    assert(hindex_get(&h, "f3") == -1);
    assert(h.cnt == 3);

    hindex_free(&h);
    printf("    PASSED\n");
}

static void test_bad_args(void)
{
    hindex_t h;

    printf("  test_bad_args...\n");
    memset(&h, 0, sizeof(h));
    assert(hindex_init(&h, 8) == AIRY_SUCCESS);

    assert(hindex_put(NULL, "k", 0) == AIRY_ERR_INVALID_PARAM);
    assert(hindex_put(&h, NULL, 0) == AIRY_ERR_INVALID_PARAM);
    assert(hindex_get(NULL, "k") == -1);
    assert(hindex_get(&h, NULL) == -1);
    hindex_del(NULL, "k");
    hindex_del(&h, NULL);
    hindex_free(NULL);
    assert(h.cnt == 0);

    hindex_free(&h);
    printf("    PASSED\n");
}

/* 长跑稳定性：反复增删，键集合始终可解析（模拟 daemon 长期运行） */
static void test_churn(void)
{
    hindex_t h;
    const int slots = 64;
    char key[32];

    printf("  test_churn...\n");
    memset(&h, 0, sizeof(h));
    assert(hindex_init(&h, slots * 4) == AIRY_SUCCESS);

    for (int round = 0; round < 200; round++) {
        for (int i = 0; i < slots; i++) {
            snprintf(key, sizeof(key), "churn_%d", i);
            if (hindex_get(&h, key) < 0)
                assert(hindex_put(&h, key, (size_t)i) == AIRY_SUCCESS);
        }
        for (int i = (round % 2); i < slots; i += 2) {
            snprintf(key, sizeof(key), "churn_%d", i);
            hindex_del(&h, key);
        }
    }

    for (int i = 0; i < slots; i++) {
        snprintf(key, sizeof(key), "churn_%d", i);
        if (hindex_get(&h, key) < 0)
            assert(hindex_put(&h, key, (size_t)i) == AIRY_SUCCESS);
    }
    for (int i = 0; i < slots; i++) {
        snprintf(key, sizeof(key), "churn_%d", i);
        assert(hindex_get(&h, key) == (ssize_t)i);
    }
    assert(h.cnt == (size_t)slots);

    hindex_free(&h);
    printf("    PASSED\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  hindex Unit Tests\n");
    printf("=========================================\n");

    test_lifecycle();
    test_put_get();
    test_del_chain();
    test_pack();
    test_full();
    test_bad_args();
    test_churn();

    printf("\nAll hindex tests PASSED\n");
    return 0;
}
