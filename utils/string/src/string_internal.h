// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_internal.h
 * @brief 字符串模块内部共享定义：内部错误码类型与跨文件辅助函数声明
 */

#ifndef AIRY_STRING_INTERNAL_H
#define AIRY_STRING_INTERNAL_H

#include "airy_string.h"

typedef enum {
    STRING_ERROR_NONE = 0,
    STRING_ERROR_INVALID_ARGUMENT,
    STRING_ERROR_BUFFER_TOO_SMALL,
    STRING_ERROR_MEMORY_ALLOCATION,
    STRING_ERROR_ENCODING_CONVERSION,
    STRING_ERROR_FORMAT,
    STRING_ERROR_OVERFLOW
} string_error_t;

void string_set_error(string_error_t error, const char *message);

void string_clear_error(void);

size_t string_safe_strlen(const char *str, size_t max_len);

bool string_is_whitespace_char(char ch);

#endif /* AIRY_STRING_INTERNAL_H */
