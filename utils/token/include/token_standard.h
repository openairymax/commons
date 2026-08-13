/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file token_standard.h
 * @brief Standardized token counting interface - unified C/Python algorithm.
 *
 * Designed per the AgentRT architecture principle (E-3 resource
 * determinism) to provide a deterministic token counting standard,
 * ensuring cross-language consistency with resource quota management and
 * monitoring integration.
 */

#ifndef AIRY_RT_TOKEN_STANDARD_H
#define AIRY_RT_TOKEN_STANDARD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Token counting algorithm version.
 */
#define AIRY_TOKEN_ALGORITHM_VERSION "1.0"

/**
 * @brief Token counting model types.
 */
typedef enum {
    AIRY_TOKEN_MODEL_GENERIC = 0,
    AIRY_TOKEN_MODEL_GPT4,
    AIRY_TOKEN_MODEL_GPT35,
    AIRY_TOKEN_MODEL_CLAUDE,
    AIRY_TOKEN_MODEL_LLAMA,
    AIRY_TOKEN_MODEL_CUSTOM
} airy_token_model_t;

/**
 * @brief Token counting config.
 */
typedef struct {
    airy_token_model_t model_type;
    const char *model_name;
    float cjk_ratio;
    float alpha_ratio;
    uint32_t flags;
} airy_token_config_t;

/**
 * @brief Token counting flags.
 */
#define AIRY_TOKEN_FLAG_ACCURATE 0x01
#define AIRY_TOKEN_FLAG_ESTIMATE 0x02
#define AIRY_TOKEN_FLAG_INCLUDE_BOM 0x04

/**
 * @brief Default token counting config.
 */
#define AIRY_TOKEN_CONFIG_DEFAULT            \
    {.model_type = AIRY_TOKEN_MODEL_GENERIC, \
     .model_name = "generic",                \
     .cjk_ratio = 0.3f,                      \
     .alpha_ratio = 0.5f,                    \
     .flags = AIRY_TOKEN_FLAG_ESTIMATE}

/**
 * @brief Standardized token counting function.
 *
 * Counts the tokens of a text according to the config, ensuring
 * consistency across language implementations.
 *
 * @param text Input text (UTF-8 encoded)
 * @param length Text length (bytes); 0 auto-computes
 * @param config Token counting config; NULL uses the default
 * @return Token count, (size_t)-1 on error
 */
size_t airy_token_standard_count(const char *text, size_t length,
                                 const airy_token_config_t *config);

/**
 * @brief Batch token counting.
 *
 * Counts tokens for multiple texts in one call for efficiency.
 *
 * @param texts Text array
 * @param lengths Length array (bytes); NULL auto-computes each length
 * @param count Text count
 * @param out_counts Output token count array
 * @param config Token counting config; NULL uses the default
 * @return 0 on success, error code on failure
 */
int airy_token_standard_count_batch(const char **texts, const size_t *lengths, size_t count,
                                    size_t *out_counts, const airy_token_config_t *config);

/**
 * @brief Detect text language features.
 *
 * Analyzes the language features of a text to optimize token counting.
 *
 * @param text Input text
 * @param length Text length
 * @param out_cjk_chars Output CJK character count
 * @param out_alpha_chars Output alphabetic character count
 * @param out_total_chars Output total character count
 * @return 0 on success, error code on failure
 */
int airy_token_analyze_text(const char *text, size_t length, size_t *out_cjk_chars,
                            size_t *out_alpha_chars, size_t *out_total_chars);

/**
 * @brief Get token counting algorithm info.
 *
 * @return Algorithm description string
 */
const char *airy_token_get_algorithm_info(void);

/**
 * @brief Validate a token counting config.
 *
 * @param config Config params
 * @return 0 if valid, error code otherwise
 */
int airy_token_validate_config(const airy_token_config_t *config);

/**
 * @brief Token counting precision levels.
 */
typedef enum {
    AIRY_TOKEN_PRECISION_LOW = 0,
    AIRY_TOKEN_PRECISION_MEDIUM,
    AIRY_TOKEN_PRECISION_HIGH
} airy_token_precision_t;

/**
 * @brief Set the token counting precision.
 *
 * @param precision Precision level
 * @param config Output config (optional)
 * @return 0 on success, error code on failure
 */
int airy_token_set_precision(airy_token_precision_t precision, airy_token_config_t *config);

/**
 * @brief Resource quota limits.
 */
typedef struct {
    size_t max_tokens_per_request;
    size_t max_tokens_per_minute;
    size_t max_tokens_per_hour;
    size_t max_tokens_per_day;
    size_t max_requests_per_minute;
    size_t max_requests_per_hour;
    size_t max_requests_per_day;
} airy_token_quota_t;

/**
 * @brief Default resource quota.
 */
#define AIRY_TOKEN_QUOTA_DEFAULT     \
    {.max_tokens_per_request = 8000, \
     .max_tokens_per_minute = 60000, \
     .max_tokens_per_hour = 360000,  \
     .max_tokens_per_day = 2000000,  \
     .max_requests_per_minute = 60,  \
     .max_requests_per_hour = 3600,  \
     .max_requests_per_day = 10000}

/**
 * @brief Resource usage.
 */
typedef struct {
    size_t tokens_used_per_minute;
    size_t tokens_used_per_hour;
    size_t tokens_used_per_day;
    size_t requests_used_per_minute;
    size_t requests_used_per_hour;
    size_t requests_used_per_day;
} airy_token_usage_t;

/**
 * @brief Check whether the resource quota is sufficient.
 *
 * Checks all quota limits level by level: per-request, per-minute,
 * per-hour, per-day. When current_usage is NULL, only the per-request
 * limit is checked.
 *
 * @param quota Quota limits
 * @param requested_tokens Requested token count
 * @param current_usage Current usage (may be NULL)
 * @return 0 quota OK, 1 over per-request limit, 2 over per-minute token
 *         limit, 3 over per-hour token limit, 4 over per-day token
 *         limit, 5 over per-minute request limit, 6 over per-hour
 *         request limit, 7 over per-day request limit
 */
int airy_token_check_quota(const airy_token_quota_t *quota, size_t requested_tokens,
                           const airy_token_usage_t *current_usage);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TOKEN_STANDARD_H */