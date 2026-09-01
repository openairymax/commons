/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file airy_string.h
 * @brief Unified string handling module: core-layer API.
 *
 * Provides safe, efficient, and unified string handling interfaces that
 * avoid common issues such as buffer overflows. The module aims to
 * eliminate scattered string-handling code across the project and provide
 * a consistent string operation policy.
 *
 * @note Thread safety: all public interfaces are thread-safe
 * @see ARCHITECTURAL_PRINCIPLES.md E-1 security-by-design principle
 */

#ifndef AIRY_RT_STRING_H
#define AIRY_RT_STRING_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup string_api String handling API
 * @{
 */

/**
 * @brief String encoding types
 */
typedef enum {
    STRING_ENCODING_ASCII,
    STRING_ENCODING_UTF8,
    STRING_ENCODING_UTF16_LE,
    STRING_ENCODING_UTF16_BE,
    STRING_ENCODING_UTF32_LE,
    STRING_ENCODING_UTF32_BE,
    STRING_ENCODING_LATIN1, /**< Latin-1 (ISO-8859-1) */
    STRING_ENCODING_WINDOWS_1252 /**< Windows-1252 */
} string_encoding_t;

/**
 * @brief String comparison options
 */
typedef enum {
    STRING_COMPARE_CASE_SENSITIVE = 0,
    STRING_COMPARE_CASE_INSENSITIVE = 1,
    STRING_COMPARE_NATURAL = 2,
    STRING_COMPARE_LOCALE_AWARE = 4
} string_compare_option_t;

/**
 * @brief String split options
 */
typedef enum {
    STRING_SPLIT_KEEP_EMPTY = 1,
    STRING_SPLIT_TRIM_WHITESPACE = 2,
    STRING_SPLIT_LIMIT_COUNT = 4
} string_split_option_t;

/**
 * @brief String buffer structure
 */
typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    string_encoding_t encoding;
    bool gateway;
} string_buffer_t;

/**
 * @brief String view structure (does not own the data)
 */
typedef struct {
    const char *data;
    size_t length;
    string_encoding_t encoding;
} string_view_t;

/**
 * @brief String list structure
 */
typedef struct {
    string_view_t *items;
    size_t count;
    size_t capacity;
} string_list_t;

/**
 * @brief String formatting options
 */
typedef struct {
    size_t initial_buffer_size;
    size_t max_buffer_size;
    bool locale_aware;
    const char *null_string;
    const char *error_string;
} string_format_options_t;

/**
 * @brief Safely copy a string into a buffer
 *
 * @param[out] dest Destination buffer
 * @param[in] src Source string
 * @param[in] dest_size Destination buffer size (bytes)
 * @return Number of characters copied (excluding the NUL) on success,
 *         -1 on failure
 *
 * @note Guarantees NUL termination of the destination buffer
 * @note If the destination buffer is too small, copies as many characters
 *       as possible and returns -1
 */
int string_copy(char *dest, const char *src, size_t dest_size);

/**
 * @brief Safely copy a bounded-length string into a buffer
 *
 * @param[out] dest Destination buffer
 * @param[in] src Source string
 * @param[in] count Maximum number of characters to copy
 * @param[in] dest_size Destination buffer size (bytes)
 * @return Number of characters copied (excluding the NUL) on success,
 *         -1 on failure
 */
int string_copy_n(char *dest, const char *src, size_t count, size_t dest_size);

/**
 * @brief Safely concatenate a string onto a buffer
 *
 * @param[inout] dest Destination buffer (must be NUL-terminated)
 * @param[in] src Source string
 * @param[in] dest_size Total destination buffer size (bytes)
 * @return Total length after concatenation on success, -1 on failure
 */
int string_concat(char *dest, const char *src, size_t dest_size);

/**
 * @brief Safely concatenate a bounded-length string onto a buffer
 *
 * @param[inout] dest Destination buffer (must be NUL-terminated)
 * @param[in] src Source string
 * @param[in] count Maximum number of characters to concatenate
 * @param[in] dest_size Total destination buffer size (bytes)
 * @return Total length after concatenation on success, -1 on failure
 */
int string_concat_n(char *dest, const char *src, size_t count, size_t dest_size);

/**
 * @brief Compare two strings
 *
 * @param[in] str1 First string
 * @param[in] str2 Second string
 * @param[in] options Comparison options (bitmask of STRING_COMPARE_*)
 * @return 0 if equal, negative if str1 < str2, positive if str1 > str2
 */
int string_compare(const char *str1, const char *str2, int options);

/**
 * @brief Compare two strings (bounded length)
 *
 * @param[in] str1 First string
 * @param[in] str2 Second string
 * @param[in] len Maximum number of characters to compare
 * @param[in] options Comparison options (bitmask of STRING_COMPARE_*)
 * @return 0 if equal, negative if str1 < str2, positive if str1 > str2
 */
int string_compare_n(const char *str1, const char *str2, size_t len, int options);

/**
 * @brief Compute a string's length (safe version)
 *
 * @param[in] str String
 * @param[in] max_len Maximum length to check (guards against unbounded strings)
 * @return String length (excluding the NUL), or max_len if longer
 */
size_t string_length(const char *str, size_t max_len);

/**
 * @brief Find a substring
 *
 * @param[in] haystack String to search
 * @param[in] needle Substring to find
 * @param[in] options Comparison options (bitmask of STRING_COMPARE_*)
 * @return Pointer to the substring start, or NULL if not found
 */
const char *string_find(const char *haystack, const char *needle, int options);

/**
 * @brief Find a substring from the end
 *
 * @param[in] haystack String to search
 * @param[in] needle Substring to find
 * @param[in] options Comparison options (bitmask of STRING_COMPARE_*)
 * @return Pointer to the substring start, or NULL if not found
 */
const char *string_find_last(const char *haystack, const char *needle, int options);

/**
 * @brief Find the first occurrence of a character
 *
 * @param[in] str String
 * @param[in] ch Character to find
 * @return Pointer to the character, or NULL if not found
 */
const char *string_find_char(const char *str, char ch);

/**
 * @brief Find the last occurrence of a character
 *
 * @param[in] str String
 * @param[in] ch Character to find
 * @return Pointer to the character, or NULL if not found
 */
const char *string_find_char_last(const char *str, char ch);

/**
 * @brief Trim leading and trailing whitespace
 *
 * @param[inout] str String to trim (modified in place)
 * @return Trimmed string (points into the original string)
 */
char *string_trim(char *str);

/**
 * @brief Trim leading whitespace
 *
 * @param[inout] str String to trim (modified in place)
 * @return Trimmed string (points into the original string)
 */
char *string_trim_start(char *str);

/**
 * @brief Trim trailing whitespace
 *
 * @param[inout] str String to trim (modified in place)
 * @return Trimmed string (points into the original string)
 */
char *string_trim_end(char *str);

/**
 * @brief Convert a string to lowercase
 *
 * @param[inout] str String to convert (modified in place)
 * @return Converted string
 */
char *string_to_lower(char *str);

/**
 * @brief Convert a string to uppercase
 *
 * @param[inout] str String to convert (modified in place)
 * @return Converted string
 */
char *string_to_upper(char *str);

/**
 * @brief Replace a substring in a string
 *
 * @param[in] str Original string
 * @param[in] old_substr Substring to replace
 * @param[in] new_substr Replacement substring
 * @param[out] result Result buffer
 * @param[in] result_size Result buffer size
 * @return Result string length on success, -1 on failure
 *
 * @note If the result buffer is too small, copies as many characters as
 *       possible and returns -1
 */
int string_replace(const char *str, const char *old_substr, const char *new_substr, char *result,
                   size_t result_size);

/**
 * @brief Split a string
 *
 * @param[in] str String to split
 * @param[in] delimiter Delimiter
 * @param[in] options Split options (bitmask of STRING_SPLIT_*)
 * @param[in] limit Maximum number of splits (if STRING_SPLIT_LIMIT_COUNT is set)
 * @return String list; must be released with string_list_free after use
 */
string_list_t string_split(const char *str, const char *delimiter, int options, size_t limit);

/**
 * @brief Join a string list
 *
 * @param[in] list String list
 * @param[in] delimiter Delimiter
 * @param[out] result Result buffer
 * @param[in] result_size Result buffer size
 * @return Result string length on success, -1 on failure
 */
int string_join(const string_list_t *list, const char *delimiter, char *result, size_t result_size);

/**
 * @brief Check whether a string starts with a prefix
 *
 * @param[in] str String
 * @param[in] prefix Prefix
 * @param[in] options Comparison options (bitmask of STRING_COMPARE_*)
 * @return true if the string starts with the prefix, false otherwise
 */
bool string_starts_with(const char *str, const char *prefix, int options);

/**
 * @brief Check whether a string ends with a suffix
 *
 * @param[in] str String
 * @param[in] suffix Suffix
 * @param[in] options Comparison options (bitmask of STRING_COMPARE_*)
 * @return true if the string ends with the suffix, false otherwise
 */
bool string_ends_with(const char *str, const char *suffix, int options);

/**
 * @brief Check whether a string contains only whitespace
 *
 * @param[in] str String
 * @return true if the string contains only whitespace, false otherwise
 */
bool string_is_blank(const char *str);

/**
 * @brief Check whether a string contains only digits
 *
 * @param[in] str String
 * @return true if the string contains only digits, false otherwise
 */
bool string_is_digit(const char *str);

/**
 * @brief Check whether a string contains only alphabetic characters
 *
 * @param[in] str String
 * @return true if the string contains only alphabetic characters, false otherwise
 */
bool string_is_alpha(const char *str);

/**
 * @brief Check whether a string contains only alphanumeric characters
 *
 * @param[in] str String
 * @return true if the string contains only alphanumeric characters, false otherwise
 */
bool string_is_alnum(const char *str);

/**
 * @brief Format a string (safe version)
 *
 * @param[out] buffer Output buffer
 * @param[in] buffer_size Buffer size
 * @param[in] format Format string
 * @param[in] ... Format arguments
 * @return Number of characters written (excluding the NUL) on success, -1 on failure
 */
int string_format(char *buffer, size_t buffer_size, const char *format, ...);

/**
 * @brief Format a string (va_list version)
 *
 * @param[out] buffer Output buffer
 * @param[in] buffer_size Buffer size
 * @param[in] format Format string
 * @param[in] args Format arguments
 * @return Number of characters written (excluding the NUL) on success, -1 on failure
 */
int string_format_v(char *buffer, size_t buffer_size, const char *format, va_list args);

/**
 * @brief Allocate and format a string
 *
 * @param[in] format Format string
 * @param[in] ... Format arguments
 * @return Allocated string on success, NULL on failure
 *
 * @note The returned string must be released with free
 */
char *string_alloc_format(const char *format, ...);

/**
 * @brief Allocate and format a string (va_list version)
 *
 * @param[in] format Format string
 * @param[in] args Format arguments
 * @return Allocated string on success, NULL on failure
 */
char *string_alloc_format_v(const char *format, va_list args);

/**
 * @brief Copy a string (allocates new memory)
 *
 * @param[in] str Source string
 * @return Copied string on success, NULL on failure
 *
 * @note The returned string must be released with free
 */
char *string_alloc_copy(const char *str);

/**
 * @brief Copy a bounded-length string (allocates new memory)
 *
 * @param[in] str Source string
 * @param[in] len Maximum length to copy
 * @return Copied string on success, NULL on failure
 */
char *string_alloc_copy_n(const char *str, size_t len);

/**
 * @brief Concatenate strings (allocates new memory)
 *
 * @param[in] str1 First string
 * @param[in] str2 Second string
 * @return Concatenated string on success, NULL on failure
 */
char *string_alloc_concat(const char *str1, const char *str2);

/**
 * @brief Create a string buffer
 *
 * @param[in] initial_capacity Initial capacity (excluding the NUL)
 * @param[in] encoding String encoding
 * @return String buffer on success, NULL on failure
 */
string_buffer_t *string_buffer_create(size_t initial_capacity, string_encoding_t encoding);

/**
 * @brief Destroy a string buffer
 *
 * @param[in] buffer String buffer
 */
void string_buffer_destroy(string_buffer_t *buffer);

/**
 * @brief Append a string to a string buffer
 *
 * @param[in] buffer String buffer
 * @param[in] str String to append
 * @return true on success, false on failure
 */
bool string_buffer_append(string_buffer_t *buffer, const char *str);

/**
 * @brief Append a bounded-length string to a string buffer
 *
 * @param[in] buffer String buffer
 * @param[in] str String to append
 * @param[in] len Length to append
 * @return true on success, false on failure
 */
bool string_buffer_append_n(string_buffer_t *buffer, const char *str, size_t len);

/**
 * @brief Append a formatted string to a string buffer
 *
 * @param[in] buffer String buffer
 * @param[in] format Format string
 * @param[in] ... Format arguments
 * @return true on success, false on failure
 */
bool string_buffer_append_format(string_buffer_t *buffer, const char *format, ...);

/**
 * @brief Append a character to a string buffer
 *
 * @param[in] buffer String buffer
 * @param[in] ch Character to append
 * @return true on success, false on failure
 */
bool string_buffer_append_char(string_buffer_t *buffer, char ch);

/**
 * @brief Clear a string buffer
 *
 * @param[in] buffer String buffer
 */
void string_buffer_clear(string_buffer_t *buffer);

/**
 * @brief Get the C string of a string buffer
 *
 * @param[in] buffer String buffer
 * @return C string (read-only; lifetime tied to the buffer)
 */
const char *string_buffer_cstr(const string_buffer_t *buffer);

/**
 * @brief Get the length of a string buffer
 *
 * @param[in] buffer String buffer
 * @return String length
 */
size_t string_buffer_length(const string_buffer_t *buffer);

/**
 * @brief Create a string view
 *
 * @param[in] str C string
 * @param[in] encoding String encoding
 * @return String view
 */
string_view_t string_view_create(const char *str, string_encoding_t encoding);

/**
 * @brief Create a string view from a bounded length
 *
 * @param[in] str C string
 * @param[in] len String length
 * @param[in] encoding String encoding
 * @return String view
 */
string_view_t string_view_create_n(const char *str, size_t len, string_encoding_t encoding);

/**
 * @brief Compare two string views
 *
 * @param[in] view1 First string view
 * @param[in] view2 Second string view
 * @param[in] options Comparison options
 * @return 0 if equal, negative if view1 < view2, positive if view1 > view2
 */
int string_view_compare(const string_view_t *view1, const string_view_t *view2, int options);

/**
 * @brief Find a substring in a string view
 *
 * @param[in] haystack String view to search
 * @param[in] needle Substring view to find
 * @param[in] options Comparison options
 * @return Index of the substring start in haystack, or -1 if not found
 */
ssize_t string_view_find(const string_view_t *haystack, const string_view_t *needle, int options);

/**
 * @brief Convert a string view to a C string (allocates new memory)
 *
 * @param[in] view String view
 * @return C string (must be released with free)
 */
char *string_view_to_cstr(const string_view_t *view);

/**
 * @brief Create a string list
 *
 * @param[in] initial_capacity Initial capacity
 * @return String list
 */
string_list_t string_list_create(size_t initial_capacity);

/**
 * @brief Destroy a string list
 *
 * @param[in] list String list
 */
void string_list_destroy(string_list_t *list);

/**
 * @brief Add a string view to a string list
 *
 * @param[inout] list String list
 * @param[in] item String view to add
 * @return true on success, false on failure
 */
bool string_list_add(string_list_t *list, const string_view_t *item);

/**
 * @brief Add a C string to a string list
 *
 * @param[inout] list String list
 * @param[in] str C string to add
 * @return true on success, false on failure
 */
bool string_list_add_cstr(string_list_t *list, const char *str);

/**
 * @brief Clear a string list
 *
 * @param[inout] list String list
 */
void string_list_clear(string_list_t *list);

/**
 * @brief Get the size of a string list
 *
 * @param[in] list String list
 * @return List size
 */
size_t string_list_size(const string_list_t *list);

/**
 * @brief Get an item from a string list
 *
 * @param[in] list String list
 * @param[in] index Index
 * @return String view, or an empty view if the index is invalid
 */
string_view_t string_list_get(const string_list_t *list, size_t index);

/**
 * @brief Convert encoding
 *
 * @param[in] src Source string
 * @param[in] src_encoding Source encoding
 * @param[out] dest Destination buffer
 * @param[in] dest_size Destination buffer size
 * @param[in] dest_encoding Destination encoding
 * @return Number of converted bytes on success, -1 on failure
 */
int string_convert_encoding(const char *src, string_encoding_t src_encoding, char *dest,
                            size_t dest_size, string_encoding_t dest_encoding);

/**
 * @brief Count the characters (code points) of a UTF-8 string
 *
 * @param[in] str UTF-8 string
 * @param[in] max_len Maximum length to check
 * @return Character count (code points)
 */
size_t string_utf8_char_count(const char *str, size_t max_len);

/**
 * @brief Get the next character of a UTF-8 string
 *
 * @param[in] str UTF-8 string
 * @param[out] ch Output character (Unicode code point)
 * @return Number of bytes skipped on success, 0 on failure
 */
size_t string_utf8_next_char(const char *str, uint32_t *ch);

/**
 * @brief Check whether a string is valid UTF-8
 *
 * @param[in] str String
 * @param[in] len String length
 * @return true if valid UTF-8, false otherwise
 */
bool string_utf8_validate(const char *str, size_t len);

/** @} */ /* end of string_api */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_STRING_H */
