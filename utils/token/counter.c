// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file counter.c
 * @brief Token counter implementation.
 *
 * Implements token counting and budget management:
 * - Character-level heuristic approximation (word/CJK/punctuation tokenization)
 * - Model-type coefficient adjustment (GPT-4/GPT-3.5/Claude/Llama)
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

#define MAX_MODEL_NAME 64

/**
 * @brief Token counter internal structure.
 */
struct airy_token_counter {
    char model_name[MAX_MODEL_NAME];
    airy_mtx_t mutex;
    size_t total_count;
    uint64_t request_count;
    size_t max_token_length;
};

static size_t count_tokens_by_model(const char *model_name, const char *text, size_t length)
{
    airy_token_config_t config = AIRY_TOKEN_CONFIG_DEFAULT;

    if (model_name) {
        if (strstr(model_name, "gpt-4") || strstr(model_name, "gpt-4o")) {
            config.model_type = AIRY_TOKEN_MODEL_GPT4;
        } else if (strstr(model_name, "gpt-35") || strstr(model_name, "gpt-3.5")) {
            config.model_type = AIRY_TOKEN_MODEL_GPT35;
        } else if (strstr(model_name, "claude")) {
            config.model_type = AIRY_TOKEN_MODEL_CLAUDE;
        } else if (strstr(model_name, "llama") || strstr(model_name, "vicuna") ||
                   strstr(model_name, "alpaca")) {
            config.model_type = AIRY_TOKEN_MODEL_LLAMA;
        }
    }

    return airy_token_standard_count(text, length, &config);
}

airy_token_counter_t *airy_token_counter_create(const char *model_name)
{
    if (!model_name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    airy_token_counter_t *counter =
        (airy_token_counter_t *)AIRY_MALLOC(sizeof(airy_token_counter_t));
    if (!counter) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    AIRY_MEMSET(counter, 0, sizeof(airy_token_counter_t));

    AIRY_STRNCPY_TERM(counter->model_name, model_name, MAX_MODEL_NAME);
    counter->model_name[MAX_MODEL_NAME - 1] = '\0';

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

    size_t token_count = count_tokens_by_model(counter->model_name, text, length);
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
            out_counts[i] = count_tokens_by_model(counter->model_name, texts[i], len);
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

    size_t current_tokens = count_tokens_by_model(counter->model_name, text, length);

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
