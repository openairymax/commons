/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cognition_common.h
 * @brief Common cognition-module definitions.
 *
 * Provides functionality shared by the cognition modules -- planning,
 * dispatch, coordination, etc. -- reducing code duplication between them.
 */

#ifndef COGNITION_COMMON_H
#define COGNITION_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Agent information structure
 */
typedef struct {
    char *agent_id; /**< Agent ID */
    double weight;
    double success_rate;
    uint64_t total_tasks;
    uint64_t successful_tasks;
    double avg_latency;
    uint64_t last_used;
} agent_info_t;

/**
 * @brief Task information structure
 */
typedef struct {
    char *task_id;
    char *task_type;
    char *task_content;
    uint64_t priority;
    uint64_t deadline;
} task_info_t;

/**
 * @brief Plan result structure
 */
typedef struct {
    bool success;
    char *plan;
    size_t plan_size;
    char *error;
    size_t error_size;
} plan_result_t;

/**
 * @brief Dispatch result structure
 */
typedef struct {
    bool success;
    char *selected_agent;
    double confidence;
    char *error;
    size_t error_size;
} dispatch_result_t;

/**
 * @brief Coordination result structure
 */
typedef struct {
    bool success;
    char *decision;
    size_t decision_size;
    char *error;
    size_t error_size;
} coordination_result_t;

/**
 * @brief Initialize agent information
 * @param agent Agent information pointer
 * @param agent_id Agent ID
 * @return 0 on success, non-zero on failure
 */
int agent_info_init(agent_info_t *agent, const char *agent_id);

/**
 * @brief Clean up agent information
 * @param agent Agent information pointer
 */
void agent_info_cleanup(agent_info_t *agent);

/**
 * @brief Update agent performance statistics
 * @param agent Agent information pointer
 * @param success Whether successful
 * @param latency Latency
 */
void agent_info_update_stats(agent_info_t *agent, bool success, uint64_t latency);

/**
 * @brief Calculate the agent weight
 * @param agent Agent information pointer
 * @return Calculated weight
 */
double agent_info_calculate_weight(const agent_info_t *agent);

/**
 * @brief Initialize task information
 * @param task Task information pointer
 * @param task_id Task ID
 * @param task_type Task type
 * @param task_content Task content
 * @return 0 on success, non-zero on failure
 */
int task_info_init(task_info_t *task, const char *task_id, const char *task_type,
                   const char *task_content);

/**
 * @brief Clean up task information
 * @param task Task information pointer
 */
void task_info_cleanup(task_info_t *task);

/**
 * @brief Initialize a plan result
 * @param result Plan result pointer
 * @return 0 on success, non-zero on failure
 */
int plan_result_init(plan_result_t *result);

/**
 * @brief Clean up a plan result
 * @param result Plan result pointer
 */
void plan_result_cleanup(plan_result_t *result);

/**
 * @brief Initialize a dispatch result
 * @param result Dispatch result pointer
 * @return 0 on success, non-zero on failure
 */
int dispatch_result_init(dispatch_result_t *result);

/**
 * @brief Clean up a dispatch result
 * @param result Dispatch result pointer
 */
void dispatch_result_cleanup(dispatch_result_t *result);

/**
 * @brief Initialize a coordination result
 * @param result Coordination result pointer
 * @return 0 on success, non-zero on failure
 */
int coordination_result_init(coordination_result_t *result);

/**
 * @brief Clean up a coordination result
 * @param result Coordination result pointer
 */
void coordination_result_cleanup(coordination_result_t *result);

/**
 * @brief Select the best agent
 * @param agents Agent array
 * @param agent_count Number of agents
 * @param task Task information
 * @param result Dispatch result
 * @return 0 on success, non-zero on failure
 */
int cognition_select_best_agent(agent_info_t *agents, size_t agent_count, const task_info_t *task,
                                dispatch_result_t *result);

/**
 * @brief Generate a plan
 * @param task Task information
 * @param result Plan result
 * @return 0 on success, non-zero on failure
 */
int cognition_generate_plan(const task_info_t *task, plan_result_t *result);

/**
 * @brief Coordinate the results of multiple agents
 * @param agent_results Results from multiple agents
 * @param result_count Number of results
 * @param result Coordination result
 * @return 0 on success, non-zero on failure
 */
int cognition_coordinate_results(const char **agent_results, size_t result_count,
                                 coordination_result_t *result);

/**
 * @brief Calculate the task priority
 * @param task Task information
 * @return Priority value
 */
uint64_t cognition_calculate_task_priority(const task_info_t *task);

/**
 * @brief Evaluate plan quality
 * @param plan Plan content
 * @param task Task information
 * @return Quality score (0-100)
 */
int cognition_evaluate_plan_quality(const char *plan, const task_info_t *task);

#ifdef __cplusplus
}
#endif

#endif /* COGNITION_COMMON_H */
