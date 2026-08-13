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

#include "../../memory/include/airy_memory.h"
#include "../../string/include/string_compat.h"
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

size_t airy_token_count(const char *text, const airy_token_config_t *config)
{
    if (!text)
        return 0;

    size_t length = strlen(text);
    if (length == 0)
        return 0;

    airy_token_config_t default_cfg = AIRY_TOKEN_CONFIG_DEFAULT;
    const airy_token_config_t *cfg = config ? config : &default_cfg;

    size_t word_count = 0;
    size_t cjk_count = 0;
    size_t punct_count = 0;
    bool in_word = false;

    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)text[i];

        if (c < 0x80) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_') {
                if (!in_word) {
                    word_count++;
                    in_word = true;
                }
            } else {
                in_word = false;
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    /* whitespace */
                } else {
                    punct_count++;
                }
            }
            i++;
        } else {
            uint32_t code_point = 0;
            size_t bytes_consumed = 0;

            if (c >= 0xC2 && c <= 0xDF && i + 1 < length) {
                unsigned char n1 = (unsigned char)text[i + 1];
                if ((n1 & 0xC0) == 0x80) {
                    code_point = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(n1 & 0x3F);
                    bytes_consumed = 2;
                }
            } else if (c >= 0xE0 && c <= 0xEF && i + 2 < length) {
                unsigned char n1 = (unsigned char)text[i + 1];
                unsigned char n2 = (unsigned char)text[i + 2];
                if ((n1 & 0xC0) == 0x80 && (n2 & 0xC0) == 0x80) {
                    if (c == 0xE0 && n1 < 0xA0) {
                        i++;
                        continue;
                    }
                    if (c == 0xED && n1 > 0x9F) {
                        i++;
                        continue;
                    }
                    code_point = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(n1 & 0x3F) << 6) |
                                 (uint32_t)(n2 & 0x3F);
                    bytes_consumed = 3;
                }
            } else if (c >= 0xF0 && c <= 0xF4 && i + 3 < length) {
                unsigned char n1 = (unsigned char)text[i + 1];
                unsigned char n2 = (unsigned char)text[i + 2];
                unsigned char n3 = (unsigned char)text[i + 3];
                if ((n1 & 0xC0) == 0x80 && (n2 & 0xC0) == 0x80 && (n3 & 0xC0) == 0x80) {
                    if (c == 0xF0 && n1 < 0x90) {
                        i++;
                        continue;
                    }
                    if (c == 0xF4 && n1 > 0x8F) {
                        i++;
                        continue;
                    }
                    code_point = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(n1 & 0x3F) << 12) |
                                 ((uint32_t)(n2 & 0x3F) << 6) | (uint32_t)(n3 & 0x3F);
                    bytes_consumed = 4;
                }
            }

            if (bytes_consumed > 0) {
                int is_cjk = 0;
                if (code_point >= 0x4E00 && code_point <= 0x9FFF)
                    is_cjk = 1;
                else if (code_point >= 0x3400 && code_point <= 0x4DBF)
                    is_cjk = 1;
                else if (code_point >= 0x20000 && code_point <= 0x2A6DF)
                    is_cjk = 1;
                else if (code_point >= 0x2A700 && code_point <= 0x2B73F)
                    is_cjk = 1;
                else if (code_point >= 0x2B740 && code_point <= 0x2B81F)
                    is_cjk = 1;
                else if (code_point >= 0x2B820 && code_point <= 0x2CEAF)
                    is_cjk = 1;
                else if (code_point >= 0x2CEB0 && code_point <= 0x2EBEF)
                    is_cjk = 1;
                else if (code_point >= 0x30000 && code_point <= 0x3134F)
                    is_cjk = 1;
                else if (code_point >= 0x31350 && code_point <= 0x323AF)
                    is_cjk = 1;
                else if (code_point >= 0xF900 && code_point <= 0xFAFF)
                    is_cjk = 1;
                else if (code_point >= 0x2F800 && code_point <= 0x2FA1F)
                    is_cjk = 1;
                else if (code_point >= 0x3000 && code_point <= 0x303F)
                    is_cjk = 1;
                else if (code_point >= 0x3040 && code_point <= 0x309F)
                    is_cjk = 1;
                else if (code_point >= 0x30A0 && code_point <= 0x30FF)
                    is_cjk = 1;
                else if (code_point >= 0xAC00 && code_point <= 0xD7AF)
                    is_cjk = 1;
                else if (code_point >= 0xFF00 && code_point <= 0xFFEF)
                    is_cjk = 1;

                if (is_cjk) {
                    cjk_count++;
                } else {
                    word_count++;
                }
                in_word = false;
                i += bytes_consumed;
            } else {
                i++;
            }
        }
    }

    size_t count = word_count + cjk_count + (punct_count + 1) / 2;

    if (count == 0 && length > 0) {
        count = (length + 3) / 4;
    }

    switch (cfg->model_type) {
    case AIRY_TOKEN_MODEL_GPT4:
        count = (count * 4 + 2) / 3;
        break;
    case AIRY_TOKEN_MODEL_GPT35:
        count = (count * 5 + 2) / 4;
        break;
    case AIRY_TOKEN_MODEL_CLAUDE:
        count = (count * 7 + 2) / 5;
        break;
    case AIRY_TOKEN_MODEL_LLAMA:
        count = (count * 3 + 1) / 2;
        break;
    default:
        break;
    }

    return count;
}

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
