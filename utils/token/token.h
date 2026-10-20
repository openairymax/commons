/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file token.h
 * @brief Token counting and budget management.
 */

#ifndef AIRY_RT_UTILS_TOKEN_H
#define AIRY_RT_UTILS_TOKEN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0.1.19 §164: UTF-8 词流扫描机制件（scan.c），词法唯一实现 */
#define AIRY_WORD_MAX 64

/**
 * @brief Word stream callback.
 * @param ud User context
 * @param word NUL-terminated word (ASCII lowercase / single UTF-8 char)
 */
typedef void (*airy_word_fn)(void *ud, const char *word);

/**
 * @brief Scan a text buffer into a word stream.
 * @param text Text buffer
 * @param len Buffer length in bytes (NUL terminates early)
 * @param fn Callback invoked per word
 * @param ud User context passed through to fn
 */
void airy_words_scan(const char *text, size_t len, airy_word_fn fn, void *ud);

typedef struct airy_token_counter airy_token_counter_t;

/**
 * @brief Create a token counter.
 * @param family_name Tokenizer family: generic | bpe | spm | uniform
 *                    (unknown names are rejected, never silently mapped)
 * @return Counter handle, NULL on failure
 */
airy_token_counter_t *airy_token_counter_create(const char *family_name);

/**
 * @brief Destroy a counter.
 */
void airy_token_counter_destroy(airy_token_counter_t *counter);

/**
 * @brief Count the tokens in a text.
 * @param counter Counter
 * @param text Text
 * @return Token count, (size_t)-1 on failure
 */
size_t airy_token_counter_count(airy_token_counter_t *counter, const char *text);

/**
 * @brief Count tokens for multiple texts in batch.
 * @param counter Counter
 * @param texts Text array
 * @param count Text count
 * @param out_counts Output token count array
 * @return 0 on success, -1 on failure
 */
size_t airy_token_counter_count_batch(airy_token_counter_t *counter, const char **texts,
                                      size_t count, size_t *out_counts);

/**
 * @brief Truncate text to a given token count.
 * @param counter Counter
 * @param text Input text
 * @param max_tokens Max token count
 * @param side Truncation side: "left","right","middle"
 * @return Newly allocated truncated text, NULL on failure
 */
char *airy_token_counter_truncate(airy_token_counter_t *counter, const char *text,
                                  size_t max_tokens, const char *side);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_TOKEN_H */
