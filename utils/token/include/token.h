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

typedef struct airy_token_counter airy_token_counter_t;

/**
 * @brief Create a token counter.
 * @param model_name Model name (e.g. "gpt-4")
 * @return Counter handle, NULL on failure
 */
airy_token_counter_t *airy_token_counter_create(const char *model_name);

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

/**
 * @brief Token budget handle.
 */
typedef struct airy_token_budget airy_token_budget_t;

/**
 * @brief Create a token budget.
 * @param max_tokens Max token count
 * @return Budget handle, NULL on failure
 */
airy_token_budget_t *airy_token_budget_create(size_t max_tokens);

/**
 * @brief Destroy a budget.
 */
void airy_token_budget_destroy(airy_token_budget_t *budget);

/**
 * @brief Add token consumption.
 * @param budget Budget
 * @param input_tokens Input token count
 * @param output_tokens Output token count
 * @return 0 on success, -1 over budget
 */
int airy_token_budget_add(airy_token_budget_t *budget, size_t input_tokens, size_t output_tokens);

/**
 * @brief Get the remaining token count.
 * @return Remaining token count
 */
size_t airy_token_budget_remaining(airy_token_budget_t *budget);

/**
 * @brief Reset the budget.
 */
void airy_token_budget_reset(airy_token_budget_t *budget);

/**
 * @brief Get the used token count.
 * @param budget Budget
 * @return Used token count
 */
size_t airy_token_budget_used(airy_token_budget_t *budget);

/**
 * @brief Get the input token count.
 * @param budget Budget
 * @return Input token count
 */
size_t airy_token_budget_input(airy_token_budget_t *budget);

/**
 * @brief Get the output token count.
 * @param budget Budget
 * @return Output token count
 */
size_t airy_token_budget_output(airy_token_budget_t *budget);

/**
 * @brief Get the request count.
 * @param budget Budget
 * @return Request count
 */
uint32_t airy_token_budget_requests(airy_token_budget_t *budget);

/**
 * @brief Get the denied count.
 * @param budget Budget
 * @return Denied count
 */
uint32_t airy_token_budget_denied(airy_token_budget_t *budget);

/**
 * @brief Set the time window.
 * @param budget Budget
 * @param window_seconds Time window (seconds)
 * @return 0 on success, -1 on failure
 */
int airy_token_budget_set_window(airy_token_budget_t *budget, size_t window_seconds);

/**
 * @brief Check and reset the time window.
 * @param budget Budget
 * @return 0 on success, -1 on failure
 */
int airy_token_budget_check_window(airy_token_budget_t *budget);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_TOKEN_H */