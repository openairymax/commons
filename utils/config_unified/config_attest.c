/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file config_attest.c
 * @brief 配置生效自证实现（0.1.21 A-8）
 *
 * 三段对账（声明 / 注册 / 生效）只做逐条判定与指纹计算，不做任何业务裁决；
 * 逃生开关只改变「不闭合」的后果，不改变判定本身——指纹与条目计数在
 * PASS / REJECT / ESCAPED 三种结局下完全一致，便于排障时横向比对。
 */

#include "config_attest.h"

#include "error.h"
#include "logging_compat.h"

#include <stdio.h>
#include <string.h>

#define ATTEST_FNV_BASIS 0xcbf29ce484222325ULL
#define ATTEST_FNV_PRIME 0x100000001b3ULL

static const char *const k_verdict_name[] = {"pass", "reject", "escaped"};

static void attest_feed(uint64_t *hash, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        *hash ^= (uint64_t)*p;
        *hash *= ATTEST_FNV_PRIME;
    }
    *hash ^= (uint64_t)'\x1f';
    *hash *= ATTEST_FNV_PRIME;
}

static void attest_feed_item(uint64_t *hash, const airy_attest_item_t *item)
{
    attest_feed(hash, item->key);
    attest_feed(hash, item->declared ? item->declared : "");
    attest_feed(hash, item->effective ? item->effective : "");
    char reg[4];
    (void)snprintf(reg, sizeof(reg), "%d", item->in_registry);
    attest_feed(hash, reg);
}

static int attest_item_open(const airy_attest_item_t *item)
{
    if (item->in_registry == AIRY_ATTEST_REG_MISS)
        return 0;
    const char *declared = item->declared ? item->declared : "";
    const char *effective = item->effective ? item->effective : "";
    if (declared[0] != '\0' && strcmp(declared, effective) != 0)
        return 0;
    return 1;
}

const char *att_verdict(airy_attest_verdict_t verdict)
{
    if ((int)verdict < 0 || (size_t)verdict >= sizeof(k_verdict_name) / sizeof(k_verdict_name[0]))
        return k_verdict_name[AIRY_ATTEST_REJECT];
    return k_verdict_name[verdict];
}

int airy_attest_run(const airy_attest_opts_t *opts, const airy_attest_item_t *items, size_t n,
                    airy_attest_result_t *out)
{
    if (!out || (n > 0 && !items))
        return AIRY_ERR_INVALID_PARAM;

    const char *owner = (opts && opts->owner && opts->owner[0]) ? opts->owner : "unknown";
    const int escape = (opts && opts->allow_unknown) ? 1 : 0;

    __builtin_memset(out, 0, sizeof(*out));
    uint64_t hash = ATTEST_FNV_BASIS;

    for (size_t i = 0; i < n; ++i) {
        const airy_attest_item_t *item = &items[i];
        if (!item->key || !item->key[0])
            return AIRY_ERR_INVALID_PARAM;

        attest_feed_item(&hash, item);
        out->checked++;

        if (attest_item_open(item)) {
            out->ok++;
            continue;
        }
        if (escape) {
            out->warned++;
            AIRY_LOG_WARN("attest[%s]: %s declared='%s' effective='%s' reg=%d 未闭合（逃生开关生效）",
                          owner, item->key, item->declared ? item->declared : "",
                          item->effective ? item->effective : "", item->in_registry);
        } else {
            out->failed++;
            AIRY_LOG_ERROR("attest[%s]: %s declared='%s' effective='%s' reg=%d 未闭合（拒绝启动）",
                           owner, item->key, item->declared ? item->declared : "",
                           item->effective ? item->effective : "", item->in_registry);
        }
    }

    if (out->failed > 0)
        out->verdict = AIRY_ATTEST_REJECT;
    else if (out->warned > 0)
        out->verdict = AIRY_ATTEST_ESCAPED;
    else
        out->verdict = AIRY_ATTEST_PASS;

    (void)snprintf(out->fingerprint, sizeof(out->fingerprint), "%016llx",
                   (unsigned long long)hash);
    AIRY_LOG_INFO("attest[%s]: verdict=%s checked=%d ok=%d warn=%d fail=%d fp=%s", owner,
                  att_verdict(out->verdict), out->checked, out->ok, out->warned,
                  out->failed, out->fingerprint);

    return out->verdict == AIRY_ATTEST_REJECT ? AIRY_ERR_SVC_CONFIG : AIRY_SUCCESS;
}
