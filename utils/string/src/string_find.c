// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_find.c
 * @brief Find and trim domain: substring/char search, whitespace trim,
 * case conversion and replacement.
 */

#include "airy_string.h"
#include "string_internal.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "error.h"

#ifdef _WIN32
#include <locale.h>
#include <windows.h>
#else
#include <locale.h>
#include <strings.h>
#endif

const char *string_find(const char *haystack, const char *needle, int options)
{
    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
        const char *h = haystack;
        size_t needle_len = strlen(needle);

        while (*h != '\0') {
            if (string_compare_n(h, needle, needle_len, options) == 0) {
                return h;
            }
            h++;
        }

        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    } else {
        return strstr(haystack, needle);
    }
}

const char *string_find_last(const char *haystack, const char *needle, int options)
{
    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);

    if (needle_len > haystack_len) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    const char *last_found = NULL;
    const char *current = haystack;

    while (*current != '\0') {
        if (string_compare_n(current, needle, needle_len, options) == 0) {
            last_found = current;
        }
        current++;
    }

    return last_found;
}

const char *string_find_char(const char *str, char ch)
{
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    return strchr(str, ch);
}

const char *string_find_char_last(const char *str, char ch)
{
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    return strrchr(str, ch);
}

char *string_trim(char *str)
{
    if (str == NULL || str[0] == '\0') {
        return str;
    }

    char *end = str + strlen(str) - 1;
    while (end >= str && string_is_whitespace_char(*end)) {
        *end = '\0';
        end--;
    }

    char *start = str;
    while (*start != '\0' && string_is_whitespace_char(*start)) {
        start++;
    }

    if (start != str) {
        size_t len = strlen(start) + 1;
        __builtin_memmove(str, start, len);
    }

    return str;
}

char *string_trim_start(char *str)
{
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    char *start = str;
    while (*start != '\0' && string_is_whitespace_char(*start)) {
        start++;
    }

    if (start != str) {
        size_t len = strlen(start) + 1;
        __builtin_memmove(str, start, len);
    }

    return str;
}

char *string_trim_end(char *str)
{
    if (str == NULL || str[0] == '\0') {
        return str;
    }

    char *end = str + strlen(str) - 1;
    while (end >= str && string_is_whitespace_char(*end)) {
        *end = '\0';
        end--;
    }

    return str;
}

char *string_to_lower(char *str)
{
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    char *p = str;
    while (*p != '\0') {
        *p = (char)tolower((unsigned char)*p);
        p++;
    }

    return str;
}

char *string_to_upper(char *str)
{
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    char *p = str;
    while (*p != '\0') {
        *p = (char)toupper((unsigned char)*p);
        p++;
    }

    return str;
}

int string_replace(const char *str, const char *old_substr, const char *new_substr, char *result,
                   size_t result_size)
{
    if (str == NULL || old_substr == NULL || new_substr == NULL || result == NULL ||
        result_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    size_t old_len = strlen(old_substr);
    size_t new_len = strlen(new_substr);

    size_t result_len = 0;
    const char *current = str;
    const char *next;

    while ((next = strstr(current, old_substr)) != NULL) {
        result_len += (next - current);
        result_len += new_len;
        current = next + old_len;
    }

    result_len += strlen(current);

    if (result_len >= result_size) {
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "buffer too small");
        return AIRY_EINVAL;
    }

    char *dest = result;
    current = str;

    while ((next = strstr(current, old_substr)) != NULL) {
        size_t copy_len = next - current;
        __builtin_memcpy(dest, current, copy_len);
        dest += copy_len;

        __builtin_memcpy(dest, new_substr, new_len);
        dest += new_len;

        current = next + old_len;
    }

    size_t remaining_len = strlen(current);
    __builtin_memcpy(dest, current, remaining_len);
    dest += remaining_len;
    *dest = '\0';

    return (int)result_len;
}
