/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file text_utils.h
 * @brief Domain-agnostic text-algorithm utilities for the string SSoT.
 *
 * Case conversion, case-insensitive substring search, longest-common-
 * substring similarity and keyword tokenisation. These are generic
 * string algorithms shared by the cognition and intent domains, so they
 * live in the string module rather than in any single consumer domain.
 */

#ifndef AIRY_RT_TEXT_UTILS_H
#define AIRY_RT_TEXT_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert a string to lowercase in place.
 * @param str Input string
 * @return The converted string, or NULL when @p str is NULL
 */
char *airy_text_lower(char *str);

/**
 * @brief Test whether haystack contains needle, ignoring ASCII case.
 * @param haystack Source string
 * @param needle Substring to find
 * @return 1 when contained, 0 otherwise
 */
int airy_text_icontains(const char *haystack, const char *needle);

/**
 * @brief Longest-common-substring similarity, ignoring ASCII case.
 * @param s1 String 1
 * @param s2 String 2
 * @return Matching fraction in [0.0, 1.0]; 0.0 when either is NULL/empty
 */
float airy_text_similar(const char *s1, const char *s2);

/**
 * @brief Tokenise text into lowercased keywords (UTF-8 aware).
 *
 * 分隔符为 ASCII 空白/标点与 CJK 标点（U+2000–U+206F、U+3000–U+303F、
 * U+FF00–U+FF65、U+00B7）。词项须含至少 2 个码点，且至少含一个字母、
 * 数字或 CJK 表意字——单字虚词（在/的/了/和/与/用/把/将）与纯符号词
 * 不再入集，避免以虚词充当实体参与匹配。仅 ASCII 字母被小写化。
 * @param text Input text
 * @param keywords Output array receiving heap strings
 * @param max_keywords Capacity of @p keywords
 * @return Number of keywords stored; free with airy_text_kw_free()
 */
size_t airy_text_keywords(const char *text, char **keywords, size_t max_keywords);

/**
 * @brief Free a keyword array produced by airy_text_keywords().
 * @param keywords Keyword array
 * @param count Number of entries to free
 */
void airy_text_kw_free(char **keywords, size_t count);

/**
 * @brief 读取回填预算（字节），默认 8192，可用 AIRY_BACKFILL_BUDGET 覆盖。
 *
 * 取值夹在 [1024, 1 MiB]：过小会切掉错误上下文，过大会让链事件与回填
 * 撑爆单条消息。环境值非法时回落默认值。
 */
size_t airy_text_budget_bytes(void);

/**
 * @brief 按预算保留头尾，中间以省略标记替换，结果为堆字符串。
 *
 * 用于工具输出回填：长输出若整段进上下文会挤掉后续轮次，字节级截断
 * 又会把多字节字符切半（LLM 接口以 400 拒绝非法 UTF-8）。这里保留头
 * 3/5 与尾 2/5，两处都回退到完整 UTF-8 字符边界，中间写入含丢弃字节
 * 数的标记。@p budget 为 0 时取 airy_text_budget_bytes()；未超预算则
 * 原样复制。返回 NULL 表示入参为 NULL 或分配失败。
 */
char *airy_text_budget(const char *s, size_t budget);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TEXT_UTILS_H */
