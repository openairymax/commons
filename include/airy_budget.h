/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file airy_budget.h
 * @brief 超时预算树契约（0.1.21 C-21 的机制面）
 *
 * 问题：一次请求要穿过客户端、gateway、agent_d、llm_d/providers、tool_d
 * 五层，每层自带一个写死的超时。内层先放弃，外层还在等；外层先放弃，
 * 内层的结果被丢弃却照常落库。C-21 实测到的冲突是 think 的调用方与
 * think_d 服务端同为 120s、零余量——任何一次 GC 停顿都会让调用方先撤。
 *
 * 机制与策略分离：本契约只回答「每层的生效预算是什么、这组值是否构成
 * 单调且有 20% 余量的预算树」，不回答「某层配多少、业务允许多久」。
 * 层级顺序固定 client ≥ server ≥ provider ≥ tool；think 是 server 的
 * 被调用方，额外要求 server ≥ think × 1.2。
 *
 * 解析顺序：层内环境变量 AIRY_TIMEOUT_<LAYER>_MS > AIRY_THINK_TIMEOUT_MS
 * （think 专用，兼容既有配置）> AIRY_TIMEOUT_PROFILE（small / default /
 * large）> 内置缺省。非法值忽略并保留上一层来源——配置错误不得放宽内层。
 *
 * 单调性在本契约就地钳制：被调用方预算高于调用方时抬高调用方，并返回
 * 修复次数。钳制只向上、永不缩短，故错误配置不会把超时压到比内层还短。
 */

#ifndef AIRY_RT_BUDGET_H
#define AIRY_RT_BUDGET_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AIRY_BUDGET_CLIENT = 0, /* 客户端/gateway 对外等待 */
    AIRY_BUDGET_SERVER,     /* daemon 内部一次完整调用的外层上限 */
    AIRY_BUDGET_THINK,      /* think_d 服务端自身预算（server 的被调用方） */
    AIRY_BUDGET_PROVIDER,   /* 单次 provider HTTP 往返 */
    AIRY_BUDGET_TOOL,       /* 工具兜底值（per-tool 覆盖优先） */
    AIRY_BUDGET_LAYERS
} airy_budget_layer_t;

/** @brief 该层生效预算（毫秒）；越界层号返回 0。 */
unsigned airy_budget_ms(airy_budget_layer_t layer);

/** @brief 调用方预算：在 inner 之上留 25% 余量并对齐到 1s。 */
unsigned airy_budget_outer(unsigned inner_ms);

/** @brief 生效配置档名（small/default/large，非法值回落 default）。 */
const char *airy_budget_profile(void);

/** @brief 预算树自检：0 表示已单调且有余量，非 0 为被钳制的层数。 */
int airy_budget_verify(void);

/** @brief 启动时打印生效预算；who 为层名，仅用于日志定位。 */
void airy_budget_log(const char *who);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_BUDGET_H */
