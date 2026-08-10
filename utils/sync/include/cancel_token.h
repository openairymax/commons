// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file cancel_token.h
 * @brief 异步可中断执行模型：取消令牌（改进1：Codex parallel.rs cancel_token 模式）
 *
 * 原子 cancelled 标志 + 回调唤醒链，贯穿 taskflow → work_hall → sched_d
 * executor → agent_d/tool_d。任一持有令牌的实体可请求取消；等待方通过
 * airy_cancel_token_is_canceled() 轮询、airy_cancel_token_wait() 阻塞
 * 等待（条件变量唤醒，非忙轮询）；注册的唤醒回调在取消瞬间触发（用于
 * 唤醒 poll/select 等无法直接阻塞在令牌上的 I/O 等待）。
 *
 * 线程安全：全部公共接口线程安全。
 * 纯 C 核心，跨 Linux/macOS/Windows（依赖 platform 层 airy_mtx/airy_cond/airy_atomic）。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AIRY_RT_CANCEL_TOKEN_H
#define AIRY_RT_CANCEL_TOKEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h" /* airy_mtx_t / airy_cond_t / airy_atomic_int_t（值类型成员） */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 取消回调数量上限（有界回调链，防资源失控） */
#define AIRY_CANCEL_TOKEN_MAX_CBS 8

/**
 * @brief 取消回调：令牌被取消时触发（用于唤醒阻塞 I/O 等待方）
 * @param ctx 注册时透传的用户上下文
 */
typedef void (*airy_cancel_cb_t)(void *ctx);

/**
 * @brief 取消令牌结构（不透明使用：调用方持有实例，勿直接访问内部字段）
 */
typedef struct airy_cancel_token {
    airy_mtx_t lock;         /**< 内部互斥锁（保护回调链） */
    airy_cond_t cond;        /**< 内部条件变量（wait 唤醒） */
    airy_atomic_int_t cancelled; /**< 原子取消标志（0=活跃，1=已取消） */
    int init_done;           /**< 初始化标记（destroy 幂等依据） */
    airy_cancel_cb_t cbs[AIRY_CANCEL_TOKEN_MAX_CBS]; /**< 唤醒回调链 */
    void *cb_ctxs[AIRY_CANCEL_TOKEN_MAX_CBS];        /**< 回调上下文 */
    size_t cb_count;         /**< 已注册回调数 */
} airy_cancel_token_t;

/**
 * @brief 初始化取消令牌（活跃状态）
 * @param token 令牌指针（非 NULL）
 * @return 0 成功；AIRY_EINVAL 参数非法；AIRY_ERR_SYS_MUTEX/CONDITION 底层创建失败
 */
int airy_cancel_token_init(airy_cancel_token_t *token);

/**
 * @brief 销毁取消令牌（幂等）
 * @param token 令牌指针（可为 NULL）
 */
void airy_cancel_token_destroy(airy_cancel_token_t *token);

/**
 * @brief 请求取消：置位原子标志并触发全部唤醒回调（幂等，重复调用仅触发一次）
 * @param token 令牌指针（可为 NULL）
 */
void airy_cancel_token_cancel(airy_cancel_token_t *token);

/**
 * @brief 检查是否已取消（线程安全，无锁读原子标志）
 * @param token 令牌指针（可为 NULL）
 * @return true 已取消；false 活跃或参数非法
 */
bool airy_cancel_token_is_canceled(const airy_cancel_token_t *token);

/**
 * @brief 复位令牌至活跃状态（供取消后恢复重跑复用；不重置回调链）
 * @param token 令牌指针（可为 NULL）
 */
void airy_cancel_token_reset(airy_cancel_token_t *token);

/**
 * @brief 注册唤醒回调（追加到回调链；超上限返回 AIRY_ERR_OVERFLOW）
 * @param token 令牌指针（非 NULL）
 * @param cb 回调（非 NULL）
 * @param ctx 回调上下文（可为 NULL）
 * @return 0 成功；AIRY_EINVAL 参数非法；AIRY_ERR_OVERFLOW 回调链已满
 */
int airy_cancel_token_register(airy_cancel_token_t *token, airy_cancel_cb_t cb, void *ctx);

/**
 * @brief 阻塞等待取消或超时（条件变量唤醒，非忙轮询）
 * @param token 令牌指针（非 NULL）
 * @param timeout_ms 超时毫秒（0 = 无限等待）
 * @return 0 已取消；1 超时；AIRY_EINVAL 参数非法
 */
int airy_cancel_token_wait(airy_cancel_token_t *token, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CANCEL_TOKEN_H */
