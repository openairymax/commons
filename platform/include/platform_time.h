/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file platform_time.h
 * @brief 时间服务：逻辑墙钟 / 时区偏移 / SNTP 校对 / 周期同步
 *
 * 解决"宿主机系统时间与所在时区标准时间不一致"的问题：
 *   1. 联网：SNTP 获取 UTC 标准时间 + 本地时区偏移 = 当前时区标准时间，
 *      以此为准（逻辑墙钟）。
 *   2. 离线：回退宿主机系统时间（offset = 0）。
 *   3. 运行时周期校对（airy_time_sync_start）：后台线程按固定间隔
 *      重新同步，保证系统时序稳定性。
 *
 * 逻辑墙钟由单调时钟驱动（校正点 base + monotonic 增量），不受用户
 * 修改系统时间影响，天然单调递增——这是"时序稳定性"的核心。
 *
 * 域名拆分（2026-08-30，任务1 时间校对）：platform.h 聚合入口，
 * 本文件独立成域，不依赖任何高层模块。
 *
 * @see platform.h aggregate entry
 */

#ifndef AIRY_RT_PLATFORM_TIME_H
#define AIRY_RT_PLATFORM_TIME_H

#include "platform_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Time service ==================== */

/**
 * @brief 当前时区相对 UTC 的偏移（秒，东正西负）。
 *
 * 基于本地时区规则（含夏令时）计算：取当前时刻 localtime 与 UTC
 * 之差，跨平台实现（不依赖 timegm/_mkgmtime）。时区配置错误时仍
 * 返回系统时区偏移——agentrt 无法修改系统时区，仅按"当前时区"
 * 校准时间值。
 *
 * @return 偏移秒数，范围 [-43200, 50400]
 */
int airy_time_tz_offset(void);

/**
 * @brief 校正后的逻辑墙钟（epoch 毫秒）。
 *
 * 单调时钟驱动：校正点（首次调用或 SNTP 成功）后由 monotonic 增量
 * 推进，不受系统时间跳变影响。联网且同步成功时等于"当前时区标准
 * 时间"，离线时等于系统时间。
 *
 * @return epoch 毫秒（逻辑时间）
 */
uint64_t airy_time_wall_ms(void);

/**
 * @brief 校正后的逻辑墙钟（epoch 秒），airy_time_wall_ms()/1000。
 * @return epoch 秒（逻辑时间）
 */
uint64_t airy_time_wall_sec(void);

/**
 * @brief SNTP 单次校对（阻塞，最多 timeout_ms）。
 *
 * 依次尝试服务器列表（AIRY_NTP_SERVERS 环境变量逗号分隔覆盖，默认
 * pool.ntp.org / ntp.aliyun.com / time.google.com）。成功后更新逻辑
 * 墙钟校正点与偏移；全部失败返回错误码，逻辑墙钟保持原状（离线
 * 模式）。
 *
 * @param timeout_ms 单服务器超时毫秒（建议 1000~3000）
 * @return AIRY_SUCCESS 校对成功；AIRY_EINVAL 参数非法；
 *         AIRY_ETIMEDOUT 全部服务器超时/不可达（离线）
 */
int airy_time_sync_once(uint32_t timeout_ms);

/**
 * @brief 启动后台周期校对线程。
 *
 * 线程立即同步一次（2s 超时），之后每 interval_sec 秒校对一次；
 * 连续失败按 2x 指数退避，上限 86400s（24h），成功后恢复标准间隔。
 * 重复调用无效（幂等）。
 *
 * @param interval_sec 校对间隔秒（0 或 <30 按 3600 处理）
 * @return AIRY_SUCCESS 启动/已在运行；AIRY_EINVAL 参数非法；
 *         AIRY_ENOMEM 线程创建失败
 */
int airy_time_sync_start(uint32_t interval_sec);

/**
 * @brief 停止后台周期校对线程（幂等，等待线程退出）。
 */
void airy_time_sync_stop(void);

/**
 * @brief 查询是否已与网络时间成功同步（离线回退则返回 0）。
 * @return 1 已同步，0 未同步（离线）
 */
int airy_time_sync_ready(void);

/**
 * @brief 最近一次成功同步的网络-系统偏移（毫秒）。
 *
 * 偏移 = 网络标准本地时间 - 系统时间。离线未同步时为 0。
 *
 * @return 偏移毫秒（可为负）
 */
int64_t airy_time_sync_delta(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_TIME_H */
