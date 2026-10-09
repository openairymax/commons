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
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
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

size_t airy_text_keywords(const char *text, char **keywords, size_t max_keywords)
{
    if (!text || !keywords || max_keywords == 0)
        return 0;

    size_t count = 0;
    char *copy = AIRY_STRDUP(text);
    if (!copy)
        return 0;

    char *saveptr = NULL;
    char *token = strtok_r(copy, " ,.!?;:\t\n\r", &saveptr);
    while (token && count < max_keywords) {
        if (strlen(token) > 2) {
            keywords[count] = AIRY_STRDUP(token);
            if (keywords[count]) {
                airy_text_lower(keywords[count]);
                count++;
            }
        }
        token = strtok_r(NULL, " ,.!?;:\t\n\r", &saveptr);
    }

    AIRY_FREE(copy);
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
