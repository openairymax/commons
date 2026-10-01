// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file safe_utf8.c
 * @brief UTF-8 安全工具实现：RFC 3629 合规性校验与非法序列净化。
 *
 * 校验与净化共用同一套单序列解码原语（utf8_unit），两种消费方式因此
 * 不可能出现语义漂移；每个原语只承担一个判定，圈复杂度保持低位。
 */

#include "safe_utf8.h"

#include <stdint.h>

/* n 字节序列的最小合法码点：拒绝 overlong（如 C0 80、E0 80 80）。 */
static const uint32_t kMinCp[5] = {0, 0, 0x80u, 0x800u, 0x10000u};

/* U+FFFD REPLACEMENT CHARACTER，非法序列的统一替代。 */
static const char kRepl[3] = {(char)0xEF, (char)0xBF, (char)0xBD};

/* 首字节指示的序列长度；非法首字节（含 0x80~0xBF 孤立续字节）返回 0。 */
static size_t utf8_seq_len(unsigned char first)
{
    if (first < 0x80) {
        return 1;
    }
    if ((first & 0xE0) == 0xC0) {
        return 2;
    }
    if ((first & 0xF0) == 0xE0) {
        return 3;
    }
    if ((first & 0xF8) == 0xF0) {
        return 4;
    }
    return 0;
}

/* 续字节必须形如 10xxxxxx。 */
static bool utf8_cont_ok(const unsigned char *s, size_t n)
{
    for (size_t j = 1; j < n; j++) {
        if ((s[j] & 0xC0) != 0x80) {
            return false;
        }
    }
    return true;
}

/* 按 n 字节序列解码码点（n 已由 utf8_seq_len 判定且续字节已核对）。 */
static uint32_t utf8_decode(const unsigned char *s, size_t n)
{
    uint32_t cp = (n == 1) ? (uint32_t)s[0] : (uint32_t)(s[0] & (0xFFu >> (n + 1)));
    for (size_t j = 1; j < n; j++) {
        cp = (cp << 6) | (uint32_t)(s[j] & 0x3Fu);
    }
    return cp;
}

/* RFC 3629 值域：不低于最小码点、不超过 U+10FFFF、且不落在代理区间。 */
static bool utf8_cp_ok(uint32_t cp, uint32_t min_cp)
{
    if (cp < min_cp || cp > 0x10FFFFu) {
        return false;
    }
    return cp < 0xD800u || cp > 0xDFFFu;
}

/* 判定位置 0 起的单个序列：返回序列长度 n（1~4），*valid 给出合规性。 */
static size_t utf8_unit(const unsigned char *s, size_t avail, bool *valid)
{
    size_t n = utf8_seq_len(s[0]);
    if (n == 0 || n > avail) {
        *valid = false;
        return 1;
    }
    if (!utf8_cont_ok(s, n)) {
        *valid = false;
        return n;
    }
    *valid = utf8_cp_ok(utf8_decode(s, n), kMinCp[n]);
    return n;
}

bool utf8_validate(const char *str, size_t len)
{
    if (str == NULL) {
        return false;
    }

    size_t i = 0;
    while (i < len && str[i] != '\0') {
        bool valid = false;
        size_t n = utf8_unit((const unsigned char *)str + i, len - i, &valid);
        if (!valid) {
            return false;
        }
        i += n;
    }

    return true;
}

size_t utf8_sanitize(const char *in, size_t len, char *out, size_t out_cap)
{
    if (in == NULL || out == NULL || out_cap == 0) {
        return 0;
    }

    size_t i = 0;
    size_t written = 0;

    while (i < len) {
        bool valid = false;
        size_t n = utf8_unit((const unsigned char *)in + i, len - i, &valid);
        const char *chunk = valid ? in + i : kRepl;
        size_t chunk_len = valid ? n : sizeof(kRepl);

        if (written + chunk_len + 1 > out_cap) {
            break;
        }
        __builtin_memcpy(out + written, chunk, chunk_len);
        written += chunk_len;
        i += valid ? n : 1;
    }

    out[written] = '\0';
    return written;
}
