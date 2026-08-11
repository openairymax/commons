/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file safe_string_utils.h
 * @brief 安全字符串处理工具
 *
 * @details
 * 提供安全字符串操作函数，替代不安全的 strcpy/strcat/sprintf/gets 等。
 * 所有函数均进行边界检查和空指针验证，防止缓冲区溢出。
 * 遵循 AgentRT 安全编码规范 3.2.2 节要求。
 */

#ifndef AIRY_RT_SAFE_STRING_UTILS_H
#define AIRY_RT_SAFE_STRING_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


int safe_strcpy(char *dest, const char *src, size_t dest_size);


int safe_strcat(char *dest, const char *src, size_t dest_size);


int safe_sprintf(char *dest, size_t dest_size, const char *fmt, ...);


size_t safe_strlen(const char *str, size_t max_len);


int safe_strcmp(const char *str1, const char *str2, size_t max_len);


char *safe_strdup_with_limit(const char *str, size_t max_copy_len);


void secure_clear(void *buf, size_t size);


bool validate_string_input(const char *str, size_t max_len);


bool validate_pointer(const void *ptr);


bool validate_range(int64_t value, int64_t min_val, int64_t max_val);


bool is_valid_ascii(const char *str, size_t len);


void *safe_malloc(size_t size, const char *purpose);


void *safe_calloc(size_t count, size_t size, const char *purpose);


void *safe_realloc(void *ptr, size_t new_size, const char *purpose);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SAFE_STRING_UTILS_H */
