/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_quality.h
 * @brief AgentRT code quality assurance framework.
 *
 * Provides standardized code quality assurance utilities:
 * - Input validation macros (NULL check, range check, type check)
 * - Error handling macros (safe return, error propagation)
 * - Resource management macros (RAII pattern, automatic cleanup)
 * - Boundary check macros (array bounds, integer overflow)
 *
 * Follows the E-1 security-by-design, E-3 resource determinism, and E-6
 * error traceability principles.
 */

#ifndef AIRY_RT_QUALITY_H
#define AIRY_RT_QUALITY_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "error.h"


void *airy_malloc(size_t size);
void *airy_calloc(size_t num, size_t size);
void airy_free(const void *ptr);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup quality_assurance
 * @{
 */


/**
 * @brief Check that a pointer is not NULL; return the error code if it is
 */
#ifndef AIRY_CHECK_NULL
#define AIRY_CHECK_NULL(ptr, error_code) \
    do {                                 \
        if ((ptr) == NULL) {             \
            return (error_code);         \
        }                                \
    } while (0)
#endif

/**
 * @brief Check that a pointer is not NULL; jump to the error label if it is
 */
#define AIRY_CHECK_NULL_GOTO(ptr, label, error_code) \
    do {                                             \
        if ((ptr) == NULL) {                         \
            err = (error_code);                      \
            goto label;                              \
        }                                            \
    } while (0)

/**
 * @brief Check a condition; return the error code if it does not hold
 */
#define AIRY_CHECK_CONDITION(cond, error_code) \
    do {                                       \
        if (!(cond)) {                         \
            return (error_code);               \
        }                                      \
    } while (0)

/**
 * @brief Check a condition; jump to the error label if it does not hold
 */
#define AIRY_CHECK_CONDITION_GOTO(cond, label, error_code) \
    do {                                                   \
        if (!(cond)) {                                     \
            err = (error_code);                            \
            goto label;                                    \
        }                                                  \
    } while (0)

/**
 * @brief Check that a value is within [min, max]
 */
#define AIRY_CHECK_RANGE(value, min_val, max_val, error_code) \
    do {                                                      \
        if ((value) < (min_val) || (value) > (max_val)) {     \
            return (error_code);                              \
        }                                                     \
    } while (0)

/**
 * @brief Check that a value is at least the minimum
 */
#define AIRY_CHECK_MIN(value, min_val, error_code) \
    do {                                           \
        if ((value) < (min_val)) {                 \
            return (error_code);                   \
        }                                          \
    } while (0)

/**
 * @brief Check that a value is at most the maximum
 */
#define AIRY_CHECK_MAX(value, max_val, error_code) \
    do {                                           \
        if ((value) > (max_val)) {                 \
            return (error_code);                   \
        }                                          \
    } while (0)

/**
 * @brief Check that a string's length is within the allowed range
 */
#define AIRY_CHECK_STR_LEN(str, max_len, error_code) \
    do {                                             \
        if (!(str) || strlen((str)) > (max_len)) {   \
            return (error_code);                     \
        }                                            \
    } while (0)

/**
 * @brief Check that an array index is valid
 */
#define AIRY_CHECK_ARRAY_INDEX(index, array_size, error_code) \
    do {                                                      \
        if ((index) >= (array_size)) {                        \
            return (error_code);                              \
        }                                                     \
    } while (0)

/**
 * @brief Check that a string is neither NULL nor empty
 */
#define AIRY_CHECK_EMPTY(str, error_code)        \
    do {                                         \
        if ((str) == NULL || (str)[0] == '\0') { \
            return (error_code);                 \
        }                                        \
    } while (0)

/**
 * @brief Check that an array index is in bounds (compatibility macro)
 */
#define AIRY_CHECK_BOUNDS(idx, size, error_code) AIRY_CHECK_ARRAY_INDEX((idx), (size), (error_code))


/**
 * @brief Execute an operation safely; jump to the cleanup label on failure
 */
#define AIRY_SAFE_EXEC(expr, cleanup_label, error_var) \
    do {                                               \
        int _ret = (expr);                             \
        if (_ret != 0) {                               \
            (error_var) = _ret;                        \
            goto cleanup_label;                        \
        }                                              \
    } while (0)

/**
 * @brief Allocate memory safely; jump to the cleanup label on failure
 */
#define AIRY_SAFE_ALLOC(var, size, cleanup_label, error_var) \
    do {                                                     \
        (var) = airy_malloc((size));                         \
        if (!(var)) {                                        \
            (error_var) = -1;                                \
            goto cleanup_label;                              \
        }                                                    \
    } while (0)

/**
 * @brief Allocate and zero memory safely; jump to the cleanup label on failure
 */
#define AIRY_SAFE_CALLOC(var, size, cleanup_label, error_var) \
    do {                                                      \
        (var) = airy_calloc(1, (size));                       \
        if (!(var)) {                                         \
            (error_var) = -1;                                 \
            goto cleanup_label;                               \
        }                                                     \
    } while (0)

/**
 * @brief Log an error and return
 */
#define AIRY_LOG_ERROR_AND_RETURN(error_code, fmt, ...) \
    do {                                                \
        /* log the error */                             \
        return (error_code);                            \
    } while (0)


/**
 * @brief Begin a RAII-style resource guard scope
 */
#define AIRY_RESOURCE_GUARD_SCOPE_BEGIN() {

/**
 * @brief End a RAII-style resource guard scope
 */
#define AIRY_RESOURCE_GUARD_SCOPE_END() }

/**
 * @brief Macro for automatic resource release (for local variables)
 */
#ifndef AIRY_AUTO_FREE
#define AIRY_AUTO_FREE(ptr) \
    __attribute__((cleanup(airy_auto_free))) char **_auto_##ptr = &(char *)(ptr)
#endif

/**
 * @brief Macro for automatic file-descriptor close
 */
#define AIRY_AUTO_CLOSE(fd) __attribute__((cleanup(airy_auto_close))) int *_auto_##fd = &(fd)

/**
 * @brief Free memory safely and set the pointer to NULL
 */
#define AIRY_SAFE_FREE(ptr)   \
    do {                      \
        if ((ptr) != NULL) {  \
            airy_free((ptr)); \
            (ptr) = NULL;     \
        }                     \
    } while (0)

/**
 * @brief Free memory securely: zero it before releasing (SEC-15 compliance)
 *
 * Used for memory holding sensitive data (API keys, tokens, passwords,
 * etc.), preventing data remnants on the heap from leaking.
 *
 * @note Only valid for memory allocated via airy_mem_alloc/malloc
 * @note Sets the pointer to NULL after freeing, preventing use-after-free
 *
 * BAN-247: sensitive data must be zeroed before release
 */
#ifndef AIRY_SECURE_FREE
#define AIRY_SECURE_FREE(ptr, size)                 \
    do {                                            \
        if ((ptr) != NULL) {                        \
            if ((size) > 0) {                       \
                airy_explicit_bzero((ptr), (size)); \
            }                                       \
            free((ptr));                            \
            (ptr) = NULL;                           \
        }                                           \
    } while (0)
#endif

/**
 * @brief Securely free memory (auto-size version for known types)
 *
 * Usage: AIRY_SECURE_FREE_T(my_struct_ptr, my_struct_t)
 */
#define AIRY_SECURE_FREE_T(ptr, type) AIRY_SECURE_FREE((ptr), sizeof(type))

/**
 * @brief Explicit memory zeroing (prevents the compiler from optimizing
 *        away the memset)
 *
 * Uses a volatile function pointer so the compiler cannot optimize away
 * the zeroing. This is critical for erasing security-sensitive data.
 */
static inline void airy_explicit_bzero(void *s, size_t n)
{
    if (s == NULL || n == 0)
        return;
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n--) {
        *p++ = 0;
    }
}


/**
 * @brief Validate that a value is non-negative
 */
static inline bool airy_validate_non_negative(int value)
{
    return value >= 0;
}

/**
 * @brief Validate that a value is positive
 */
static inline bool airy_validate_positive(int value)
{
    return value > 0;
}

/**
 * @brief Validate that a value is a valid percentage [0, 100]
 */
static inline bool airy_validate_percentage(float value)
{
    return value >= 0.0f && value <= 100.0f;
}

/**
 * @brief Validate that a value is a valid probability [0, 1]
 */
static inline bool airy_validate_probability(float value)
{
    return value >= 0.0f && value <= 1.0f;
}

/**
 * @brief Validate that a priority is within the valid range
 */
static inline bool airy_validate_priority(int priority, int min_val, int max_val)
{
    return priority >= min_val && priority <= max_val;
}


/**
 * @brief Safe integer addition (detects overflow)
 * @param[in] a Operand a
 * @param[in] b Operand b
 * @param[out] result Result
 * @return 0 on success, -1 on overflow
 */
static inline int safe_add_int(int a, int b, int *result)
{
    if (!result)
        return AIRY_EINVAL;

    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        return AIRY_EINVAL;
    }

    *result = a + b;
    return AIRY_SUCCESS;
}

/**
 * @brief Safe integer multiplication (detects overflow)
 * @param[in] a Operand a
 * @param[in] b Operand b
 * @param[out] result Result
 * @return 0 on success, -1 on overflow
 */
static inline int safe_mul_int(int a, int b, int *result)
{
    if (!result)
        return AIRY_EINVAL;

    if (a > 0) {
        if (b > 0 && a > INT_MAX / b)
            return AIRY_EINVAL;
        if (b < 0 && b < INT_MIN / a)
            return AIRY_EINVAL;
    } else if (a < 0) {
        if (b > 0 && a < INT_MIN / b)
            return AIRY_EINVAL;
        if (b < 0 && a > INT_MAX / b)
            return AIRY_EINVAL;
    }

    *result = a * b;
    return AIRY_SUCCESS;
}

/**
 * @brief Safe size_t addition (detects overflow)
 * @param[in] a Operand a
 * @param[in] b Operand b
 * @param[out] result Result
 * @return 0 on success, -1 on overflow
 */
static inline int safe_add_size(size_t a, size_t b, size_t *result)
{
    if (!result)
        return AIRY_EINVAL;

    if (a > SIZE_MAX - b) {
        return AIRY_EINVAL;
    }

    *result = a + b;
    return AIRY_SUCCESS;
}

/**
 * @brief Safe size_t multiplication (detects overflow)
 * @param[in] a Operand a
 * @param[in] b Operand b
 * @param[out] result Result
 * @return 0 on success, -1 on overflow
 */
static inline int safe_mul_size(size_t a, size_t b, size_t *result)
{
    if (!result)
        return AIRY_EINVAL;

    if (b != 0 && a > SIZE_MAX / b) {
        return AIRY_EINVAL;
    }

    *result = a * b;
    return AIRY_SUCCESS;
}

/**
 * @brief Check whether an array access is safe
 * @param[in] index Index
 * @param[in] size Array size
 * @return true if safe, false otherwise
 */
static inline bool is_safe_array_access(size_t index, size_t size)
{
    return index < size;
}

/**
 * @brief Check whether a pointer offset is safe
 * @param[in] ptr Base address pointer
 * @param[in] offset Offset
 * @param[in] size Buffer size
 * @return true if safe, false otherwise
 */
static inline bool is_safe_ptr_offset(const void *ptr, size_t offset, size_t size)
{
    if (!ptr || offset >= size) {
        return false;
    }
    return true;
}

/**
 * @brief Check whether a string copy is safe
 * @param[in] src Source string
 * @param[in] dest Destination buffer
 * @param[in] dest_size Destination buffer size
 * @return true if safe, false otherwise
 */
static inline bool is_safe_str_copy(const char *src, char *dest, size_t dest_size)
{
    if (!src || !dest || dest_size == 0) {
        return false;
    }

    if (strlen(src) >= dest_size) {
        return false;
    }

    return true;
}


/**
 * @brief Safe memory copy (with boundary checks)
 * @param[out] dest Destination buffer
 * @param[in] dest_size Destination buffer size
 * @param[in] src Source data
 * @param[in] src_size Source data size
 * @return 0 on success, -1 on invalid parameters or insufficient buffer
 */
static inline int safe_memcpy(void *dest, size_t dest_size, const void *src, size_t src_size)
{
    if (!dest || !src)
        return AIRY_EINVAL;
    if (src_size > dest_size)
        return AIRY_EINVAL;

    AIRY_MEMCPY(dest, src, src_size);
    return AIRY_SUCCESS;
}

/**
 * @brief Safe memory set (with boundary checks)
 * @param[out] dest Destination buffer
 * @param[in] dest_size Destination buffer size
 * @param[in] value Set value
 * @param[in] count Byte count
 * @return 0 on success, -1 on invalid parameters or out of range
 */
static inline int safe_memset(void *dest, size_t dest_size, int value, size_t count)
{
    if (!dest)
        return AIRY_EINVAL;
    if (count > dest_size)
        return AIRY_EINVAL;

    AIRY_MEMSET(dest, value, count);
    return AIRY_SUCCESS;
}

/**
 * @brief Safe string copy (with length limit)
 * @param[out] dest Destination buffer
 * @param[in] dest_size Destination buffer size (including terminator space)
 * @param[in] src Source string
 * @return 0 on success, -1 on invalid parameters or source too long
 */
static inline int safe_strcpy(char *dest, size_t dest_size, const char *src)
{
    if (!dest || !src || dest_size == 0)
        return AIRY_EINVAL;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overread"
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    size_t src_len = strlen(src);
    if (src_len >= dest_size)
        return AIRY_EINVAL;

    AIRY_MEMCPY(dest, src, src_len + 1);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    return AIRY_SUCCESS;
}

/**
 * @brief Safe string concatenation (with length limit)
 * @param[in,out] dest Destination buffer
 * @param[in] dest_size Total destination buffer size
 * @param[in] src Source string
 * @return 0 on success, -1 on invalid parameters or out of range
 */
static inline int safe_strcat(char *dest, size_t dest_size, const char *src)
{
    if (!dest || !src || dest_size == 0)
        return AIRY_EINVAL;

    size_t current_len = strlen(dest);
    size_t src_len = strlen(src);

    if (current_len + src_len >= dest_size)
        return AIRY_EINVAL;

    AIRY_MEMCPY(dest + current_len, src, src_len + 1);
    return AIRY_SUCCESS;
}

/**
 * @brief Safe string length (with NULL protection)
 * @param[in] str String
 * @return String length, 0 for NULL
 */
static inline size_t safe_strlen(const char *str)
{
    if (!str)
        return AIRY_SUCCESS;
    return strlen(str);
}

/**
 * @brief Safe string comparison (with NULL protection)
 * @param[in] str1 String 1
 * @param[in] str2 String 2
 * @return Comparison result; NULL is treated as an empty string
 */
/* BAN-073 exempt: this function returns strcmp's three-state semantics
 * (negative/0/positive), not an error code. NULL participates as an empty
 * string; -1 means str1 < str2, not an AIRY_ERR_* error code. */
static inline int safe_strcmp(const char *str1, const char *str2)
{
    if (!str1 && !str2)
        return 0;
    if (!str1)
        return -1;
    if (!str2)
        return 1;
    return strcmp(str1, str2);
}


/**
 * @brief Safe int-to-size_t conversion (checks for negatives)
 * @param[in] value Integer value
 * @param[out] result Conversion result
 * @return 0 on success, -1 on negative overflow
 */
static inline int safe_int_to_size(int value, size_t *result)
{
    if (!result)
        return AIRY_EINVAL;
    if (value < 0)
        return AIRY_EINVAL;
    *result = (size_t)value;
    return AIRY_SUCCESS;
}

/**
 * @brief Safe size_t-to-int conversion (checks the range)
 * @param[in] value size_t value
 * @param[out] result Conversion result
 * @return 0 on success, -1 if out of int range
 */
static inline int safe_size_to_int(size_t value, int *result)
{
    if (!result)
        return AIRY_EINVAL;
    if (value > (size_t)INT_MAX)
        return AIRY_EINVAL;
    *result = (int)value;
    return AIRY_SUCCESS;
}

/**
 * @brief Safe double-to-int conversion (checks the range)
 * @param[in] value double value
 * @param[out] result Conversion result
 * @return 0 on success, -1 if out of range
 */
static inline int safe_double_to_int(double value, int *result)
{
    if (!result)
        return AIRY_EINVAL;
    if (value > (double)INT_MAX || value < (double)INT_MIN)
        return AIRY_EINVAL;
    *result = (int)value;
    return AIRY_SUCCESS;
}


/**
 * @brief airy_safe_strcpy compatibility alias
 * @note Kept for compatibility with atoms/tests/test_common_utils.c
 */
#define airy_safe_strcpy(dest, dest_size, src) safe_strcpy((dest), (dest_size), (src))

/**
 * @brief airy_safe_strcat compatibility alias
 * @note Kept for compatibility with atoms/tests/test_common_utils.c
 */
#define airy_safe_strcat(dest, dest_size, src) safe_strcat((dest), (dest_size), (src))

/** @} */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_QUALITY_H */
