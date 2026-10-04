/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file scan.c
 * @brief UTF-8/ASCII 混合文本词流扫描机制件（词法唯一实现，契约见 token.h）。
 *
 * 词定义：[a-z0-9_] 连续段为一个词（A-Z 归一为小写）；其余 UTF-8 序列整段
 * 为一个词，残缺尾部序列按剩余字节产出，非法前缀字节跳过；NUL 提前终止。
 * 多字节词产出前先 flush 已累积的 ASCII 词，避免跨字符边界粘连。
 * 0.1.19 §164：单源化 mem_d compress/cache 与 atoms memory builtin_index
 * 三处同构扫描器（G26 跨文件克隆消解 + compress 侧 CJK 边界粘连修复）。
 */

#include "token.h"

/* 词字符归一：返回小写字符值，非词字符返回 0（词法原语） */
static int word_char(unsigned char c)
{
    if (c >= 'a' && c <= 'z')
        return c;
    if (c >= '0' && c <= '9')
        return c;
    if (c == '_')
        return c;
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    return 0;
}

/* UTF-8 前导字节定序长（词法原语） */
static size_t utf8_seq_len(unsigned char c)
{
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return 1;
}

/* 冲刷累积中的 ASCII 词（词法原语） */
static void flush_word(char *word, size_t *wlen, airy_word_fn fn, void *ud)
{
    if (*wlen > 0) {
        word[*wlen] = '\0';
        fn(ud, word);
        *wlen = 0;
    }
}

void airy_words_scan(const char *text, size_t len, airy_word_fn fn, void *ud)
{
    if (!text || !fn || len == 0)
        return;

    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *end = p + len;
    char word[AIRY_WORD_MAX];
    size_t wlen = 0;

    while (p < end && *p) {
        int ch = word_char(*p);
        if (ch) {
            if (wlen < sizeof(word) - 1)
                word[wlen++] = (char)ch;
            p++;
            continue;
        }
        flush_word(word, &wlen, fn, ud);
        if (*p < 0x80) {
            p++;
            continue;
        }
        /* 多字节 UTF-8 序列：残缺尾部按剩余字节产出，非法前缀跳过 */
        size_t seq = utf8_seq_len(*p);
        size_t n = 0;
        while (n < seq && p + n < end && p[n])
            n++;
        if (n >= 2) {
            char buf[5] = {0};
            for (size_t i = 0; i < n; i++)
                buf[i] = (char)p[i];
            fn(ud, buf);
            p += n;
        } else {
            p++;
        }
    }
    flush_word(word, &wlen, fn, ud);
}
