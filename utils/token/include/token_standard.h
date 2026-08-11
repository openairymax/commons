/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file token_standard.h
 * @brief Token 计算标准化接口 - 统一 C/Python Token 计算算法
 *
 * 根据 AgentRT 架构原则（E-3 资源确定性）设计，提供确定性的 Token 计算标准。
 * 确保跨语言实现的一致性，支持资源配额管理和监控集成。
 *
 * @version 0.1.0
 * @date 2026-04-07
 */

#ifndef AIRY_RT_TOKEN_STANDARD_H
#define AIRY_RT_TOKEN_STANDARD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Token 计算算法版本
 */
#define AIRY_TOKEN_ALGORITHM_VERSION "1.0"

/**
 * @brief Token 计算模型类型
 */
typedef enum {
    AIRY_TOKEN_MODEL_GENERIC = 0,
    AIRY_TOKEN_MODEL_GPT4,
    AIRY_TOKEN_MODEL_GPT35,
    AIRY_TOKEN_MODEL_CLAUDE,
    AIRY_TOKEN_MODEL_LLAMA,
    AIRY_TOKEN_MODEL_CUSTOM
} airy_token_model_t;

/**
 * @brief Token 计算配置
 */
typedef struct {
    airy_token_model_t model_type;
    const char *model_name;
    float cjk_ratio;
    float alpha_ratio;
    uint32_t flags;
} airy_token_config_t;

/**
 * @brief Token 计算标志位
 */
#define AIRY_TOKEN_FLAG_ACCURATE 0x01
#define AIRY_TOKEN_FLAG_ESTIMATE 0x02
#define AIRY_TOKEN_FLAG_INCLUDE_BOM 0x04

/**
 * @brief 默认 Token 计算配置
 */
#define AIRY_TOKEN_CONFIG_DEFAULT            \
    {.model_type = AIRY_TOKEN_MODEL_GENERIC, \
     .model_name = "generic",                \
     .cjk_ratio = 0.3f,                      \
     .alpha_ratio = 0.5f,                    \
     .flags = AIRY_TOKEN_FLAG_ESTIMATE}

/**
 * @brief 标准化 Token 计算函数
 *
 * 根据配置计算文本的 Token 数量，确保跨语言实现的一致性。
 *
 * @param text 输入文本（UTF-8 编码）
 * @param length 文本长度（字节数），如果为 0 则自动计算
 * @param config Token 计算配置，如果为 NULL 则使用默认配置
 * @return Token 数量，如果出错返回 (size_t)-1
 */
size_t airy_token_standard_count(const char *text, size_t length,
                                 const airy_token_config_t *config);

/**
 * @brief 批量 Token 计算
 *
 * 一次性计算多个文本的 Token 数量，提高效率。
 *
 * @param texts 文本数组
 * @param lengths 长度数组（字节数），如果为 NULL 则每个文本自动计算长度
 * @param count 文本数量
 * @param out_counts 输出 Token 数量数组
 * @param config Token 计算配置，如果为 NULL 则使用默认配置
 * @return 成功返回 0，失败返回错误码
 */
int airy_token_standard_count_batch(const char **texts, const size_t *lengths, size_t count,
                                    size_t *out_counts, const airy_token_config_t *config);

/**
 * @brief 检测文本语言特征
 *
 * 分析文本的语言特征，用于优化 Token 计算。
 *
 * @param text 输入文本
 * @param length 文本长度
 * @param out_cjk_chars 输出中日韩字符数量
 * @param out_alpha_chars 输出字母字符数量
 * @param out_total_chars 输出总字符数量
 * @return 成功返回 0，失败返回错误码
 */
int airy_token_analyze_text(const char *text, size_t length, size_t *out_cjk_chars,
                            size_t *out_alpha_chars, size_t *out_total_chars);

/**
 * @brief 获取 Token 计算算法信息
 *
 * @return 算法描述字符串
 */
const char *airy_token_get_algorithm_info(void);

/**
 * @brief 验证 Token 计算配置
 *
 * @param config 配置参数
 * @return 配置有效返回 0，无效返回错误码
 */
int airy_token_validate_config(const airy_token_config_t *config);

/**
 * @brief Token 计算精度级别
 */
typedef enum {
    AIRY_TOKEN_PRECISION_LOW = 0,
    AIRY_TOKEN_PRECISION_MEDIUM,
    AIRY_TOKEN_PRECISION_HIGH
} airy_token_precision_t;

/**
 * @brief 设置 Token 计算精度
 *
 * @param precision 精度级别
 * @param config 输出配置（可选）
 * @return 成功返回 0，失败返回错误码
 */
int airy_token_set_precision(airy_token_precision_t precision, airy_token_config_t *config);

/**
 * @brief 资源配额限制
 */
typedef struct {
    size_t max_tokens_per_request;
    size_t max_tokens_per_minute;
    size_t max_tokens_per_hour;
    size_t max_tokens_per_day;
    size_t max_requests_per_minute;
    size_t max_requests_per_hour;
    size_t max_requests_per_day;
} airy_token_quota_t;

/**
 * @brief 默认资源配额
 */
#define AIRY_TOKEN_QUOTA_DEFAULT     \
    {.max_tokens_per_request = 8000, \
     .max_tokens_per_minute = 60000, \
     .max_tokens_per_hour = 360000,  \
     .max_tokens_per_day = 2000000,  \
     .max_requests_per_minute = 60,  \
     .max_requests_per_hour = 3600,  \
     .max_requests_per_day = 10000}

/**
 * @brief 资源使用情况
 */
typedef struct {
    size_t tokens_used_per_minute;
    size_t tokens_used_per_hour;
    size_t tokens_used_per_day;
    size_t requests_used_per_minute;
    size_t requests_used_per_hour;
    size_t requests_used_per_day;
} airy_token_usage_t;

/**
 * @brief 检查资源配额是否足够
 *
 * 逐级检查所有配额限制：单次请求、分钟级、小时级、日级。
 * 当 current_usage 为 NULL 时，仅检查单次请求限制。
 *
 * @param quota 配额限制
 * @param requested_tokens 请求的 Token 数量
 * @param current_usage 当前使用情况（可为 NULL）
 * @return 0 配额足够，1 超出单次请求限制，2 超出分钟Token限制，
 *         3 超出小时Token限制，4 超出日Token限制，
 *         5 超出分钟请求限制，6 超出小时请求限制，7 超出日请求限制
 */
int airy_token_check_quota(const airy_token_quota_t *quota, size_t requested_tokens,
                           const airy_token_usage_t *current_usage);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TOKEN_STANDARD_H */