// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_id.c
 * @brief 品牌化 ID 实现（阶段 3）：trace_id / msg_id 结构化命名。
 *
 * - trace_id：64 位熵（splitmix64 混合：时间 + ASLR 地址 + 状态），
 *   命名 "tr-<16 hex>"（W3C 风格）。
 * - msg_id：高 32 位秒时间戳 | 低 32 位进程内单调序列（C11 原子），
 *   命名 "msg-<ts:08x>-<seq:08x>"。
 *
 * 类型声明见 commons/include/airy_types.h（品牌化 ID 段）。
 */

#include "airy_types.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 * trace_id：64 位熵（splitmix64）
 * ================================================================ */

static atomic_uint_fast64_t s_state = 0;

static uint64_t airy_id_entropy64(void)
{
    uint64_t st = atomic_load_explicit(&s_state, memory_order_relaxed);
    if (st == 0) {
        /* 首次：时间 + 本函数地址（ASLR）混合种子；CAS 保证仅首个线程写入 */
        uint64_t seed = (uint64_t)(uintptr_t)&airy_id_entropy64 ^
                        ((uint64_t)time(NULL) << 32) ^ (uint64_t)time(NULL);
        if (seed == 0)
            seed = 0x9E3779B97F4A7C15ull;
        uint64_t expected = 0;
        atomic_compare_exchange_strong_explicit(&s_state, &expected, seed,
                                                memory_order_relaxed, memory_order_relaxed);
    }
    uint64_t next = atomic_load_explicit(&s_state, memory_order_relaxed) + 0x9E3779B97F4A7C15ull;
    atomic_store_explicit(&s_state, next, memory_order_relaxed);
    uint64_t z = next;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

airy_trace_id_t airy_trace_id_generate(void)
{
    airy_trace_id_t id;
    id.value = airy_id_entropy64();
    return id;
}

int airy_trace_id_eq(airy_trace_id_t a, airy_trace_id_t b)
{
    return a.value == b.value;
}

/* ================================================================
 * msg_id：时间戳 | 单调序列
 * ================================================================ */

static atomic_uint_fast32_t g_msg_seq = 0;

airy_msg_id_t airy_msg_id_generate(void)
{
    uint32_t ts = (uint32_t)time(NULL);
    uint32_t seq = atomic_fetch_add_explicit(&g_msg_seq, 1u, memory_order_relaxed);
    airy_msg_id_t id;
    id.value = ((uint64_t)ts << 32) | (uint64_t)seq;
    return id;
}

int airy_msg_id_eq(airy_msg_id_t a, airy_msg_id_t b)
{
    return a.value == b.value;
}

/* ================================================================
 * 结构化命名
 * ================================================================ */

void airy_trace_id_to_string(airy_trace_id_t id, char *out, size_t out_cap)
{
    if (!out || out_cap == 0)
        return;
    snprintf(out, out_cap, "tr-%016llx", (unsigned long long)id.value);
}

void airy_msg_id_to_string(airy_msg_id_t id, char *out, size_t out_cap)
{
    if (!out || out_cap == 0)
        return;
    uint32_t ts = (uint32_t)(id.value >> 32);
    uint32_t seq = (uint32_t)(id.value & 0xFFFFFFFFull);
    snprintf(out, out_cap, "msg-%08x-%08x", ts, seq);
}

/* ================================================================
 * 解析（手写 hex，避免 sscanf；格式非法返回 *_NULL）
 * ================================================================ */

static int airy_id_hex_val(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static uint64_t airy_id_parse_hex(const char *s, size_t len, int *ok)
{
    uint64_t v = 0;
    for (size_t i = 0; i < len; i++) {
        int d = airy_id_hex_val(s[i]);
        if (d < 0) {
            *ok = 0;
            return 0;
        }
        v = (v << 4) | (uint64_t)d;
    }
    *ok = 1;
    return v;
}

airy_trace_id_t airy_trace_id_from_string(const char *str)
{
    airy_trace_id_t id = AIRY_TRACE_ID_NULL;
    if (!str || strncmp(str, "tr-", 3) != 0)
        return id;
    const char *hex = str + 3;
    if (strlen(hex) != 16)
        return id;
    int ok = 0;
    id.value = airy_id_parse_hex(hex, 16, &ok);
    return ok ? id : AIRY_TRACE_ID_NULL;
}

airy_msg_id_t airy_msg_id_from_string(const char *str)
{
    airy_msg_id_t id = AIRY_MSG_ID_NULL;
    if (!str || strncmp(str, "msg-", 4) != 0)
        return id;
    const char *p = str + 4;
    /* "<ts:8hex>-<seq:8hex>" */
    const char *dash = strchr(p, '-');
    if (!dash)
        return id;
    if ((size_t)(dash - p) != 8 || strlen(dash + 1) != 8)
        return id;
    int ok1 = 0, ok2 = 0;
    uint64_t ts = airy_id_parse_hex(p, 8, &ok1);
    uint64_t seq = airy_id_parse_hex(dash + 1, 8, &ok2);
    if (!ok1 || !ok2)
        return id;
    id.value = (ts << 32) | seq;
    return id;
}
