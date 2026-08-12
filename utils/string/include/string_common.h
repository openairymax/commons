/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file string_common.h
 * @brief Common string utility library.
 *
 * Provides unified string operation interfaces, including:
 * - String copy and concatenation
 * - String comparison
 * - String search and replacement
 * - String conversion
 * - String memory management
 */

#ifndef STRING_COMMON_H
#define STRING_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe string copy function
 * @param dest Destination string
 * @param dest_size Destination string size
 * @param src Source string
 * @return Destination string pointer
 */
char *string_common_strlcpy(char *dest, size_t dest_size, const char *src);

/**
 * @brief Safe string concatenation function
 * @param dest Destination string
 * @param dest_size Destination string size
 * @param src Source string
 * @return Destination string pointer
 */
char *string_common_strlcat(char *dest, size_t dest_size, const char *src);

/**
 * @brief String copy (dynamic memory allocation)
 * @param str Source string
 * @return Copied string pointer, release with free()
 */
char *string_common_strdup(const char *str);

/**
 * @brief String copy with length limit (dynamic memory allocation)
 * @param str Source string
 * @param n Maximum copy length
 * @return Copied string pointer, release with free()
 */
char *string_common_strndup(const char *str, size_t n);

/**
 * @brief Case-insensitive string comparison
 * @param s1 String 1
 * @param s2 String 2
 * @return Comparison result
 */
int string_common_strcasecmp(const char *s1, const char *s2);

/**
 * @brief Case-insensitive string comparison (bounded length)
 * @param s1 String 1
 * @param s2 String 2
 * @param n Maximum comparison length
 * @return Comparison result
 */
int string_common_strncasecmp(const char *s1, const char *s2, size_t n);

/**
 * @brief String search
 * @param haystack String to search
 * @param needle Substring to find
 * @return Pointer to the substring position, NULL if not found
 */
char *string_common_strstr(const char *haystack, const char *needle);

/**
 * @brief String split
 * @param str String to split
 * @param delim Delimiter
 * @return Split string array, last element is NULL
 */
char **string_common_strsplit(const char *str, const char *delim);

/**
 * @brief Free a string array
 * @param arr String array
 */
void string_common_strsplit_free(char **arr);

/**
 * @brief Convert a string to an integer
 * @param str String
 * @param base Base
 * @param result Conversion result
 * @return true on success, false on failure
 */
bool string_common_strtoint(const char *str, int base, int *result);

/**
 * @brief Convert a string to an unsigned integer
 * @param str String
 * @param base Base
 * @param result Conversion result
 * @return true on success, false on failure
 */
bool string_common_strtouint(const char *str, int base, uint32_t *result);

/**
 * @brief Convert a string to a double
 * @param str String
 * @param result Conversion result
 * @return true on success, false on failure
 */
bool string_common_strtod(const char *str, double *result);

/**
 * @brief Convert an integer to a string
 * @param value Integer value
 * @param base Base
 * @param buf Buffer
 * @param buf_size Buffer size
 * @return Converted string length
 */
size_t string_common_itoa(int value, int base, char *buf, size_t buf_size);

/**
 * @brief Convert an unsigned integer to a string
 * @param value Unsigned integer value
 * @param base Base
 * @param buf Buffer
 * @param buf_size Buffer size
 * @return Converted string length
 */
size_t string_common_utoa(uint32_t value, int base, char *buf, size_t buf_size);

/**
 * @brief Convert a double to a string
 * @param value Double value
 * @param precision Fractional digits
 * @param buf Buffer
 * @param buf_size Buffer size
 * @return Converted string length
 */
size_t string_common_ftoa(double value, int precision, char *buf, size_t buf_size);

/**
 * @brief String trim (removes leading/trailing whitespace)
 * @param str String
 * @return Trimmed string pointer
 */
char *string_common_strtrim(char *str);

/**
 * @brief Convert a string to lowercase
 * @param str String
 * @return Converted string pointer
 */
char *string_common_strtolower(char *str);

/**
 * @brief Convert a string to uppercase
 * @param str String
 * @return Converted string pointer
 */
char *string_common_strtoupper(char *str);

/**
 * @brief JSON string escaping
 * @param src Source string
 * @param out Escaped string output (dynamically allocated; caller frees)
 * @return 0 on success, -1 on failure
 */
int string_common_json_escape(const char *src, char **out);

/**
 * @brief JSON string escaping (fixed-buffer version)
 * @param src Source string
 * @param dst Destination buffer
 * @param dst_size Destination buffer size
 * @return Number of characters written
 */
size_t string_common_json_escape_buf(const char *src, char *dst, size_t dst_size);

#ifdef __cplusplus
}
#endif

#endif /* STRING_COMMON_H */
