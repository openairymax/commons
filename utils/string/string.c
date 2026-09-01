// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string.c
 * @brief Unified string handling module - core layer implementation.
 * Provides safe, efficient, unified string handling with a full string
 * operation API: copy, concat, compare, search, split, format, etc.
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

/**
 * @brief Whitespace character definition.
 */
static const char *__attribute__((unused)) WHITESPACE_CHARS = " \t\n\r\v\f";

/**
 * @brief Default string formatting options.
 */
static const string_format_options_t
    __attribute__((unused)) DEFAULT_FORMAT_OPTIONS = {.initial_buffer_size = 256,
                                                      .max_buffer_size = 0,
                                                      .locale_aware = false,
                                                      .null_string = "(null)",
                                                      .error_string = "(error)"};

typedef struct {
    string_error_t last_error;
    char error_message[256];
    bool initialized;
} string_context_t;

static string_context_t g_context = {.last_error = STRING_ERROR_NONE,
                                     .error_message = {0},
                                     .initialized = true};

void string_set_error(string_error_t error, const char *message)
{
    g_context.last_error = error;
    if (message != NULL) {
        AIRY_STRNCPY_TERM(g_context.error_message, message, sizeof(g_context.error_message));
        g_context.error_message[sizeof(g_context.error_message) - 1] = '\0';
    }
}

void string_clear_error(void)
{
    g_context.last_error = STRING_ERROR_NONE;
    g_context.error_message[0] = '\0';
}

size_t string_safe_strlen(const char *str, size_t max_len)
{
    if (str == NULL) {
        return 0;
    }

    size_t len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }

    return len;
}

bool string_is_whitespace_char(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f');
}

int string_copy(char *dest, const char *src, size_t dest_size)
{
    string_clear_error();

    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    size_t src_len = string_safe_strlen(src, dest_size - 1);

    if (src_len >= dest_size) {
        __builtin_memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "buffer too small");
        return AIRY_EINVAL;
    }

    __builtin_memcpy(dest, src, src_len);
    dest[src_len] = '\0';

    return (int)src_len;
}

int string_copy_n(char *dest, const char *src, size_t count, size_t dest_size)
{
    string_clear_error();

    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    size_t src_len = string_safe_strlen(src, count);
    size_t copy_len = (src_len < count) ? src_len : count;

    if (copy_len >= dest_size) {
        size_t actual_copy = (dest_size > 0) ? dest_size - 1 : 0;
        __builtin_memcpy(dest, src, actual_copy);
        if (dest_size > 0) {
            dest[actual_copy] = '\0';
        }
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "buffer too small");
        return AIRY_EINVAL;
    }

    __builtin_memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return (int)copy_len;
}

int string_concat(char *dest, const char *src, size_t dest_size)
{
    string_clear_error();

    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    size_t dest_len = string_safe_strlen(dest, dest_size);
    size_t src_len = string_safe_strlen(src, dest_size - dest_len);

    if (dest_len + src_len >= dest_size) {
        size_t available = dest_size - dest_len - 1;
        if (available > 0) {
            __builtin_memcpy(dest + dest_len, src, available);
            dest[dest_len + available] = '\0';
        }
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "buffer too small");
        return AIRY_EINVAL;
    }

    __builtin_memcpy(dest + dest_len, src, src_len);
    dest[dest_len + src_len] = '\0';

    return (int)(dest_len + src_len);
}

int string_concat_n(char *dest, const char *src, size_t count, size_t dest_size)
{
    string_clear_error();

    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    size_t dest_len = string_safe_strlen(dest, dest_size);
    size_t src_len = string_safe_strlen(src, count);
    size_t copy_len = (src_len < count) ? src_len : count;

    if (dest_len + copy_len >= dest_size) {
        size_t available = dest_size - dest_len - 1;
        if (available > 0) {
            __builtin_memcpy(dest + dest_len, src, available);
            dest[dest_len + available] = '\0';
        }
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "buffer too small");
        return AIRY_EINVAL;
    }

    __builtin_memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';

    return (int)(dest_len + copy_len);
}

int string_compare(const char *str1, const char *str2, int options)
{
    if (str1 == str2) {
        return 0;
    }

    if (str1 == NULL) {
        return AIRY_EINVAL;
    }

    if (str2 == NULL) {
        return 1;
    }

    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
#ifdef _WIN32
        return _stricmp(str1, str2);
#else
        return strcasecmp(str1, str2);
#endif
    } else {
        return strcmp(str1, str2);
    }
}

int string_compare_n(const char *str1, const char *str2, size_t len, int options)
{
    if (str1 == str2 || len == 0) {
        return 0;
    }

    if (str1 == NULL) {
        return AIRY_EINVAL;
    }

    if (str2 == NULL) {
        return 1;
    }

    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
#ifdef _WIN32
        return _strnicmp(str1, str2, len);
#else
        return strncasecmp(str1, str2, len);
#endif
    } else {
        return strncmp(str1, str2, len);
    }
}

size_t string_length(const char *str, size_t max_len)
{
    if (str == NULL) {
        return 0;
    }

    return string_safe_strlen(str, max_len);
}
