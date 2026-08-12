// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file estimator.c
 * @brief 成本预估器实现
 *
 * @details
 * 本模块实现LLM调用成本预估功能：
 * - 基于模型配置的成本计算
 * - 支持自定义费率配置
 * - 提供成本分析和报告接口
 */

#include "../../platform/include/platform.h"
#include "cost.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include <airy_memory.h>
#include <string_compat.h>
#include "error.h"

#define MAX_MODEL_NAME 64
#define MAX_CONFIG_ENTRIES 16

/**
 * @brief 模型成本配置
 */
typedef struct {
    char model_name[MAX_MODEL_NAME];
    double input_cost_per_1k;
    double output_cost_per_1k;
    int max_input_tokens;
    int max_output_tokens;
} model_cost_config_t;

/**
 * @brief 成本预估器内部结构
 */
struct airy_cost_estimator {
    model_cost_config_t configs[MAX_CONFIG_ENTRIES];
    int config_count;
    airy_mtx_t mutex;
    double total_cost;
    size_t total_input_tokens;
    size_t total_output_tokens;
    uint64_t request_count;
};

static const model_cost_config_t default_configs[] = {
    {"gpt-4o", 0.005, 0.015, 128000, 16384},
    {"gpt-4-turbo", 0.01, 0.03, 128000, 4096},
    {"gpt-4", 0.03, 0.06, 8192, 4096},
    {"gpt-3.5-turbo", 0.0005, 0.0015, 16385, 4096},
    {"claude-3-opus", 0.015, 0.075, 200000, 4096},
    {"claude-3-sonnet", 0.003, 0.015, 200000, 4096},
    {"claude-3-haiku", 0.00025, 0.00125, 200000, 4096},
    {"deepseek-chat", 0.00014, 0.00028, 163840, 16384},
    {"deepseek-coder", 0.00014, 0.00028, 163840, 16384},
    {"", 0.001, 0.002, 4096, 4096}};

static const model_cost_config_t *find_model_config(airy_cost_estimator_t *estimator,
                                                    const char *model_name)
{
    if (!estimator || !model_name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    for (int i = 0; i < estimator->config_count; i++) {
        if (strcasecmp(estimator->configs[i].model_name, model_name) == 0) {
            return &estimator->configs[i];
        }
    }

    for (size_t i = 0; i < sizeof(default_configs) / sizeof(default_configs[0]); i++) {
        if (strcasecmp(default_configs[i].model_name, model_name) == 0) {
            return &default_configs[i];
        }
    }

    return &default_configs[sizeof(default_configs) / sizeof(default_configs[0]) - 1];
}

static void normalize_model_name(const char *input, char *output, size_t output_size)
{
    if (!input || !output) {
        return;
    }

    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 1; i++) {
        if (!isspace((unsigned char)input[i])) {
            output[j++] = tolower((unsigned char)input[i]);
        }
    }
    output[j] = '\0';
}

static int load_config_from_file(airy_cost_estimator_t *estimator, const char *config_path)
{
    if (!config_path || !estimator)
        return AIRY_EINVAL;

    FILE *fp = fopen(config_path, "r");
    if (!fp)
        return AIRY_EINVAL;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        char *cr = strchr(line, '\r');
        if (cr)
            *cr = '\0';

        if (line[0] == '#' || line[0] == '\0')
            continue;

        char model[MAX_MODEL_NAME];
        double input_cost = 0.0, output_cost = 0.0;
        int max_in = 0, max_out = 0;
        char *saveptr;
        char *token;

        /* Manual parse: model,input_cost,output_cost,max_in,max_out */
        token = strtok_r(line, ",", &saveptr);
        if (!token)
            continue;
        AIRY_STRNCPY_TERM(model, token, MAX_MODEL_NAME);

        token = strtok_r(NULL, ",", &saveptr);
        if (token)
            input_cost = strtod(token, NULL);

        token = strtok_r(NULL, ",", &saveptr);
        if (token)
            output_cost = strtod(token, NULL);

        token = strtok_r(NULL, ",", &saveptr);
        if (token)
            max_in = (int)strtol(token, NULL, 10);

        token = strtok_r(NULL, ",", &saveptr);
        if (token)
            max_out = (int)strtol(token, NULL, 10);

        if (model[0] != '\0') {
            if (estimator->config_count < MAX_CONFIG_ENTRIES) {
                AIRY_STRNCPY_TERM(estimator->configs[estimator->config_count].model_name, model,
                                  MAX_MODEL_NAME);
                estimator->configs[estimator->config_count].input_cost_per_1k = input_cost;
                estimator->configs[estimator->config_count].output_cost_per_1k = output_cost;
                estimator->configs[estimator->config_count].max_input_tokens =
                    max_in > 0 ? max_in : 4096;
                estimator->configs[estimator->config_count].max_output_tokens =
                    max_out > 0 ? max_out : 4096;
                estimator->config_count++;
            }
        }
    }

    fclose(fp);
    return 0;
}

airy_cost_estimator_t *airy_cost_estimator_create(const char *config_path)
{
    airy_cost_estimator_t *estimator =
        (airy_cost_estimator_t *)AIRY_MALLOC(sizeof(airy_cost_estimator_t));
    if (!estimator) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    AIRY_MEMSET(estimator, 0, sizeof(airy_cost_estimator_t));

    if (airy_mtx_init(&estimator->mutex) != 0) {
        AIRY_FREE(estimator);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    estimator->config_count = 0;
    estimator->total_cost = 0.0;
    estimator->total_input_tokens = 0;
    estimator->total_output_tokens = 0;
    estimator->request_count = 0;

    for (size_t i = 0; i < sizeof(default_configs) / sizeof(default_configs[0]); i++) {
        if (estimator->config_count < MAX_CONFIG_ENTRIES) {
            AIRY_STRNCPY_TERM(estimator->configs[estimator->config_count].model_name,
                              default_configs[i].model_name, MAX_MODEL_NAME);
            estimator->configs[estimator->config_count].input_cost_per_1k =
                default_configs[i].input_cost_per_1k;
            estimator->configs[estimator->config_count].output_cost_per_1k =
                default_configs[i].output_cost_per_1k;
            estimator->configs[estimator->config_count].max_input_tokens =
                default_configs[i].max_input_tokens;
            estimator->configs[estimator->config_count].max_output_tokens =
                default_configs[i].max_output_tokens;
            estimator->config_count++;
        }
    }

    if (config_path && config_path[0] != '\0') {
        load_config_from_file(estimator, config_path);
    }

    return estimator;
}

void airy_cost_estimator_destroy(airy_cost_estimator_t *estimator)
{
    if (!estimator) {
        return;
    }

    airy_mtx_destroy(&estimator->mutex);
    AIRY_FREE(estimator);
}

double airy_cost_estimator_estimate(airy_cost_estimator_t *estimator, const char *model_name,
                                    size_t input_tokens, size_t output_tokens)
{
    if (!estimator || !model_name) {
        return AIRY_EINVAL;
    }

    char normalized[MAX_MODEL_NAME];
    normalize_model_name(model_name, normalized, sizeof(normalized));

    const model_cost_config_t *manager = find_model_config(estimator, normalized);
    if (!manager) {
        return AIRY_EINVAL;
    }

    double input_cost = (input_tokens / 1000.0) * manager->input_cost_per_1k;
    double output_cost = (output_tokens / 1000.0) * manager->output_cost_per_1k;
    double total_cost = input_cost + output_cost;

    airy_mtx_lock(&estimator->mutex);

    estimator->total_cost += total_cost;
    estimator->total_input_tokens += input_tokens;
    estimator->total_output_tokens += output_tokens;
    estimator->request_count++;

    airy_mtx_unlock(&estimator->mutex);

    return total_cost;
}

double airy_cost_estimator_get_total(airy_cost_estimator_t *estimator)
{
    if (!estimator) {
        return 0.0;
    }

    airy_mtx_lock(&estimator->mutex);
    double total = estimator->total_cost;
    airy_mtx_unlock(&estimator->mutex);

    return total;
}

size_t airy_cest_get_input_tokens(airy_cost_estimator_t *estimator)
{
    if (!estimator) {
        return 0;
    }

    airy_mtx_lock(&estimator->mutex);
    size_t tokens = estimator->total_input_tokens;
    airy_mtx_unlock(&estimator->mutex);

    return tokens;
}

size_t airy_cest_get_output_tokens(airy_cost_estimator_t *estimator)
{
    if (!estimator) {
        return 0;
    }

    airy_mtx_lock(&estimator->mutex);
    size_t tokens = estimator->total_output_tokens;
    airy_mtx_unlock(&estimator->mutex);

    return tokens;
}

uint64_t airy_cest_get_request_count(airy_cost_estimator_t *estimator)
{
    if (!estimator) {
        return 0;
    }

    airy_mtx_lock(&estimator->mutex);
    uint64_t count = estimator->request_count;
    airy_mtx_unlock(&estimator->mutex);

    return count;
}

void airy_cost_estimator_reset(airy_cost_estimator_t *estimator)
{
    if (!estimator) {
        return;
    }

    airy_mtx_lock(&estimator->mutex);

    estimator->total_cost = 0.0;
    estimator->total_input_tokens = 0;
    estimator->total_output_tokens = 0;
    estimator->request_count = 0;

    airy_mtx_unlock(&estimator->mutex);
}

int airy_cost_estimator_add_model(airy_cost_estimator_t *estimator, const char *model_name,
                                  double input_cost_per_1k, double output_cost_per_1k)
{
    if (!estimator || !model_name) {
        return AIRY_EINVAL;
    }

    if (estimator->config_count >= MAX_CONFIG_ENTRIES) {
        return AIRY_EINVAL;
    }

    airy_mtx_lock(&estimator->mutex);

    AIRY_STRNCPY_TERM(estimator->configs[estimator->config_count].model_name, model_name,
                      MAX_MODEL_NAME);
    estimator->configs[estimator->config_count].input_cost_per_1k = input_cost_per_1k;
    estimator->configs[estimator->config_count].output_cost_per_1k = output_cost_per_1k;
    estimator->configs[estimator->config_count].max_input_tokens = 4096;
    estimator->configs[estimator->config_count].max_output_tokens = 4096;
    estimator->config_count++;

    airy_mtx_unlock(&estimator->mutex);

    return 0;
}
