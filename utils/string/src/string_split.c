// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_split.c
 * @brief 切分与连接域：字符串分割、连接以及前后缀/空白判断
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

string_list_t string_split(const char *str, const char *delimiter, int options, size_t limit)
{
    string_list_t list = string_list_create(8);

    if (str == NULL || delimiter == NULL || delimiter[0] == '\0') {
        return list;
    }

    size_t delimiter_len = strlen(delimiter);
    const char *start = str;
    const char *end;
    size_t count = 0;

    while (*start != '\0' && (limit == 0 || count < limit - 1)) {
        end = strstr(start, delimiter);

        if (end == NULL) {
            end = str + strlen(str);
        }

        const char *token_start = start;
        const char *token_end = end;

        if (options & STRING_SPLIT_TRIM_WHITESPACE) {
            while (token_start < token_end && string_is_whitespace_char(*token_start)) {
                token_start++;
            }

            while (token_end > token_start && string_is_whitespace_char(*(token_end - 1))) {
                token_end--;
            }
        }

        size_t trimmed_len = token_end - token_start;

        if (trimmed_len > 0 || (options & STRING_SPLIT_KEEP_EMPTY)) {
            string_view_t view =
                string_view_create_n(token_start, trimmed_len, STRING_ENCODING_UTF8);
            string_list_add(&list, &view);
            count++;
        }

        if (end == NULL || *end == '\0') {
            break;
        }

        start = end + delimiter_len;
    }

    if (*start != '\0' && (limit == 0 || count < limit)) {
        size_t token_len = strlen(start);
        const char *token_start = start;
        const char *token_end = start + token_len;

        if (options & STRING_SPLIT_TRIM_WHITESPACE) {
            while (token_start < token_end && string_is_whitespace_char(*token_start)) {
                token_start++;
            }

            while (token_end > token_start && string_is_whitespace_char(*(token_end - 1))) {
                token_end--;
            }
        }

        size_t trimmed_len = token_end - token_start;

        if (trimmed_len > 0 || (options & STRING_SPLIT_KEEP_EMPTY)) {
            string_view_t view =
                string_view_create_n(token_start, trimmed_len, STRING_ENCODING_UTF8);
            string_list_add(&list, &view);
        }
    }

    return list;
}

int string_join(const string_list_t *list, const char *delimiter, char *result, size_t result_size)
{
    if (list == NULL || result == NULL || result_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    size_t delimiter_len = (delimiter != NULL) ? strlen(delimiter) : 0;

    size_t total_len = 0;
    for (size_t i = 0; i < list->count; i++) {
        total_len += list->items[i].length;
        if (i < list->count - 1 && delimiter_len > 0) {
            total_len += delimiter_len;
        }
    }

    if (total_len >= result_size) {
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "buffer too small");
        return AIRY_EINVAL;
    }

    char *dest = result;
    for (size_t i = 0; i < list->count; i++) {
        const string_view_t *item = &list->items[i];

        __builtin_memcpy(dest, item->data, item->length);
        dest += item->length;

        if (i < list->count - 1 && delimiter_len > 0) {
            __builtin_memcpy(dest, delimiter, delimiter_len);
            dest += delimiter_len;
        }
    }

    *dest = '\0';

    return (int)total_len;
}

bool string_starts_with(const char *str, const char *prefix, int options)
{
    if (str == NULL || prefix == NULL) {
        return false;
    }

    size_t prefix_len = strlen(prefix);
    return string_compare_n(str, prefix, prefix_len, options) == 0;
}

bool string_ends_with(const char *str, const char *suffix, int options)
{
    if (str == NULL || suffix == NULL) {
        return false;
    }

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return false;
    }

    const char *str_suffix = str + (str_len - suffix_len);
    return string_compare(str_suffix, suffix, options) == 0;
}

bool string_is_blank(const char *str)
{
    if (str == NULL) {
        return true;
    }

    while (*str != '\0') {
        if (!string_is_whitespace_char(*str)) {
            return false;
        }
        str++;
    }

    return true;
}
