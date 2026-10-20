// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file counter.c
 * @brief Token counter implementation.
 *
 * Implements token counting and budget management:
 * - Character-level heuristic approximation (word/CJK/punctuation tokenization)
 * - Tokenizer family coefficient adjustment (generic/bpe/spm/uniform)
 * - Batch counting and truncation
 * - Thread-safe counter operations
 *
 * @note This implementation uses a lightweight character heuristic, not a
 * full BPE encoder. For high-precision production needs, consider
 * integrating TikToken or an equivalent library.
 */

#include "../../platform/include/platform.h"
#include "token.h"
#include "token_standard.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../memory/airy_memory.h"
#include "../string/string_compat.h"
#include "error.h"

#define MAX_FAMILY_NAME 16

/**
 * @brief Token counter internal structure.
 */
struct airy_token_counter {
    char family[MAX_FAMILY_NAME];
    airy_token_model_t type;
    airy_mtx_t mutex;
    size_t total_count;
    uint64_t request_count;
    size_t max_token_length;
};

static int family_lookup(const char *name, airy_token_model_t *out)
{
    static const struct {
        const char *name;
        airy_token_model_t type;
    } table[] = {
        {"generic", AIRY_TOKEN_MODEL_GENERIC},
        {"bpe", AIRY_TOKEN_MODEL_BPE},
        {"spm", AIRY_TOKEN_MODEL_SPM},
        {"uniform", AIRY_TOKEN_MODEL_UNIFORM},
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(name, table[i].name) == 0) {
            *out = table[i].type;
            return 0;
        }
    }
    return -1;
}

static size_t count_tok_family(airy_token_model_t type, const char *text,
                                     size_t length)
{
    airy_token_config_t config = AIRY_TOKEN_CONFIG_DEFAULT;

    config.model_type = type;
    /* 族系数只在 ACCURATE 路径生效（ACCURATE/ESTIMATE 互斥）；声明了具体族
     * 就切到 ACCURATE，未声明族保持启发式估计。 */
    if (type != AIRY_TOKEN_MODEL_GENERIC) {
        config.flags = AIRY_TOKEN_FLAG_ACCURATE;
    }

    return airy_token_standard_count(text, length, &config);
}

airy_token_counter_t *airy_token_counter_create(const char *family_name)
{
    if (!family_name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    airy_token_model_t type;
    if (family_lookup(family_name, &type) != 0) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "unknown token family");
    }

    airy_token_counter_t *counter =
        (airy_token_counter_t *)AIRY_MALLOC(sizeof(airy_token_counter_t));
    if (!counter) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    AIRY_MEMSET(counter, 0, sizeof(airy_token_counter_t));

    AIRY_STRNCPY_TERM(counter->family, family_name, MAX_FAMILY_NAME);
    counter->family[MAX_FAMILY_NAME - 1] = '\0';
    counter->type = type;

    if (airy_mtx_init(&counter->mutex) != 0) {
        AIRY_FREE(counter);
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    counter->total_count = 0;
    counter->request_count = 0;
    counter->max_token_length = 128 * 1024;

    return counter;
}

void airy_token_counter_destroy(airy_token_counter_t *counter)
{
    if (!counter) {
        return;
    }

    airy_mtx_destroy(&counter->mutex);
    AIRY_FREE(counter);
}

size_t airy_token_counter_count(airy_token_counter_t *counter, const char *text)
{
    if (!counter || !text) {
        return (size_t)-1;
    }

    size_t length = strlen(text);
    if (length == 0) {
        return 0;
    }

    airy_mtx_lock(&counter->mutex);

    size_t token_count = count_tok_family(counter->type, text, length);
    counter->total_count += token_count;
    counter->request_count++;

    airy_mtx_unlock(&counter->mutex);

    return token_count;
}

size_t airy_token_counter_count_batch(airy_token_counter_t *counter, const char **texts,
                                      size_t count, size_t *out_counts)
{
    if (!counter || !texts || !out_counts) {
        return (size_t)-1;
    }

    airy_mtx_lock(&counter->mutex);

    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (texts[i]) {
            size_t len = strlen(texts[i]);
            out_counts[i] = count_tok_family(counter->type, texts[i], len);
            total += out_counts[i];
        } else {
            out_counts[i] = 0;
        }
    }

    counter->total_count += total;
    counter->request_count += count;

    airy_mtx_unlock(&counter->mutex);

    return 0;
}

char *airy_token_counter_truncate(airy_token_counter_t *counter, const char *text,
                                  size_t max_tokens, const char *side)
{
    if (!counter || !text || max_tokens == 0) {
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    size_t length = strlen(text);
    if (length == 0) {
        return AIRY_STRDUP("");
    }

    airy_mtx_lock(&counter->mutex);

    size_t current_tokens = count_tok_family(counter->type, text, length);

    if (current_tokens <= max_tokens) {
        airy_mtx_unlock(&counter->mutex);
        return AIRY_STRDUP(text);
    }

    size_t target_chars = (length * max_tokens) / current_tokens;
    if (target_chars > length) {
        target_chars = length;
    }

    char *result = NULL;

    if (side && strcmp(side, "left") == 0) {
        result = AIRY_MALLOC(target_chars + 4);
        if (result) {
            __builtin_memcpy(result, text + length - target_chars, target_chars);
            result[target_chars] = '\0';
            snprintf(result + target_chars, 4, "...");
        }
    } else if (side && strcmp(side, "middle") == 0) {
        size_t half = target_chars / 2;
        result = AIRY_MALLOC(target_chars + 8);
        if (result) {
            __builtin_memcpy(result, text, half);
            result[half] = '\0';
            snprintf(result + half, target_chars + 8 - half, "...[truncated]...");
            size_t remaining_space = target_chars + 8 - (half + 15);
            if (remaining_space > 0) {
                size_t copy_len = (target_chars - half) < (remaining_space - 1) ?
                                      (target_chars - half) :
                                      (remaining_space - 1);
                __builtin_memcpy(result + half + 15, text + length - (target_chars - half),
                                 copy_len);
                result[half + 15 + copy_len] = '\0';
            }
        }
    } else {
        result = AIRY_MALLOC(target_chars + 4);
        if (result) {
            __builtin_memcpy(result, text, target_chars);
            result[target_chars] = '\0';
            snprintf(result + target_chars, 4, "...");
        }
    }

    airy_mtx_unlock(&counter->mutex);

    return result;
}
