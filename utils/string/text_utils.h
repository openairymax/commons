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
 * @brief Tokenise text into lowercased keywords of more than two bytes.
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

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TEXT_UTILS_H */
