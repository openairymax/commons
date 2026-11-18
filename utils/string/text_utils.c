// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file text_utils.c
 * @brief Domain-agnostic text-algorithm utilities implementation.
 *
 * Relocated from cognition/think/intent/intent_utils.c during the 0.1.19
 * mechanism/policy split: these are generic string algorithms consumed by
 * both the cognition and intent domains, so they belong to the string SSoT
 * rather than to the intent domain. The redundant #include "cognition.h"
 * was dropped on relocation (no cognition symbol is referenced here).
 */

#include "text_utils.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "safe_utf8.h"
#include "string_compat.h"

char *airy_text_lower(char *str)
{
    if (!str)
        return NULL;
    for (char *p = str; *p; ++p) {
        *p = (char)tolower((unsigned char)*p);
    }
    return str;
}

int airy_text_icontains(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return 0;

    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);

    if (needle_len > haystack_len)
        return 0;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match)
            return 1;
    }
    return 0;
}

float airy_text_similar(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return 0.0f;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t denom = (len1 > len2) ? len1 : len2;
    if (denom == 0)
        return 0.0f;

    size_t longest = 0;
    for (size_t i = 0; i < len1; i++) {
        for (size_t j = 0; j < len2; j++) {
            size_t k = 0;
            while (i + k < len1 && j + k < len2 &&
                   tolower((unsigned char)s1[i + k]) ==
                       tolower((unsigned char)s2[j + k])) {
                k++;
            }
            if (k > longest)
                longest = k;
        }
    }

    return (float)longest / (float)denom;
}

/* UTF-8 码点解码：合法序列按 1~4 字节消费；截断/非法首字节按单字节
 * 处理，避免在残缺序列上读到 NUL 之后。 */
static size_t kw_utf8_dec(const unsigned char *s, uint32_t *cp)
{
    if (s[0] < 0x80) {
        *cp = s[0];
        return 1;
    }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
        return 2;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) |
              (uint32_t)(s[2] & 0x3F);
        return 3;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
        (s[3] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
              ((uint32_t)(s[2] & 0x3F) << 6) | (uint32_t)(s[3] & 0x3F);
        return 4;
    }
    *cp = s[0];
    return 1;
}

/* 分隔判定：ASCII 沿用旧口径，另把 CJK 标点区段视为分隔，避免整句
 * 汉字粘成一个词项。 */
static int kw_is_sep(uint32_t cp)
{
    if (cp < 0x80)
        return strchr(" ,.!?;:\t\n\r", (int)cp) != NULL;
    return (cp >= 0x2000 && cp <= 0x206F) || (cp >= 0x3000 && cp <= 0x303F) ||
           (cp >= 0xFF00 && cp <= 0xFF65) || cp == 0x00B7;
}

/* 词项判定：≥2 码点且至少一个字母/数字/CJK 表意字；单字虚词因此
 * 天然出局，纯符号串也不入集。 */
static int kw_accept(const char *tok)
{
    const unsigned char *p = (const unsigned char *)tok;
    size_t n = 0;
    int content = 0;

    while (*p) {
        uint32_t cp = 0;
        size_t len = kw_utf8_dec(p, &cp);

        n++;
        if (cp < 0x80 ? isalnum((int)cp) != 0
                      : (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
                            (cp >= 0xF900 && cp <= 0xFAFF))
            content = 1;
        p += len;
    }
    return n >= 2 && content;
}

/* 收尾当前词项：命中小写入集，缓冲清零供下一词使用。 */
static void kw_flush(char *tok, size_t *tlen, char **keywords, size_t *count,
                     size_t max_keywords)
{
    if (*tlen == 0)
        return;
    tok[*tlen] = '\0';
    *tlen = 0;
    if (*count >= max_keywords || !kw_accept(tok))
        return;
    keywords[*count] = AIRY_STRDUP(tok);
    if (!keywords[*count])
        return;
    airy_text_lower(keywords[*count]);
    (*count)++;
}

size_t airy_text_keywords(const char *text, char **keywords, size_t max_keywords)
{
    if (!text || !keywords || max_keywords == 0)
        return 0;

    size_t count = 0;
    size_t tlen = 0;
    size_t cap = strlen(text) + 1;
    char *tok = (char *)AIRY_MALLOC(cap);
    if (!tok)
        return 0;

    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        uint32_t cp = 0;
        size_t len = kw_utf8_dec(p, &cp);

        if (kw_is_sep(cp)) {
            kw_flush(tok, &tlen, keywords, &count, max_keywords);
            p += len;
            continue;
        }
        if (tlen + len >= cap)
            break;
        AIRY_MEMCPY(tok + tlen, p, len);
        tlen += len;
        p += len;
    }
    kw_flush(tok, &tlen, keywords, &count, max_keywords);
    AIRY_FREE(tok);
    return count;
}

void airy_text_kw_free(char **keywords, size_t count)
{
    if (!keywords)
        return;
    for (size_t i = 0; i < count; i++) {
        if (keywords[i])
            AIRY_FREE(keywords[i]);
    }
}

#define AIRY_TEXT_BUDGET_DEF 8192u
#define AIRY_TEXT_BUDGET_MIN 1024u
#define AIRY_TEXT_BUDGET_MAX (1024u * 1024u)

size_t text_budget(void)
{
    const char *v = getenv("AIRY_BACKFILL_BUDGET");
    if (!v || !v[0])
        return AIRY_TEXT_BUDGET_DEF;
    char *end = NULL;
    unsigned long n = strtoul(v, &end, 10);
    if (end == v || (end && *end != '\0'))
        return AIRY_TEXT_BUDGET_DEF;
    if (n < AIRY_TEXT_BUDGET_MIN)
        return AIRY_TEXT_BUDGET_MIN;
    if (n > AIRY_TEXT_BUDGET_MAX)
        return AIRY_TEXT_BUDGET_MAX;
    return (size_t)n;
}

char *airy_text_budget(const char *s, size_t budget)
{
    if (!s)
        return NULL;
    size_t len = strlen(s);
    if (budget == 0)
        budget = text_budget();
    if (len <= budget)
        return AIRY_STRDUP(s);

    size_t head_max = budget * 3 / 5;
    size_t tail_max = budget - head_max;
    size_t head_len = utf8_prefix_len(s, head_max);
    size_t tail_pos = len - tail_max;
    while (tail_pos < len && ((unsigned char)s[tail_pos] & 0xC0) == 0x80)
        tail_pos++;
    if (tail_pos <= head_len)
        tail_pos = head_len;

    size_t dropped = len - head_len - (len - tail_pos);
    char mark[64];
    int mlen = snprintf(mark, sizeof(mark), "\n…[truncated %zu bytes]…\n", dropped);
    if (mlen <= 0)
        return AIRY_STRDUP(s);

    size_t tail_len = len - tail_pos;
    size_t total = head_len + (size_t)mlen + tail_len;
    char *out = (char *)AIRY_MALLOC(total + 1);
    if (!out)
        return NULL;
    __builtin_memcpy(out, s, head_len);
    __builtin_memcpy(out + head_len, mark, (size_t)mlen);
    __builtin_memcpy(out + head_len + (size_t)mlen, s + tail_pos, tail_len);
    out[total] = '\0';
    return out;
}
