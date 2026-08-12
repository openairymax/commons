/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cost.h
 * @brief Cost estimation and control.
 */

#ifndef AIRY_RT_UTILS_COST_H
#define AIRY_RT_UTILS_COST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct airy_cost_estimator airy_cost_estimator_t;

/**
 * @brief Create a cost estimator
 * @param config_path Config file path (YAML); NULL for the built-in
 *                    defaults
 * @return Estimator handle, NULL on failure
 */
airy_cost_estimator_t *airy_cost_estimator_create(const char *config_path);

/* From data intelligence emerges. by spharx */
/**
 * @brief Destroy an estimator
 */
void airy_cost_estimator_destroy(airy_cost_estimator_t *estimator);

/**
 * @brief Estimate the cost of one call
 * @param estimator Estimator
 * @param model_name Model name
 * @param input_tokens Input token count
 * @param output_tokens Output token count
 * @return Cost (USD), -1.0 on failure
 */
double airy_cost_estimator_estimate(airy_cost_estimator_t *estimator, const char *model_name,
                                    size_t input_tokens, size_t output_tokens);

typedef struct airy_budget_controller airy_budget_controller_t;

/**
 * @brief Create a budget controller
 * @param max_cost_usd Maximum cost (USD)
 * @param period_seconds Period (seconds)
 * @return Controller handle, NULL on failure
 */
airy_budget_controller_t *airy_budget_controller_create(double max_cost_usd,
                                                        uint32_t period_seconds);

/**
 * @brief Destroy a controller
 */
void airy_budget_controller_destroy(airy_budget_controller_t *controller);

/**
 * @brief Consume cost
 * @param controller Controller
 * @param cost_usd Cost
 * @return 0 on success, -1 if over budget
 */
int airy_budget_controller_consume(airy_budget_controller_t *controller, double cost_usd);

/**
 * @brief Get the remaining budget
 * @return Remaining cost (USD)
 */
double airy_budget_controller_remaining(airy_budget_controller_t *controller);

/**
 * @brief Get the consumed cost
 * @param controller Controller
 * @return Consumed cost (USD)
 */
double airy_budget_controller_consumed(airy_budget_controller_t *controller);

/**
 * @brief Get the consumption within the current period
 * @param controller Controller
 * @return Period consumption (USD)
 */
double airy_budget_controller_period_consumed(airy_budget_controller_t *controller);

/**
 * @brief Get the request count
 * @param controller Controller
 * @return Request count
 */
uint64_t airy_budget_controller_requests(airy_budget_controller_t *controller);

/**
 * @brief Get the denied count
 * @param controller Controller
 * @return Denied count
 */
uint64_t airy_budget_controller_denied(airy_budget_controller_t *controller);

/**
 * @brief Set the warning threshold
 * @param controller Controller
 * @param threshold Threshold (0.0-1.0)
 * @return 0 on success, -1 on failure
 */
int airy_budget_controller_set_warning(airy_budget_controller_t *controller, double threshold);

/**
 * @brief Reset the period
 * @param controller Controller
 * @return 0 on success, -1 on failure
 */
int airy_budget_controller_reset_period(airy_budget_controller_t *controller);

/**
 * @brief Get the average cost
 * @param controller Controller
 * @return Average cost (USD)
 */
double airy_budget_controller_average(airy_budget_controller_t *controller);

/**
 * @brief Get the budget status
 * @param controller Controller
 * @return 0=normal, 1=warning, 2=over limit, -1=failure
 */
int airy_budget_controller_get_status(airy_budget_controller_t *controller);

/**
 * @brief Get the cumulative total cost
 * @param estimator Estimator
 * @return Cumulative cost (USD)
 */
double airy_cost_estimator_get_total(airy_cost_estimator_t *estimator);

/**
 * @brief Get the cumulative input token count
 * @param estimator Estimator
 * @return Cumulative input token count
 */
size_t airy_cost_estimator_get_input_tokens(airy_cost_estimator_t *estimator);

/**
 * @brief Get the cumulative output token count
 * @param estimator Estimator
 * @return Cumulative output token count
 */
size_t airy_cost_estimator_get_output_tokens(airy_cost_estimator_t *estimator);

/**
 * @brief Get the request count
 * @param estimator Estimator
 * @return Request count
 */
uint64_t airy_cost_estimator_get_request_count(airy_cost_estimator_t *estimator);

/**
 * @brief Reset statistics
 * @param estimator Estimator
 */
void airy_cost_estimator_reset(airy_cost_estimator_t *estimator);

/**
 * @brief Add a custom model configuration
 * @param estimator Estimator
 * @param model_name Model name
 * @param input_cost_per_1k Input cost (USD per 1K tokens)
 * @param output_cost_per_1k Output cost (USD per 1K tokens)
 * @return 0 on success, -1 on failure
 */
int airy_cost_estimator_add_model(airy_cost_estimator_t *estimator, const char *model_name,
                                  double input_cost_per_1k, double output_cost_per_1k);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_COST_H */
