// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file safe_string_utils.c
 * @brief Safe string handling utilities implementation.
 *
 * SP05 decoupling: migrated from daemons/common/src/ to
 * commons/utils/string/src/, removing protocols' physical dependency on
 * the daemons layer (ACC-SP03 decoupling point #4). The redundant
 * #include "svc_logger.h" was dropped during migration (this file does
 * not use SVC_LOG_* macros; logging comes via AIRY_ERROR/AIRY_ERROR_NULL
 * from commons/utils/error).
 */

#include "safe_string_utils.h"

#include "error.h" /* AIRY_ERROR / AIRY_ERROR_NULL */
#include "airy_memory.h" /* AIRY_MALLOC / AIRY_CALLOC / AIRY_REALLOC / AIRY_FREE */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int safe_strcpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0) {
        AIRY_ERROR(AIRY_ERR_INVALID_PARAM, "safe_strcpy: null parameter");
    }

    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        __builtin_memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
        AIRY_ERROR(AIRY_ERR_OVERFLOW, "safe_strcpy: buffer overflow");
    }

    __builtin_memcpy(dest, src, src_len + 1);
    return 0;
}

int safe_strcat(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0) {
        AIRY_ERROR(AIRY_ERR_INVALID_PARAM, "safe_strcat: null parameter");
    }

    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);

    /* dest_len + src_len may overflow size_t; and when dest_len >=
     * dest_size, dest_size - dest_len - 1 underflows. Check each before
     * the additions. */
    if (dest_len >= dest_size) {
        dest[dest_size - 1] = '\0';
        AIRY_ERROR(AIRY_ERR_OVERFLOW, "safe_strcat: buffer overflow");
    }

    if (src_len > dest_size - dest_len - 1) {
        size_t remaining = dest_size - dest_len - 1;
        __builtin_memcpy(dest + dest_len, src, remaining);
        dest[dest_len + remaining] = '\0';
        AIRY_ERROR(AIRY_ERR_OVERFLOW, "safe_strcat: buffer overflow");
    }

    __builtin_memcpy(dest + dest_len, src, src_len + 1);
    return 0;
}

int safe_sprintf(char *dest, size_t dest_size, const char *fmt, ...)
{
    if (!dest || !fmt || dest_size == 0) {
        AIRY_ERROR(AIRY_ERR_INVALID_PARAM, "safe_sprintf: null parameter");
    }

    va_list args;
    va_start(args, fmt);
    int written =
        vsnprintf(dest, dest_size, fmt,
                  args); /* flawfinder: ignore - safe_sprintf wrapper with bounds-checked dest */
    va_end(args);

    if (written < 0 || (size_t)written >= dest_size) {
        dest[dest_size - 1] = '\0';
        AIRY_ERROR(AIRY_ERR_PARSE_ERROR, "safe_sprintf: buffer overflow");
    }

    return written;
}

size_t safe_strlen(const char *str, size_t max_len)
{
    if (!str)
        return 0;
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0')
            return i;
    }
    return max_len;
}

int safe_strcmp(const char *str1, const char *str2, size_t max_len)
{
    if (!str1 && !str2)
        return 0;
    if (!str1) {
        AIRY_ERROR(AIRY_ERR_INVALID_PARAM, "safe_strcmp: null str1");
    }
    if (!str2)
        return 1;

    for (size_t i = 0; i < max_len; i++) {
        if (str1[i] == '\0' && str2[i] == '\0')
            return 0;
        if (str1[i] == '\0') {
            AIRY_ERROR(AIRY_ERR_PARSE_ERROR, "safe_strcmp: premature end of str1");
        }
        if (str2[i] == '\0')
            return 1;
        int diff = (unsigned char)str1[i] - (unsigned char)str2[i];
        if (diff != 0)
            return diff;
    }
    return 0;
}

char *safe_strdup_with_limit(const char *str, size_t max_copy_len)
{
    if (!str) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    size_t len = strlen(str);
    if (max_copy_len > 0 && len > max_copy_len)
        len = max_copy_len;

    char *copy = (char *)AIRY_MALLOC(len + 1);
    if (!copy) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    __builtin_memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

void secure_clear(void *buf, size_t size)
{
    if (!buf || size == 0)
        return;
    volatile unsigned char *p = (volatile unsigned char *)buf;
    for (size_t i = 0; i < size; i++)
        p[i] = 0;
}

bool validate_string_input(const char *str, size_t max_len)
{
    if (!str)
        return false;
    size_t len = 0;
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0') {
            len = i;
            break;
        }
        if (i == max_len - 1)
            return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
            return false;
    }
    return true;
}

bool validate_pointer(const void *ptr)
{
    return ptr != NULL;
}

bool validate_range(int64_t value, int64_t min_val, int64_t max_val)
{
    return value >= min_val && value <= max_val;
}

bool is_valid_ascii(const char *str, size_t len)
{
    if (!str)
        return false;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\0')
            return true;
        if ((unsigned char)str[i] > 0x7F)
            return false;
    }
    return true;
}

void *safe_malloc(size_t size, const char *purpose)
{
    /* purpose is kept for debug tracing (not a stub): the current
     * implementation does not consume it; mark it explicitly to avoid an
     * unused-parameter warning while preserving call-site semantics
     * (a future tracing module can consume it). */
    (void)purpose;
    if (size == 0) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    void *ptr = AIRY_MALLOC(size);
    return ptr;
}

void *safe_calloc(size_t count, size_t size, const char *purpose)
{
    (void)purpose;
    if (count == 0 || size == 0) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    if (count > SIZE_MAX / size) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    void *ptr = AIRY_CALLOC(count, size);
    return ptr;
}

void *safe_realloc(void *ptr, size_t new_size, const char *purpose)
{
    (void)purpose;
    if (new_size == 0) {
        AIRY_FREE(ptr);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    void *new_ptr = AIRY_REALLOC(ptr, new_size);
    return new_ptr;
}
