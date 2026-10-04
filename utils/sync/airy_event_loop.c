// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_event_loop.c
 * @brief Event loop core: platform-independent facade over the backends.
 *
 * 事件循环按职责域拆分（2026-08-27，单文件三平台分支 950 行 → 核心门面 +
 * 三后端 TU）：
 *
 *   本文件                —— 跨后端共享的核心门面（run 主循环脚手架、
 *                            stop 控制、定时器委托、创建前奏、公共 API 形状）
 *   airy_event_loop_posix.c —— POSIX 两后端共享机制（fd 注册表、就绪分发）
 *   _epoll/_win/_kqueue   —— 纯平台机制（多路复用等待、唤醒、fd 订阅、
 *                            创建/销毁），按平台互斥编译、三选一链接
 *   airy_event_timer.c    —— 平台无关定时器管理
 *
 * 平台无关策略经 airy_evloop_head_t 首成员契约触达后端实例；后端只导出
 * 机制钩子（airy_evloop_wait / dispatch / notify / fd_add 及 POSIX 的
 * tbl_init / tbl_fini），核心层不依赖任何后端结构体布局。
 */

#include "airy_event_loop.h"
#include "airy_event_loop_internal.h"

#include "airy_memory.h"
#include "svc_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

airy_event_loop_t *airy_evloop_alloc(size_t loop_size, int *max_events)
{
    if (max_events && *max_events <= 0)
        *max_events = AIRY_EVENT_LOOP_MAX_EVENTS;

    airy_event_loop_t *loop = (airy_event_loop_t *)AIRY_CALLOC(1, loop_size);
    if (!loop) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }
    return loop;
}

int airy_event_loop_add_fd(airy_event_loop_t *loop, int fd, uint32_t events,
                           airy_event_callback_t cb, void *user_data)
{
    return airy_evloop_fd_add(loop, fd, events, cb, user_data, false);
}

int airy_event_loop_add_fd_lt(airy_event_loop_t *loop, int fd, uint32_t events,
                              airy_event_callback_t cb, void *user_data)
{
    return airy_evloop_fd_add(loop, fd, events, cb, user_data, true);
}

int airy_event_loop_run(airy_event_loop_t *loop)
{
    if (!loop)
        return AIRY_ERR_INVALID_PARAM;

    airy_evloop_head_t *head = airy_evloop_head(loop);
    head->running = true;
    head->stop_requested = false;
    AIRY_LOG_INFO("Event loop started (max_events=%d)", head->max_events);

    while (!head->stop_requested) {
        int ready_n = airy_evloop_wait(loop, AIRY_EVLOOP_POLL_MS);

        airy_timer_process(&head->timers, loop);

        if (ready_n < 0) {
            AIRY_LOG_DEBUG("event loop wait failed: %d", ready_n);
            continue;
        }
        if (ready_n > 0)
            airy_evloop_dispatch(loop, ready_n);
    }

    head->running = false;
    AIRY_LOG_INFO("Event loop stopped");
    return 0;
}

void airy_event_loop_stop(airy_event_loop_t *loop)
{
    if (!loop)
        return;
    airy_event_loop_stop_async(loop);
    AIRY_LOG_DEBUG("Event loop stop requested");
}

/**
 * @brief Async-safe stop of the event loop (safe in signal handlers).
 *
 * Only sets stop_requested and issues a best-effort wakeup; no logging, no
 * locks, keeping it async-signal-safe and avoiding deadlock between a signal
 * handler and the logging lock.
 */
void airy_event_loop_stop_async(airy_event_loop_t *loop)
{
    if (!loop)
        return;
    airy_evloop_head(loop)->stop_requested = true;
    (void)airy_evloop_notify(loop);
}

int airy_event_loop_wakeup(airy_event_loop_t *loop)
{
    if (!loop)
        return AIRY_ERR_INVALID_PARAM;

    int rc = airy_evloop_notify(loop);
    if (rc == AIRY_ERR_INVALID_PARAM)
        return AIRY_ERR_INVALID_PARAM;
    if (rc < 0) {
        AIRY_ERROR(AIRY_ERR_IO, "wakeup failed");
    }
    return 0;
}

uint64_t airy_event_loop_add_timer(airy_event_loop_t *loop, uint64_t interval_ms,
                                   airy_timer_callback_t cb, void *user_data)
{
    if (!loop)
        return 0;
    return airy_timer_add(&airy_evloop_head(loop)->timers, interval_ms, cb, user_data);
}

int airy_event_loop_cancel_timer(airy_event_loop_t *loop, uint64_t timer_id)
{
    if (!loop)
        return AIRY_ERR_INVALID_PARAM;
    return airy_timer_cancel(&airy_evloop_head(loop)->timers, timer_id);
}

#ifdef __cplusplus
}
#endif
