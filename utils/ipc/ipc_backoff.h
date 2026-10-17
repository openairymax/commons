/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file ipc_backoff.h
 * @brief IPC 重连退避与失败窗口聚合原语（0.1.19 §3.6 D7）。
 *
 * 两个纯算术机制件，无日志/传输/全局状态耦合，供任意 IPC 消费方内嵌：
 *   - ipc_bkoff_*：指数退避（base 起倍增、cap 封顶），消除重连高频抖动；
 *   - ipc_agg_*：同类失败按固定窗口聚合，窗口内只放行首条并在滚动时回传
 *     上一窗口累计次数，替代重连日志洪水（基线 481 次/2h）。
 *
 * 定级策略（首条 INFO / 窗口内抑制 / 全路径失败 ERROR）由调用点决定，
 * 本件只裁决「是否放行」，贯彻机制与策略分离。
 */

#ifndef AIRY_RT_IPC_BACKOFF_H
#define AIRY_RT_IPC_BACKOFF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 指数退避状态：cur_ms 为当前档位延迟。 */
typedef struct {
    uint32_t base_ms;
    uint32_t cap_ms;
    uint32_t cur_ms;
} ipc_bkoff_t;

/* 初始化退避；base_ms/cap_ms 为 0 时取默认（200ms / 5000ms），并保证 base<=cap。 */
void ipc_bkoff_init(ipc_bkoff_t *b, uint32_t base_ms, uint32_t cap_ms);

/* 返回本次应等待毫秒数，并推进到下一档（base, 2*base, ... 封顶 cap）。 */
uint32_t ipc_bkoff_next(ipc_bkoff_t *b);

/* 连接恢复后复位到 base。 */
void ipc_bkoff_reset(ipc_bkoff_t *b);

/* 失败窗口聚合状态：win_sec 窗口宽度（秒），win_start 当前窗口起点，count 累计。 */
typedef struct {
    uint32_t win_sec;
    uint64_t win_start;
    uint64_t count;
} ipc_agg_t;

/* 初始化聚合窗口；win_sec 为 0 时取默认（60s）。 */
void ipc_agg_init(ipc_agg_t *a, uint32_t win_sec);

/* 喂入一次失败（now_sec 为墙钟秒）。返回 true 表示该条为窗口首条、应记录，
 * false 表示窗口内重复、应抑制。进入新窗口时 *prev_count 回传上一窗口累计
 * 失败数（无上一窗口或未滚动则为 0），供首条附带上窗计数。 */
bool ipc_agg_feed(ipc_agg_t *a, uint64_t now_sec, uint64_t *prev_count);

/* 连接恢复后复位窗口，下次失败重新计为首条。 */
void ipc_agg_reset(ipc_agg_t *a);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_BACKOFF_H */
