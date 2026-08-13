// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_format.c
 * @brief Format and allocate domain: safe formatting, heap copy/concat.
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

int string_format(char *buffer, size_t buffer_size, const char *format, ...)
{
    if (buffer == NULL || format == NULL || buffer_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    va_list args;
    va_start(args, format);
    int result = string_format_v(buffer, buffer_size, format, args);
    va_end(args);

    return result;
}

int string_format_v(char *buffer, size_t buffer_size, const char *format, va_list args)
{
    if (buffer == NULL || format == NULL || buffer_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return AIRY_EINVAL;
    }

    va_list args_copy;
    va_copy(args_copy, args);

    int result = vsnprintf(buffer, buffer_size, format, args_copy);
    va_end(args_copy);

    if (result < 0) {
        string_set_error(STRING_ERROR_FORMAT, "format failed");
        return AIRY_EINVAL;
    }

    if ((size_t)result >= buffer_size) {
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "buffer too small");
        return AIRY_EINVAL;
    }

    return result;
}

char *string_alloc_format(const char *format, ...)
{
    if (format == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    va_list args;
    va_start(args, format);
    char *result = string_alloc_format_v(format, args);
    va_end(args);

    return result;
}

char *string_alloc_format_v(const char *format, va_list args)
{
    if (format == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        string_set_error(STRING_ERROR_FORMAT, "format failed");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    char *buffer = (char *)AIRY_MALLOC((size_t)needed + 1);
    if (buffer == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    va_copy(args_copy, args);
    int result =
        vsnprintf(buffer, (size_t)needed + 1, format,
                  args_copy); /* flawfinder: ignore - variadic string wrapper with bounded buffer */
    va_end(args_copy);

    if (result < 0) {
        AIRY_FREE(buffer);
        string_set_error(STRING_ERROR_FORMAT, "format failed");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    return buffer;
}

char *string_alloc_copy(const char *str)
{
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    size_t len = strlen(str);
    char *copy = (char *)AIRY_MALLOC(len + 1);
    if (copy == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    __builtin_memcpy(copy, str, len);
    copy[len] = '\0';

    return copy;
}

char *string_alloc_copy_n(const char *str, size_t len)
{
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    size_t actual_len = string_safe_strlen(str, len);
    char *copy = (char *)AIRY_MALLOC(actual_len + 1);
    if (copy == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    __builtin_memcpy(copy, str, actual_len);
    copy[actual_len] = '\0';

    return copy;
}

char *string_alloc_concat(const char *str1, const char *str2)
{
    if (str1 == NULL && str2 == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    size_t len1 = (str1 != NULL) ? strlen(str1) : 0;
    size_t len2 = (str2 != NULL) ? strlen(str2) : 0;

    char *result = (char *)AIRY_MALLOC(len1 + len2 + 1);
    if (result == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    if (str1 != NULL) {
        __builtin_memcpy(result, str1, len1);
    }

    if (str2 != NULL) {
        __builtin_memcpy(result + len1, str2, len2);
    }

    result[len1 + len2] = '\0';

    return result;
}
