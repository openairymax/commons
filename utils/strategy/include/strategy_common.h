/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file strategy_common.h
 * @brief Strategy pattern shared utilities - eliminates strategy code dup.
 *
 * Provides common strategy-pattern implementations, including:
 * - Weighted scoring algorithm
 * - Scheduling strategy common structures
 * - Planning strategy common structures
 * - Cross-module shared strategy utility functions
 */

#ifndef AIRY_RT_STRATEGY_COMMON_H
#define AIRY_RT_STRATEGY_COMMON_H

#include <stdbool.h>
#include <stddef.h>

/* Unified base library compatibility layer */
#include "../../memory/include/airy_memory.h"
#include "../../string/include/string_compat.h"

#include <string.h>

/**
 * @brief Weighted scoring config.
 */
typedef struct weighted_config {
    float cost_weight;
    float perf_weight;
    float trust_weight;
} weighted_config_t;

/**
 * @brief Agent info structure (strategy common).
 */
typedef struct strategy_agent_info {
    float cost_estimate;
    float success_rate;
    float trust_score;
    const char *name;
    void *user_data;
} strategy_agent_info_t;

/**
 * @brief Strategy result structure.
 */
typedef struct strategy_result {
    int selected_index;
    float best_score;
    bool success;
} strategy_result_t;

/**
 * @brief Compute the weighted score.
 * @param agent Agent info
 * @param manager Weighted config
 * @return The computed score
 */
float strategy_compute_weighted_score(const strategy_agent_info_t *agent,
                                      const weighted_config_t *manager);

/**
 * @brief Select the best agent from an agent array.
 * @param agents Agent array
 * @param agent_count Agent count
 * @param manager Weighted config
 * @param result Output result
 * @return 0 on success, nonzero on failure
 */
int strategy_select_best_agent(const strategy_agent_info_t *agents, size_t agent_count,
                               const weighted_config_t *manager, strategy_result_t *result);

/**
 * @brief Create the default weighted config.
 * @return Default weighted config
 */
weighted_config_t strategy_create_default_weighted_config(void);

/**
 * @brief Validate a weighted config.
 * @param manager Weighted config
 * @return true if valid, false otherwise
 */
bool strategy_validate_weighted_config(const weighted_config_t *manager);

/**
 * @brief Normalize weights.
 * @param manager Weighted config
 * @return The normalized config
 */
weighted_config_t strategy_normalize_weights(const weighted_config_t *manager);

/**
 * @brief Common strategy data cleanup function.
 * @param data Data pointer
 * @param free_func Free function
 */
void strategy_cleanup_data(void *data, void (*free_func)(void *));

/**
 * @brief Strategy name generator.
 * @param base_name Base name
 * @param suffix Suffix
 * @return Generated strategy name (free with AIRY_FREE)
 */
char *strategy_generate_name(const char *base_name, const char *suffix);

#endif /* AIRY_RT_STRATEGY_COMMON_H */