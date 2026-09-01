/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file input_validator.h
 * @brief Input validation utility library: security-by-design hardening.
 *
 * @details
 * This module provides unified input validation to prevent injection
 * attacks, path traversal, buffer overflows, and other security issues.
 * It follows the whitelist validation principle, only allowing known-safe
 * input patterns.
 *
 * Security principles:
 * 1. Never trust external input
 * 2. Whitelists over blacklists
 * 3. Boundary checks must be strict
 * 4. Fail safely on error
 *
 * @note Thread safety: all public interfaces are thread-safe
 * @see ARCHITECTURAL_PRINCIPLES.md E-1 security-by-design principle
 * @see C_Cpp_secure_coding_standard.md secure coding guide
 */

#ifndef AIRY_RT_INPUT_VALIDATOR_H
#define AIRY_RT_INPUT_VALIDATOR_H

#include "../error/error.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Validation result structure
 */
typedef struct {
    int is_valid;
    const char *error_message;
    int error_code;
    const char *error_field;
} airy_validation_result_t;


/**
 * @brief Validate a string's length
 * @param str [in] Input string
 * @param min_len Minimum length
 * @param max_len Maximum length
 * @param result [out] Validation result
 */
void airy_validate_string_length(const char *str, size_t min_len, size_t max_len,
                                 airy_validation_result_t *result);

/**
 * @brief Validate that a string contains only safe characters
 * @param str [in] Input string
 * @param allowed_chars [in] Allowed character set (whitelist)
 * @param result [out] Validation result
 */
void airy_validate_string_charset(const char *str, const char *allowed_chars,
                                  airy_validation_result_t *result);

/**
 * @brief Validate an identifier (letters, digits, underscores)
 * @param str [in] Input string
 * @param max_len Maximum length
 * @param result [out] Validation result
 */
void airy_validate_identifier(const char *str, size_t max_len, airy_validation_result_t *result);

/**
 * @brief Validate a JSON string
 * @param str [in] Input string
 * @param max_len Maximum length
 * @param result [out] Validation result
 */
void airy_validate_json_string(const char *str, size_t max_len, airy_validation_result_t *result);


/**
 * @brief Validate file path safety
 * @param path [in] Input path
 * @param allowed_root [in] Allowed root directory (may be NULL)
 * @param result [out] Validation result
 *
 * @details
 * Detects the following security issues:
 * - Path traversal attacks (../)
 * - NUL-byte injection
 * - Symlink attacks
 * - Absolute path restrictions
 */
void airy_validate_file_path(const char *path, const char *allowed_root,
                             airy_validation_result_t *result);

/**
 * @brief Normalize a path
 * @param path [in] Input path
 * @param out_normalized [out] Normalized path output (caller frees)
 * @param out_len Output length
 * @return airy_err_t Error code
 */
airy_err_t airy_normalize_path(const char *path, char **out_normalized, size_t *out_len);


/**
 * @brief Validate shell command safety
 * @param cmd [in] Input command
 * @param allowed_commands [in] Allowed command list (NULL-terminated)
 * @param result [out] Validation result
 *
 * @details
 * Detects the following security issues:
 * - Command injection (; | & $ ` etc.)
 * - Dangerous commands (rm -rf, dd, mkfs, etc.)
 * - Environment variable injection
 */
void airy_validate_shell_command(const char *cmd, const char **allowed_commands,
                                 airy_validation_result_t *result);

/**
 * @brief Sanitize a shell parameter
 * @param param [in] Input parameter
 * @param out_sanitized [out] Sanitized parameter output (caller frees)
 * @return airy_err_t Error code
 */
airy_err_t airy_sanitize_shell_param(const char *param, char **out_sanitized);


/**
 * @brief Validate SQL query safety
 * @param sql [in] Input SQL
 * @param result [out] Validation result
 *
 * @details
 * Detects the following security issues:
 * - SQL injection (UNION, OR 1=1, -- etc.)
 * - Dangerous operations (DROP, TRUNCATE, ALTER, etc.)
 * - Multi-statement execution
 */
void airy_validate_sql_query(const char *sql, airy_validation_result_t *result);

/**
 * @brief Sanitize an SQL identifier (table names, column names, etc.)
 * @param identifier [in] Input identifier
 * @param out_sanitized [out] Sanitized identifier output (caller frees)
 * @return airy_err_t Error code
 */
airy_err_t airy_sanitize_sql_identifier(const char *identifier, char **out_sanitized);


/**
 * @brief Validate URL safety
 * @param url [in] Input URL
 * @param allowed_schemes [in] Allowed scheme list (e.g. {"http",
 *                             "https", NULL})
 * @param result [out] Validation result
 *
 * @details
 * Detects the following security issues:
 * - Scheme injection (javascript:, data:, etc.)
 * - SSRF attacks (internal IPs, localhost, etc.)
 * - Credential leakage
 */
void airy_validate_url(const char *url, const char **allowed_schemes,
                       airy_validation_result_t *result);

/**
 * @brief Parse URL components
 * @param url [in] Input URL
 * @param out_scheme [out] Scheme output (caller frees)
 * @param out_host [out] Hostname output (caller frees)
 * @param out_port [out] Port output
 * @param out_path [out] Path output (caller frees)
 * @return airy_err_t Error code
 */
airy_err_t airy_parse_url(const char *url, char **out_scheme, char **out_host, uint16_t *out_port,
                          char **out_path);


/**
 * @brief Validate an integer range
 * @param value Input value
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @param result [out] Validation result
 */
void airy_validate_int_range(int64_t value, int64_t min_val, int64_t max_val,
                             airy_validation_result_t *result);

/**
 * @brief Validate a float range
 * @param value Input value
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @param result [out] Validation result
 */
void airy_validate_float_range(double value, double min_val, double max_val,
                               airy_validation_result_t *result);


/**
 * @brief Safe memory copy
 * @param dest [out] Destination buffer
 * @param dest_size Destination buffer size
 * @param src [in] Source data
 * @param src_size Source data size
 * @return airy_err_t Error code
 */
airy_err_t airy_safe_memcpy(void *dest, size_t dest_size, const void *src, size_t src_size);

/**
 * @brief Safe string copy
 * @param dest [out] Destination buffer
 * @param dest_size Destination buffer size
 * @param src [in] Source string
 * @return airy_err_t Error code
 */
airy_err_t airy_safe_strcpy(char *dest, size_t dest_size, const char *src);

/**
 * @brief Safe string concatenation
 * @param dest [in,out] Destination buffer
 * @param dest_size Destination buffer size
 * @param src [in] Source string
 * @return airy_err_t Error code
 */
airy_err_t airy_safe_strcat(char *dest, size_t dest_size, const char *src);


/**
 * @brief Validate and return on failure
 */
#define AIRY_VALIDATE_OR_RETURN(result, error_code) \
    do {                                            \
        if (!(result).is_valid) {                   \
            return error_code;                      \
        }                                           \
    } while (0)

/**
 * @brief Validate and jump to error handling
 */
#define AIRY_VALIDATE_OR_GOTO(result, label, error_code) \
    do {                                                 \
        if (!(result).is_valid) {                        \
            err = error_code;                            \
            goto label;                                  \
        }                                                \
    } while (0)

/**
 * @brief Safe string copy macro
 */
#define AIRY_SAFE_STRCPY(dest, src) airy_safe_strcpy(dest, sizeof(dest), src)

/**
 * @brief Safe string concatenation macro
 */
#define AIRY_SAFE_STRCAT(dest, src) airy_safe_strcat(dest, sizeof(dest), src)

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_INPUT_VALIDATOR_H */
