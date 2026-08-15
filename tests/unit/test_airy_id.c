// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_airy_id.c
 * @brief 品牌化 ID 单元测试（阶段 3）：trace_id / msg_id 结构化命名。
 *
 * 验证：
 * - 生成非零 / 两次生成不相同（64 位熵 + 原子序列）
 * - 类型相等性 / *_NULL 判空
 * - to_string 结构化命名格式（"tr-<16 hex>" / "msg-<ts>-<seq>"）
 * - from_string 解析往返（含大写 hex / 非法格式返回 *_NULL）
 * - msg_id 高 32 位秒时间戳字段 / 低 32 位单调序列
 * - 输出缓冲 NULL / 容量不足安全截断
 */

#include "airy_types.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Release 下 assert 无效：恒生效 CHECK 宏 */
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "CHECK FAILED at %s:%d: %s\n", __FILE__,    \
                    __LINE__, #cond);                                   \
            return 1;                                                   \
        }                                                               \
    } while (0)

static int is_hex_str(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        int c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

static int test_trace_id_generate(void)
{
    airy_trace_id_t a = airy_trace_id_generate();
    airy_trace_id_t b = airy_trace_id_generate();

    /* 生成非零 */
    CHECK(!airy_trace_id_eq(a, AIRY_TRACE_ID_NULL));
    CHECK(!airy_trace_id_eq(b, AIRY_TRACE_ID_NULL));

    /* 两次生成不相同（64 位熵，碰撞概率可忽略） */
    CHECK(!airy_trace_id_eq(a, b));

    /* 相等性：自等 / 拷贝等 */
    airy_trace_id_t c = a;
    CHECK(airy_trace_id_eq(a, c));
    CHECK(airy_trace_id_eq(a, a));
    return 0;
}

static int test_trace_id_string_roundtrip(void)
{
    airy_trace_id_t id = airy_trace_id_generate();
    char buf[AIRY_TRACE_ID_STR_MAX];
    airy_trace_id_to_string(id, buf, sizeof(buf));

    /* 格式："tr-" + 16 hex + NUL，总长 3+16 = 19 */
    CHECK(strncmp(buf, "tr-", 3) == 0);
    CHECK(strlen(buf) == 19);
    CHECK(is_hex_str(buf + 3, 16));

    /* 往返：解析 == 原值 */
    airy_trace_id_t parsed = airy_trace_id_from_string(buf);
    CHECK(airy_trace_id_eq(parsed, id));

    /* 大写 hex 也可解析 */
    char upper[16];
    for (int i = 0; i < 16; i++)
        upper[i] = (char)(buf[3 + i] >= 'a' && buf[3 + i] <= 'f' ? buf[3 + i] - 'a' + 'A' : buf[3 + i]);
    char upper_str[24];
    snprintf(upper_str, sizeof(upper_str), "tr-%.16s", upper);
    parsed = airy_trace_id_from_string(upper_str);
    CHECK(airy_trace_id_eq(parsed, id));
    return 0;
}

static int test_msg_id_generate(void)
{
    airy_msg_id_t a = airy_msg_id_generate();
    airy_msg_id_t b = airy_msg_id_generate();

    CHECK(!airy_msg_id_eq(a, AIRY_MSG_ID_NULL));
    CHECK(!airy_msg_id_eq(b, AIRY_MSG_ID_NULL));

    /* 单调序列：b > a（高 32 位秒时间戳至少相等，低 32 位序列递增） */
    CHECK(a.value < b.value);
    return 0;
}

static int test_msg_id_string_roundtrip(void)
{
    airy_msg_id_t id = airy_msg_id_generate();
    char buf[AIRY_MSG_ID_STR_MAX];
    airy_msg_id_to_string(id, buf, sizeof(buf));

    /* 格式："msg-" + 8hex + '-' + 8hex + NUL，总长 4+8+1+8 = 21 */
    CHECK(strncmp(buf, "msg-", 4) == 0);
    CHECK(strlen(buf) == 21);
    CHECK(buf[12] == '-');
    CHECK(is_hex_str(buf + 4, 8));
    CHECK(is_hex_str(buf + 13, 8));

    /* 往返 */
    airy_msg_id_t parsed = airy_msg_id_from_string(buf);
    CHECK(airy_msg_id_eq(parsed, id));

    /* 高 32 位为秒时间戳：与当前时间相近（±120s 容差） */
    uint32_t ts = (uint32_t)(id.value >> 32);
    uint32_t now = (uint32_t)time(NULL);
    CHECK(ts <= now && now - ts <= 120);

    /* 低 32 位序列可递增可见 */
    airy_msg_id_t c = airy_msg_id_generate();
    CHECK((uint32_t)(c.value & 0xFFFFFFFFull) > (uint32_t)(id.value & 0xFFFFFFFFull));
    return 0;
}

static int test_from_string_invalid(void)
{
    /* NULL / 错误前缀 */
    CHECK(airy_trace_id_eq(airy_trace_id_from_string(NULL), AIRY_TRACE_ID_NULL));
    CHECK(airy_trace_id_eq(airy_trace_id_from_string("msg-00000000-00000000"), AIRY_TRACE_ID_NULL));
    CHECK(airy_msg_id_eq(airy_msg_id_from_string(NULL), AIRY_MSG_ID_NULL));
    CHECK(airy_msg_id_eq(airy_msg_id_from_string("tr-0000000000000000"), AIRY_MSG_ID_NULL));

    /* 长度错误 */
    CHECK(airy_trace_id_eq(airy_trace_id_from_string("tr-000000000000000"), AIRY_TRACE_ID_NULL));   /* 15 hex */
    CHECK(airy_trace_id_eq(airy_trace_id_from_string("tr-00000000000000000"), AIRY_TRACE_ID_NULL)); /* 17 hex */
    CHECK(airy_msg_id_eq(airy_msg_id_from_string("msg-0000000-00000000"), AIRY_MSG_ID_NULL));       /* ts 7 hex */
    CHECK(airy_msg_id_eq(airy_msg_id_from_string("msg-00000000-0000000"), AIRY_MSG_ID_NULL));       /* seq 7 hex */
    CHECK(airy_msg_id_eq(airy_msg_id_from_string("msg-0000000000000000"), AIRY_MSG_ID_NULL));       /* 缺 '-' */

    /* 非法 hex 字符 */
    CHECK(airy_trace_id_eq(airy_trace_id_from_string("tr-0000g00000000000"), AIRY_TRACE_ID_NULL));
    CHECK(airy_msg_id_eq(airy_msg_id_from_string("msg-0000g000-00000000"), AIRY_MSG_ID_NULL));
    return 0;
}

static int test_to_string_buffer_safety(void)
{
    airy_trace_id_t tid = airy_trace_id_generate();
    airy_msg_id_t mid = airy_msg_id_generate();

    /* NULL 缓冲 / 零容量：不崩溃 */
    airy_trace_id_to_string(tid, NULL, 0);
    airy_msg_id_to_string(mid, NULL, 0);

    /* 容量不足：安全截断（snprintf 保证 NUL） */
    char small[4];
    airy_trace_id_to_string(tid, small, sizeof(small));
    CHECK(strlen(small) < sizeof(small));
    small[0] = 'x';
    airy_msg_id_to_string(mid, small, 1);
    CHECK(small[0] == '\0');
    return 0;
}

/* ================================================================
 * main
 * ================================================================ */

int main(void)
{
    printf("=== 品牌化 ID（airy_id）单元测试 ===\n");
    int rc = 0;
    rc |= test_trace_id_generate();
    rc |= test_trace_id_string_roundtrip();
    rc |= test_msg_id_generate();
    rc |= test_msg_id_string_roundtrip();
    rc |= test_from_string_invalid();
    rc |= test_to_string_buffer_safety();
    if (rc == 0) {
        printf("ALL PASS\n");
    } else {
        printf("FAILED\n");
    }
    return rc;
}
