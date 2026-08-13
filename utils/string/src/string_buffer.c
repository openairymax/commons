// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_buffer.c
 * @brief Buffer container domain: dynamic string buffer create/append/
 * clear/query.
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

string_buffer_t *string_buffer_create(size_t initial_capacity, string_encoding_t encoding)
{
    if (initial_capacity == 0) {
        initial_capacity = 16;
    }

    string_buffer_t *buffer = (string_buffer_t *)AIRY_MALLOC(sizeof(string_buffer_t));
    if (buffer == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    buffer->data = (char *)AIRY_MALLOC(initial_capacity + 1);
    if (buffer->data == NULL) {
        AIRY_FREE(buffer);
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    buffer->data[0] = '\0';
    buffer->capacity = initial_capacity + 1;
    buffer->length = 0;
    buffer->encoding = encoding;
    buffer->gateway = true;

    return buffer;
}

void string_buffer_destroy(string_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (buffer->gateway && buffer->data != NULL) {
        AIRY_FREE(buffer->data);
    }

    AIRY_FREE(buffer);
}

bool string_buffer_append(string_buffer_t *buffer, const char *str)
{
    if (buffer == NULL || str == NULL) {
        return false;
    }

    return string_buffer_append_n(buffer, str, strlen(str));
}

bool string_buffer_append_n(string_buffer_t *buffer, const char *str, size_t len)
{
    if (buffer == NULL || str == NULL) {
        return false;
    }

    size_t new_length = buffer->length + len;
    if (new_length >= buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        while (new_capacity <= new_length) {
            new_capacity *= 2;
        }

        char *new_data = (char *)AIRY_REALLOC(buffer->data, new_capacity);
        if (new_data == NULL) {
            return false;
        }

        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    __builtin_memcpy(buffer->data + buffer->length, str, len);
    buffer->length = new_length;
    buffer->data[buffer->length] = '\0';

    return true;
}

bool string_buffer_append_format(string_buffer_t *buffer, const char *format, ...)
{
    if (buffer == NULL || format == NULL) {
        return false;
    }

    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return false;
    }

    size_t new_length = buffer->length + (size_t)needed;
    if (new_length >= buffer->capacity) {
        size_t new_capacity = buffer->capacity;
        while (new_capacity <= new_length) {
            new_capacity *= 2;
        }

        char *new_data = (char *)AIRY_REALLOC(buffer->data, new_capacity);
        if (new_data == NULL) {
            va_end(args);
            return false;
        }

        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    int result =
        vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);

    if (result < 0) {
        return false;
    }

    buffer->length += (size_t)result;
    return true;
}

bool string_buffer_append_char(string_buffer_t *buffer, char ch)
{
    if (buffer == NULL) {
        return false;
    }

    if (buffer->length + 1 >= buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        char *new_data = (char *)AIRY_REALLOC(buffer->data, new_capacity);
        if (new_data == NULL) {
            return false;
        }

        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    buffer->data[buffer->length] = ch;
    buffer->length++;
    buffer->data[buffer->length] = '\0';

    return true;
}

void string_buffer_clear(string_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }

    buffer->length = 0;
    if (buffer->data != NULL) {
        buffer->data[0] = '\0';
    }
}

const char *string_buffer_cstr(const string_buffer_t *buffer)
{
    if (buffer == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    return buffer->data;
}

size_t string_buffer_length(const string_buffer_t *buffer)
{
    if (buffer == NULL) {
        return 0;
    }

    return buffer->length;
}
