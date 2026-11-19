// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file hall_evt_gseq.h
 * @brief hall 事件全局序号（gseq）编号机制——写侧内部件，非公共 ABI。
 *
 * 与 hall_event.h 的分工：hall_event.c 负责磁盘格式与落盘，本件只负责
 * 「下一个全局序号是多少」。两者共同构成写侧 SSoT，对外只暴露
 * hall_evt_commit 一个入口，外部不应直接调用本件。
 */

#ifndef AIRY_RT_HALL_EVT_GSEQ_H
#define AIRY_RT_HALL_EVT_GSEQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 解析 p 处的前导十进制数（sscanf 禁用）。
 * @return 0 成功；-1 首字符非数字
 */
int hall_atou(const char *p, uint64_t *out);

/**
 * @brief 登记默认事件根，启用该根的进程内区间缓存（初始化期调用一次）。
 */
void hall_gseq_init(const char *root);

/**
 * @brief 在指定根上分配下一个全局序号。
 *
 * 跨进程安全：同一根上的写者通过 <root>/.gseq 水位线与 <root>/.gseq.lck
 * 排他锁串行化预留，任意两个写者拿到的 gseq 必不相同。调用方须持本模块
 * 的写锁（hall_event.c 的 g_evt_lock）以保证进程内串行。
 */
uint64_t hall_gseq_next(const char *root);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_HALL_EVT_GSEQ_H */
