/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file hindex.h
 * @brief 字符串→下标开放寻址哈希索引唯一真相源（SSoT）：纯机制件。
 *
 * djb2 散列 + 线性探测 + 墓碑删除。此前 agent_d 与 mem_d 各自复刻一份同源
 * 实现，且 mem_d 副本删除后把槽位置空、无墓碑，会截断探测链——长跑后同一
 * 探测簇内的后继键被误判 miss。本文件为唯一权威实现，两处消费方一律委托。
 *
 * 契约：
 *   - key 为 NUL 结尾字符串；put 时内部 strdup 持有所有权，free 时统一释放
 *   - 装载上限 3/4；触顶时先同容量压实回收墓碑，压实后仍越限则 fail-closed
 *     返回 AIRY_ERR_OUT_OF_MEMORY，绝不静默丢键
 *   - get 命中返回 put 时登记的 index（非内部槽位）；未命中返回 -1
 *   - del 置墓碑（HINDEX_DEAD），探测链不截断
 *   - 非线程安全：调用方负责串行化（agent_d/mem_d 各自持全局锁）
 *
 * 依据：0.1.19 架构改进方案 §4.3（机制件收敛至 commons）、L4 归位消解。
 */

#ifndef AIRY_RT_COMMONS_DS_HINDEX_H
#define AIRY_RT_COMMONS_DS_HINDEX_H

#include "error.h"

#include <stddef.h>
#ifndef _WIN32
#include <sys/types.h> /* ssize_t */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 槽状态：空 / 存活 / 墓碑（删除标记，探测链不截断） */
#define HINDEX_EMPTY 0
#define HINDEX_LIVE 1
#define HINDEX_DEAD 2

typedef struct {
    char *key;    /* 内部持有；空槽与墓碑为 NULL */
    size_t index; /* 键映射的下标（由调用方解释语义） */
    int state;    /* HINDEX_EMPTY / HINDEX_LIVE / HINDEX_DEAD */
} hindex_ent_t;

typedef struct {
    hindex_ent_t *ents; /* cap 个槽的连续数组 */
    size_t cap;         /* 槽总数（初值，压实不扩容） */
    size_t cnt;         /* 存活键数 */
    size_t used;        /* 占用槽数（存活 + 墓碑） */
} hindex_t;

int hindex_init(hindex_t *h, size_t cap);
void hindex_free(hindex_t *h);
int hindex_put(hindex_t *h, const char *key, size_t index);
ssize_t hindex_get(const hindex_t *h, const char *key);
void hindex_del(hindex_t *h, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMONS_DS_HINDEX_H */
