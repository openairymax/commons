/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file ipc_backpressure.h
 * @brief IPC Bus 背压控制 — 三级策略
 *
 * P0.17 阶段 4：从 daemons/common/include/ 迁移至 commons，
 * 消除 atoms→daemons 编译期反向依赖（IRON-6）。daemons 版保留为重导出兼容头。
 *
 * P1.24: 三级背压策略防止 IPC Bus 过载：
 *   - Queue > 80%: 生产者降速
 *   - Queue > 90%: Droppable 消息丢弃
 *   - Queue > 95%: 拒绝新连接 + 告警
 *   - Queue < 60%: 恢复正常速率
 */

#ifndef AIRY_RT_IPC_BACKPRESSURE_H
#define AIRY_RT_IPC_BACKPRESSURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 背压级别
 */
typedef enum {
    IPC_BP_NORMAL = 0,
    IPC_BP_SLOW = 1,
    IPC_BP_DROP = 2,
    IPC_BP_REJECT = 3
} ipc_bp_level_t;

/**
 * @brief 背压配置
 */
typedef struct {
    size_t queue_capacity;
    uint32_t slow_threshold_pct;
    uint32_t drop_threshold_pct;
    uint32_t reject_threshold_pct;
    uint32_t recover_threshold_pct;
    uint32_t sample_interval_ms;
} ipc_bp_config_t;

/**
 * @brief 背压统计
 */
typedef struct {
    ipc_bp_level_t current_level;
    size_t queue_depth;
    size_t queue_capacity;
    uint64_t total_sent;
    uint64_t total_dropped;
    uint64_t total_rejected;
    uint64_t slow_down_events;
    uint64_t recover_events;
} ipc_bp_stats_t;

/**
 * @brief 背压控制器句柄
 */
typedef struct ipc_bp_controller ipc_bp_controller_t;

/**
 * @brief 创建背压控制器
 *
 * @param config 配置（NULL 使用默认）
 * @return 控制器句柄，失败返回 NULL
 */
ipc_bp_controller_t *ipc_bp_create(const ipc_bp_config_t *config);

/**
 * @brief 销毁背压控制器
 */
void ipc_bp_destroy(ipc_bp_controller_t *ctrl);

/**
 * @brief 更新队列深度并评估背压级别
 *
 * 每 5s 采样一次，根据队列深度计算背压级别。
 *
 * @param ctrl 控制器
 * @param current_depth 当前队列深度
 * @return 当前背压级别
 */
ipc_bp_level_t ipc_bp_update(ipc_bp_controller_t *ctrl, size_t current_depth);

/**
 * @brief 检查消息是否应被发送
 *
 * @param ctrl 控制器
 * @param is_droppable 消息是否可丢弃（日志/指标等低优先级）
 * @return true 允许发送，false 应丢弃/拒绝
 */
bool ipc_bp_should_send(ipc_bp_controller_t *ctrl, bool is_droppable);

/**
 * @brief 检查是否应接受新连接
 *
 * @param ctrl 控制器
 * @return true 接受，false 拒绝
 */
bool ipc_bp_should_accept_connection(ipc_bp_controller_t *ctrl);

/**
 * @brief 获取背压统计
 */
void ipc_bp_get_stats(ipc_bp_controller_t *ctrl, ipc_bp_stats_t *out_stats);

/**
 * @brief 获取当前背压级别
 */
ipc_bp_level_t ipc_bp_get_level(ipc_bp_controller_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_BACKPRESSURE_H */
