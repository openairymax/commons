/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file check.h
 * @brief Common check macros: reduce duplicated parameter validation and
 *        error-handling code.
 *
 * Provides a set of common check macros for parameter validation, error
 * handling, and resource cleanup. Aims to eliminate scattered check code
 * across the project and provide consistent validation patterns.
 *
 * @note Thread safety: all macros are thread-safe (no side effects)
 * @see ARCHITECTURAL_PRINCIPLES.md E-1 security-by-design principle
 */

#ifndef AIRY_RT_CHECK_H
#define AIRY_RT_CHECK_H

#include "../error/include/error.h"

#include <stdbool.h>

/**
 * @defgroup check_macros Check macros
 * @{
 */

/**
 * @brief Check that a pointer is not NULL; return the error code if it is
 * @param ptr Pointer to check
 * @param err_code Error code (e.g. AIRY_EINVAL)
 * @return err_code if ptr is NULL
 *
 * @code
 * CHECK_NULL_RET(input, AIRY_EINVAL);
 * @endcode
 */
#define CHECK_NULL_RET(ptr, err_code) \
    do {                              \
        if ((ptr) == NULL) {          \
            return (err_code);        \
        }                             \
    } while (0)

/**
 * @brief Check that a pointer is not NULL; return AIRY_EINVAL if it is
 * @param ptr Pointer to check
 * @return AIRY_EINVAL if ptr is NULL
 */
#define CHECK_NULL(ptr) CHECK_NULL_RET(ptr, AIRY_EINVAL)

/**
 * @brief Check that an expression is true; return the error code if false
 * @param expr Expression to check
 * @param err_code Error code
 * @return err_code if expr is false
 *
 * @code
 * CHECK_COND_RET(size > 0, AIRY_EINVAL);
 * @endcode
 */
#define CHECK_COND_RET(expr, err_code) \
    do {                               \
        if (!(expr)) {                 \
            return (err_code);         \
        }                              \
    } while (0)

/**
 * @brief Check that an expression is true; return AIRY_EINVAL if false
 * @param expr Expression to check
 * @return AIRY_EINVAL if expr is false
 */
#define CHECK_COND(expr) CHECK_COND_RET(expr, AIRY_EINVAL)

/**
 * @brief Check a function-call result; return the error code on failure
 * @param func_call Function-call expression (returns airy_err_t)
 * @param err_var Variable name storing the error result
 * @return The error code if func_call failed
 *
 * @code
 * CHECK_ERR_RET(airy_init(), err);
 * @endcode
 */
#define CHECK_ERR_RET(func_call, err_var) \
    do {                                  \
        airy_err_t err_var = (func_call); \
        if (err_var != AIRY_SUCCESS) {    \
            return err_var;               \
        }                                 \
    } while (0)

/**
 * @brief Check a function-call result; jump to the cleanup label on failure
 * @param func_call Function-call expression (returns airy_err_t)
 * @param err_var Variable name storing the error result
 *
 * @code
 * CHECK_ERR_GOTO(airy_alloc(&ptr), err, cleanup);
 * @endcode
 */
#define CHECK_ERR_GOTO(func_call, err_var, label) \
    do {                                          \
        airy_err_t err_var = (func_call);         \
        if (err_var != AIRY_SUCCESS) {            \
            goto label;                           \
        }                                         \
    } while (0)

/**
 * @brief Check that a pointer is not NULL; jump to the cleanup label if it is
 * @param ptr Pointer to check
 * @param label Jump label
 *
 * @code
 * CHECK_NULL_GOTO(buffer, cleanup);
 * @endcode
 */
#define CHECK_NULL_GOTO(ptr, label) \
    do {                            \
        if ((ptr) == NULL) {        \
            goto label;             \
        }                           \
    } while (0)

/**
 * @brief Safely free a pointer and set it to NULL
 * @param ptr Pointer to free
 *
 * @note Releases with AIRY_FREE
 * @code
 * SAFE_FREE(buffer);
 * @endcode
 */
#define SAFE_FREE(ptr)       \
    do {                     \
        if ((ptr) != NULL) { \
            AIRY_FREE(ptr);  \
            (ptr) = NULL;    \
        }                    \
    } while (0)

/**
 * @brief Allocate memory and check the result; jump to the cleanup label on
 *        failure
 * @param ptr_var Pointer variable name
 * @param size Allocation size
 * @param label Jump label
 *
 * @code
 * ALLOC_CHECK(buffer, sizeof(buffer_t), cleanup);
 * @endcode
 */
#define ALLOC_CHECK(ptr_var, size, label) \
    do {                                  \
        (ptr_var) = AIRY_MALLOC(size);    \
        CHECK_NULL_GOTO(ptr_var, label);  \
    } while (0)

/**
 * @brief Allocate and zero memory; jump to the cleanup label on failure
 * @param ptr_var Pointer variable name
 * @param count Element count
 * @param size Element size
 * @param label Jump label
 *
 * @code
 * CALLOC_CHECK(array, 10, sizeof(int), cleanup);
 * @endcode
 */
#define CALLOC_CHECK(ptr_var, count, size, label) \
    do {                                          \
        (ptr_var) = AIRY_CALLOC(count, size);     \
        CHECK_NULL_GOTO(ptr_var, label);          \
    } while (0)

/**
 * @brief String duplication check; jump to the cleanup label on failure
 * @param dest Destination pointer variable
 * @param src Source string
 * @param label Jump label
 *
 * @code
 * STRDUP_CHECK(copy, original, cleanup);
 * @endcode
 */
#define STRDUP_CHECK(dest, src, label) \
    do {                               \
        (dest) = AIRY_STRDUP(src);     \
        CHECK_NULL_GOTO(dest, label);  \
    } while (0)

/**
 * @brief Range check, ensuring the value is within [min, max]
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param err_code Error code
 * @return err_code if the value is out of range
 */
#define CHECK_RANGE_RET(value, min, max, err_code) \
    do {                                           \
        if ((value) < (min) || (value) > (max)) {  \
            return (err_code);                     \
        }                                          \
    } while (0)

/**
 * @brief Non-zero check, ensuring the value is not zero
 * @param value Value to check
 * @param err_code Error code
 * @return err_code if the value is zero
 */
#define CHECK_NONZERO_RET(value, err_code) \
    do {                                   \
        if ((value) == 0) {                \
            return (err_code);             \
        }                                  \
    } while (0)

/**
 * @brief Check that a string is neither NULL nor empty
 * @param str String to check
 * @param err_code Error code
 * @return err_code if the string is NULL or empty
 */
#define CHECK_STRING_RET(str, err_code)          \
    do {                                         \
        if ((str) == NULL || (str)[0] == '\0') { \
            return (err_code);                   \
        }                                        \
    } while (0)

/**
 * @brief Check that a pointer is not NULL; set the error code and jump if it
 *        is
 * @param ptr Pointer to check
 * @param label Jump label
 * @param err_var Error variable name (e.g. ret)
 * @param err_code Error code
 *
 * @code
 * CHECK_NULL_GOTO_ERR(buffer, cleanup, ret, AIRY_ENOMEM);
 * @endcode
 */
#define CHECK_NULL_GOTO_ERR(ptr, label, err_var, err_code) \
    do {                                                   \
        if ((ptr) == NULL) {                               \
            (err_var) = (err_code);                        \
            goto label;                                    \
        }                                                  \
    } while (0)

/**
 * @brief String duplication check; set the error code and jump on failure
 * @param dest Destination pointer variable
 * @param src Source string
 * @param label Jump label
 * @param err_var Error variable name
 * @param err_code Error code
 *
 * @code
 * STRDUP_CHECK_ERR(copy, original, cleanup, ret, AIRY_ENOMEM);
 * @endcode
 */
#define STRDUP_CHECK_ERR(dest, src, label, err_var, err_code) \
    do {                                                      \
        (dest) = AIRY_STRDUP(src);                            \
        CHECK_NULL_GOTO_ERR(dest, label, err_var, err_code);  \
    } while (0)

/**
 * @brief Allocation check; set the error code and jump on failure
 * @param ptr_var Pointer variable name
 * @param size Allocation size
 * @param label Jump label
 * @param err_var Error variable name
 * @param err_code Error code
 *
 * @code
 * MALLOC_CHECK_ERR(buffer, sizeof(buffer_t), cleanup, ret, AIRY_ENOMEM);
 * @endcode
 */
#define MALLOC_CHECK_ERR(ptr_var, size, label, err_var, err_code) \
    do {                                                          \
        (ptr_var) = AIRY_MALLOC(size);                            \
        CHECK_NULL_GOTO_ERR(ptr_var, label, err_var, err_code);   \
    } while (0)

/**
 * @brief Allocate-and-zero check; set the error code and jump on failure
 * @param ptr_var Pointer variable name
 * @param count Element count
 * @param size Element size
 * @param label Jump label
 * @param err_var Error variable name
 * @param err_code Error code
 *
 * @code
 * CALLOC_CHECK_ERR(array, 10, sizeof(int), cleanup, ret, AIRY_ENOMEM);
 * @endcode
 */
#define CALLOC_CHECK_ERR(ptr_var, count, size, label, err_var, err_code) \
    do {                                                                 \
        (ptr_var) = AIRY_CALLOC(count, size);                            \
        CHECK_NULL_GOTO_ERR(ptr_var, label, err_var, err_code);          \
    } while (0)

/** @} */ /* end of check_macros */
#endif /* AIRY_RT_CHECK_H */
