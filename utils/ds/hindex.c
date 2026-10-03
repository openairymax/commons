/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file hindex.c
 * @brief 字符串→下标开放寻址哈希索引实现（djb2 + 线性探测 + 墓碑）。
 *
 * 装载治理：used×4 >= cap×3 时先同容量压实回收墓碑；压实后仍越限则
 * fail-closed。插入只落在首个 EMPTY（不复用墓碑），其探测终点与 get 的
 * 未命中终点一致，杜绝"绕过墓碑造成重复键或漏键"。
 *
 * 依据：0.1.19 架构改进方案 §4.3。
 */

#include "hindex.h"
#include "airy_memory.h"

#include <string.h>

/* djb2：与历史副本逐位一致，既有键分布不变（收敛零迁移代价） */
static unsigned long hindex_hash(const char *s)
{
    unsigned long h = 5381;
    int c;

    while ((c = (unsigned char)*s++) != 0)
        h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

/* 返回 key 的存活槽下标；遇 EMPTY 即止（线性探测终止条件） */
static ssize_t hindex_scan(const hindex_t *h, const char *key)
{
    size_t pos = (size_t)(hindex_hash(key) % h->cap);

    for (size_t i = 0; i < h->cap; i++, pos = (pos + 1) % h->cap) {
        const hindex_ent_t *e = &h->ents[pos];

        if (e->state == HINDEX_EMPTY)
            return -1;
        if (e->state == HINDEX_LIVE && strcmp(e->key, key) == 0)
            return (ssize_t)pos;
    }
    return -1;
}

int hindex_init(hindex_t *h, size_t cap)
{
    if (!h || cap == 0)
        return AIRY_ERR_INVALID_PARAM;

    h->ents = (hindex_ent_t *)AIRY_CALLOC(cap, sizeof(hindex_ent_t));
    if (!h->ents) {
        h->cap = 0;
        h->cnt = 0;
        h->used = 0;
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    h->cap = cap;
    h->cnt = 0;
    h->used = 0;
    return AIRY_SUCCESS;
}

void hindex_free(hindex_t *h)
{
    if (!h || !h->ents)
        return;

    for (size_t i = 0; i < h->cap; i++)
        AIRY_FREE(h->ents[i].key);
    AIRY_FREE(h->ents);
    h->ents = NULL;
    h->cap = 0;
    h->cnt = 0;
    h->used = 0;
}

/* 同容量压实：存活键在空表重建，墓碑归零。失败时原表保持可用 */
static int hindex_pack(hindex_t *h)
{
    hindex_ent_t *neu = (hindex_ent_t *)AIRY_CALLOC(h->cap, sizeof(hindex_ent_t));
    if (!neu)
        return AIRY_ERR_OUT_OF_MEMORY;

    hindex_ent_t *old = h->ents;
    size_t old_cap = h->cap;
    size_t live = 0;

    h->ents = neu;
    for (size_t i = 0; i < old_cap; i++) {
        size_t pos;

        if (old[i].state != HINDEX_LIVE)
            continue;
        pos = (size_t)(hindex_hash(old[i].key) % old_cap);
        while (neu[pos].state == HINDEX_LIVE)
            pos = (pos + 1) % old_cap;
        neu[pos] = old[i];
        live++;
    }
    AIRY_FREE(old);
    h->cnt = live;
    h->used = live;
    return AIRY_SUCCESS;
}

int hindex_put(hindex_t *h, const char *key, size_t index)
{
    ssize_t found;
    char *dup;
    size_t pos;

    if (!h || !h->ents || !key)
        return AIRY_ERR_INVALID_PARAM;

    found = hindex_scan(h, key);
    if (found >= 0) {
        h->ents[found].index = index;
        return AIRY_SUCCESS;
    }

    if (h->used * 4 >= h->cap * 3) {
        if (h->used > h->cnt && hindex_pack(h) != AIRY_SUCCESS)
            return AIRY_ERR_OUT_OF_MEMORY;
        if (h->used * 4 >= h->cap * 3)
            return AIRY_ERR_OUT_OF_MEMORY;
    }

    dup = AIRY_STRDUP(key);
    if (!dup)
        return AIRY_ERR_OUT_OF_MEMORY;

    pos = (size_t)(hindex_hash(key) % h->cap);
    while (h->ents[pos].state != HINDEX_EMPTY)
        pos = (pos + 1) % h->cap;

    h->ents[pos].key = dup;
    h->ents[pos].index = index;
    h->ents[pos].state = HINDEX_LIVE;
    h->cnt++;
    h->used++;
    return AIRY_SUCCESS;
}

ssize_t hindex_get(const hindex_t *h, const char *key)
{
    ssize_t pos;

    if (!h || !h->ents || !key || h->cnt == 0)
        return -1;

    pos = hindex_scan(h, key);
    return pos < 0 ? -1 : (ssize_t)h->ents[pos].index;
}

void hindex_del(hindex_t *h, const char *key)
{
    ssize_t pos;

    if (!h || !h->ents || !key || h->cnt == 0)
        return;

    pos = hindex_scan(h, key);
    if (pos < 0)
        return;

    AIRY_FREE(h->ents[pos].key);
    h->ents[pos].key = NULL;
    h->ents[pos].index = 0;
    h->ents[pos].state = HINDEX_DEAD;
    h->cnt--;
}
