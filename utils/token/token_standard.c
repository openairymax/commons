// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file token_standard.c
 * @brief Standardized token counting - unified C/Python algorithm.
 *
 * Implements the standardized interfaces declared in token_standard.h,
 * ensuring cross-language consistency. Follows the AgentRT architecture
 * principle (E-3 resource determinism) for deterministic token counts.
 */

#include "token_standard.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "safe_utf8.h"

/**
 * @brief Algorithm info string.
 */
static const char *ALGORITHM_INFO =
    "AgentRT Token Standard v1.0 - Unified Token Counting Algorithm";

typedef struct {
    uint32_t lo;
    uint32_t hi;
} cjk_rng_t;

static const cjk_rng_t cjk_rngs[] = {
    {0x4E00, 0x9FFF},   {0x3400, 0x4DBF},   {0x20000, 0x2A6DF}, {0x2A700, 0x2B73F},
    {0x2B740, 0x2B81F}, {0x2B820, 0x2CEAF}, {0x2CEB0, 0x2EBEF}, {0x30000, 0x3134F},
    {0x31350, 0x323AF}, {0xF900, 0xFAFF},   {0x2F800, 0x2FA1F}, {0x3000, 0x303F},
    {0x3040, 0x309F},   {0x30A0, 0x30FF},   {0xAC00, 0xD7AF},   {0xFF00, 0xFFEF},
};

static int cp_is_cjk(uint32_t code_point)
{
    for (size_t i = 0; i < sizeof(cjk_rngs) / sizeof(cjk_rngs[0]); i++) {
        if (code_point >= cjk_rngs[i].lo && code_point <= cjk_rngs[i].hi)
            return 1;
    }
    return 0;
}

int airy_token_analyze_text(const char *text, size_t length, size_t *out_cjk_chars,
                            size_t *out_alpha_chars, size_t *out_total_chars)
{
    if (!text || !out_cjk_chars || !out_alpha_chars || !out_total_chars) {
        return AIRY_EINVAL;
    }

    size_t cjk_chars = 0;
    size_t alpha_chars = 0;
    size_t total_chars = 0;

    size_t i = 0;
    while (i < length) {
        uint32_t code_point = 0;
        size_t bytes_consumed = 0;

        if (utf8_decode((const unsigned char *)text + i, length - i, &bytes_consumed,
                        &code_point)) {
            total_chars++;
            if (code_point < 0x80 && isalpha((int)code_point)) {
                alpha_chars++;
            } else if (cp_is_cjk(code_point)) {
                cjk_chars++;
            }
            i += bytes_consumed;
        } else {
            i++;
        }
    }

    *out_cjk_chars = cjk_chars;
    *out_alpha_chars = alpha_chars;
    *out_total_chars = total_chars;

    return 0;
}

size_t airy_token_standard_count(const char *text, size_t length, const airy_token_config_t *config)
{
    if (!text) {
        return (size_t)-1;
    }

    airy_token_config_t default_config = AIRY_TOKEN_CONFIG_DEFAULT;
    const airy_token_config_t *cfg = config ? config : &default_config;

    if (airy_token_validate_config(cfg) != 0) {
        return (size_t)-1;
    }

    size_t text_len = length;
    if (text_len == 0) {
        text_len = strlen(text);
    }

    size_t cjk_chars = 0, alpha_chars = 0, total_chars = 0;
    if (airy_token_analyze_text(text, text_len, &cjk_chars, &alpha_chars, &total_chars) != 0) {
        return (size_t)-1;
    }

    size_t token_count = 0;

    if (cfg->flags & AIRY_TOKEN_FLAG_ACCURATE) {
        switch (cfg->model_type) {
        case AIRY_TOKEN_MODEL_BPE:
            token_count = (size_t)(cjk_chars / 1.5f + (total_chars - cjk_chars) / 4.0f);
            break;

        case AIRY_TOKEN_MODEL_UNIFORM:
            token_count = (size_t)(total_chars / 3.5f);
            break;

        case AIRY_TOKEN_MODEL_SPM:
            token_count = (size_t)(cjk_chars / 2.0f + (total_chars - cjk_chars) / 4.0f);
            break;

        default:
            if (cjk_chars > total_chars * cfg->cjk_ratio) {
                token_count = (size_t)(cjk_chars / 1.5f + (total_chars - cjk_chars) / 4.0f);
            } else if (alpha_chars > total_chars * cfg->alpha_ratio) {
                token_count = (size_t)(total_chars / 4.0f);
            } else {
                token_count = (size_t)(total_chars / 3.0f);
            }
            break;
        }
    } else {
        if (cjk_chars > total_chars * cfg->cjk_ratio) {
            token_count = (size_t)(total_chars / 1.5f);
        } else if (alpha_chars > total_chars * cfg->alpha_ratio) {
            token_count = (size_t)(total_chars / 4.0f);
        } else {
            token_count = (size_t)(total_chars / 3.0f);
        }
    }

    if (token_count == 0 && total_chars > 0) {
        token_count = 1;
    }

    return token_count;
}

const char *airy_token_get_algorithm_info(void)
{
    return ALGORITHM_INFO;
}

int airy_token_validate_config(const airy_token_config_t *config)
{
    if (!config) {
        return AIRY_EINVAL;
    }

    if (config->model_type < AIRY_TOKEN_MODEL_GENERIC ||
        config->model_type > AIRY_TOKEN_MODEL_CUSTOM) {
        return AIRY_EINVAL;
    }

    if (config->cjk_ratio <= 0.0f || config->cjk_ratio >= 1.0f) {
        return AIRY_EINVAL;
    }

    if (config->alpha_ratio <= 0.0f || config->alpha_ratio >= 1.0f) {
        return AIRY_EINVAL;
    }

    if ((config->flags & AIRY_TOKEN_FLAG_ACCURATE) && (config->flags & AIRY_TOKEN_FLAG_ESTIMATE)) {
        return AIRY_EINVAL;
    }

    return 0;
}

int airy_token_set_precision(airy_token_precision_t precision, airy_token_config_t *config)
{
    if (!config) {
        return AIRY_EINVAL;
    }

    switch (precision) {
    case AIRY_TOKEN_PRECISION_LOW:
        config->flags = AIRY_TOKEN_FLAG_ESTIMATE;
        config->cjk_ratio = 0.3f;
        config->alpha_ratio = 0.5f;
        break;

    case AIRY_TOKEN_PRECISION_MEDIUM:
        config->flags = AIRY_TOKEN_FLAG_ESTIMATE;
        config->cjk_ratio = 0.2f;
        config->alpha_ratio = 0.4f;
        break;

    case AIRY_TOKEN_PRECISION_HIGH:
        config->flags = AIRY_TOKEN_FLAG_ACCURATE;
        config->cjk_ratio = 0.1f;
        config->alpha_ratio = 0.3f;
        break;

    default:
        return AIRY_EINVAL;
    }

    return 0;
}

int airy_token_check_quota(const airy_token_quota_t *quota, size_t requested_tokens,
                           const airy_token_usage_t *current_usage)
{
    if (!quota) {
        return AIRY_EINVAL;
    }

    if (quota->max_tokens_per_request > 0 && requested_tokens > quota->max_tokens_per_request) {
        return 1;
    }

    if (current_usage) {
        if (quota->max_tokens_per_minute > 0 &&
            current_usage->tokens_used_per_minute + requested_tokens >
                quota->max_tokens_per_minute) {
            return 2;
        }

        if (quota->max_tokens_per_hour > 0 &&
            current_usage->tokens_used_per_hour + requested_tokens > quota->max_tokens_per_hour) {
            return 3;
        }

        if (quota->max_tokens_per_day > 0 &&
            current_usage->tokens_used_per_day + requested_tokens > quota->max_tokens_per_day) {
            return 4;
        }

        if (quota->max_requests_per_minute > 0 &&
            current_usage->requests_used_per_minute + 1 > quota->max_requests_per_minute) {
            return 5;
        }

        if (quota->max_requests_per_hour > 0 &&
            current_usage->requests_used_per_hour + 1 > quota->max_requests_per_hour) {
            return 6;
        }

        if (quota->max_requests_per_day > 0 &&
            current_usage->requests_used_per_day + 1 > quota->max_requests_per_day) {
            return 7;
        }
    }

    return 0;
}
