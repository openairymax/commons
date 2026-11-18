/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file config_attest.h
 * @brief 配置生效自证契约（0.1.21 A-8 的机制面）
 *
 * 问题（§16.19.6 A-8）：配置解析、注册表构建、运行态标志三段各自为政，
 * 「写了」不等于「生效」。`default_model` 被写成展示名、`think.enabled`
 * 被写成非法值，启动全程无一报错，直到首次调用才暴露为远端 404 或静默降级。
 *
 * 机制与策略分离：本契约只回答「声明值、注册结果、生效值三者是否闭合」，
 * 不回答「检查哪些键、不闭合时怎么办」——后者由 llm_d / think_d 的启动段
 * 以条目表与本节返回值自行裁决。本文件不感知模型、槽位、GRAD 等业务概念。
 *
 * 判定三值（§12.3）：PASS / REJECT / ESCAPED 三值三码，
 * 禁止把「已打印」当成「已闭合」。
 *
 * 调用约定：
 *   airy_attest_run 返回 AIRY_SUCCESS 表示可放行（PASS 或 ESCAPED）；
 *   返回 AIRY_ERR_SVC_CONFIG 表示存在不闭合条目且逃生开关未开（REJECT），
 *   调用方应当拒绝启动并让进程以非零码退出。
 *   参数非法返回 AIRY_ERR_INVALID_PARAM，不与判定结果混用。
 *
 * 指纹：对「键=声明|生效|注册」序列做 FNV-1a 64，取 16 位十六进制。
 * 同一份配置必然得到同一指纹，可用于跨版本/跨机器比对「到底生效了什么」。
 */

#ifndef AIRY_RT_CONFIG_ATTEST_H
#define AIRY_RT_CONFIG_ATTEST_H

#include <stddef.h>

#include "error_codes.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 判定三值：取值两两不同，禁止用布尔代替。 */
typedef enum {
    AIRY_ATTEST_PASS = 0,   /* 全部条目闭合 */
    AIRY_ATTEST_REJECT = 1, /* 存在不闭合条目，调用方应拒绝启动 */
    AIRY_ATTEST_ESCAPED = 2 /* 不闭合但逃生开关生效，仅告警放行 */
} airy_attest_verdict_t;

/* 注册表命中三值：未知（未提供注册信息）不得被当成「未命中」。 */
#define AIRY_ATTEST_REG_HIT 1  /* 声明值在注册表内 */
#define AIRY_ATTEST_REG_MISS 0 /* 声明值不在注册表内 */
#define AIRY_ATTEST_REG_NA (-1) /* 本条目不做注册对账 */

/** 单条对账项；字符串均为借用，调用方保证在本调用期间有效。 */
typedef struct {
    const char *key;       /* 配置键名，如 default_model / think.enabled */
    const char *declared;  /* 声明值（经完整优先级链后的最终决定） */
    const char *effective; /* 生效值（运行态实际持有） */
    int in_registry;       /* AIRY_ATTEST_REG_* 三值之一 */
} airy_attest_item_t;

/** 对账选项；owner 用于日志归属，allow_unknown 为逃生开关。 */
typedef struct {
    const char *owner;   /* 对账方标识，如 llm_d / think_d */
    int allow_unknown;   /* 非零：不闭合仅告警（排障逃生），仍打印 ESCAPED */
} airy_attest_opts_t;

/** 对账结果；fingerprint 为 NUL 结尾的 16 位十六进制串。 */
typedef struct {
    int checked;  /* 参与对账的条目数 */
    int ok;       /* 闭合条目数 */
    int warned;   /* 不闭合但被逃生开关放行的条目数 */
    int failed;   /* 不闭合且未放行的条目数 */
    airy_attest_verdict_t verdict;
    char fingerprint[24];
} airy_attest_result_t;

/**
 * @brief 对一个条目表做「声明 = 注册 = 生效」对账并打印生效指纹。
 *
 * 对每条目：声明的非空值必须与生效值逐字节一致；in_registry 非 NA 时
 * 必须命中。空声明表示「未声明、按回落生效」，只做注册对账不做一致性对账。
 *
 * @param opts  [in] 对账选项（owner 可为 NULL，按 "unknown" 记）
 * @param items [in] 条目数组（BORROW，n>0 时不得为 NULL）
 * @param n     [in] 条目数
 * @param out   [out] 结果（必填）
 * @return AIRY_SUCCESS（PASS/ESCAPED）、AIRY_ERR_SVC_CONFIG（REJECT）、
 *         AIRY_ERR_INVALID_PARAM（参数非法）
 */
int airy_attest_run(const airy_attest_opts_t *opts, const airy_attest_item_t *items, size_t n,
                    airy_attest_result_t *out);

/**
 * @brief 判定三值的稳定名称，用于日志与状态输出（永不返回 NULL）。
 */
const char *att_verdict(airy_attest_verdict_t verdict);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CONFIG_ATTEST_H */
