// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_event_loop_internal.h
 * @brief Internal contract between the event loop core and platform backends.
 *
 * airy_event_loop.c 自 2026-08-27 起按职责域拆分：
 *
 *   airy_event_loop.c         事件循环核心（平台无关门面：stop / 定时器委托）
 *   airy_event_loop_epoll.c   Linux epoll 后端（IO 多路复用 + 回调分发）
 *   airy_event_loop_win.c     Windows WSAEventSelect 后端
 *   airy_event_loop_kqueue.c  macOS/BSD kqueue 后端
 *   airy_event_timer.c        平台无关定时器管理（更早拆出）
 *
 * struct airy_event_loop 的布局由各后端私有定义（字段随多路复用机制而异，
 * 且同一平台仅有一个后端参与链接），核心层不依赖其布局；跨编译单元共享的
 * 内部符号统一经本头文件以原名 extern 导出。
 *
 * 事件循环家族按「机制 / 策略」划分：平台无关的策略（默认容量钳制、
 * 公共 API 形状、定时器委托）收敛于核心 TU airy_event_loop.c；平台相关的
 * 机制（fd 注册、多路复用、唤醒）由三后端各自实现并以下述钩子导出。
 *
 * NOT part of the public API.
 */

#ifndef AIRY_EVENT_LOOP_INTERNAL_H
#define AIRY_EVENT_LOOP_INTERNAL_H

#include "airy_event_loop.h"
#include "airy_event_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Accessor to the timer subsystem owned by a backend loop instance.
 *
 * 由当前参与链接的平台后端提供（epoll / win / kqueue 三选一）；事件循环
 * 核心的定时器委托入口（airy_event_loop_add_timer / cancel_timer）经此
 * 触达 timers 域，避免核心层依赖任何后端的结构体布局。
 */
airy_timer_state_t *airy_event_loop_timers(airy_event_loop_t *loop);

/**
 * @brief Allocate a backend loop instance with the standard capacity policy.
 *
 * 平台无关的创建前奏：先将 max_events 钳制到合法容量（<=0 回落到
 * AIRY_EVENT_LOOP_MAX_EVENTS，经 max_events 指针原位回写），再以
 * loop_size（由后端以 sizeof(airy_event_loop_t) 传入，核心层不依赖
 * 结构体布局）零初始化分配实例。分配失败经 AIRY_ERROR_NULL 上报并返回
 * NULL，调用方据此提前返回。
 */
airy_event_loop_t *airy_evloop_alloc(size_t loop_size, int *max_events);

/**
 * @brief Backend hook implementing fd registration (edge / level triggered).
 *
 * 由当前参与链接的平台后端提供（epoll / win / kqueue 三选一），承载各
 * 多路复用机制的实际订阅工作；核心 TU 的公共入口
 * airy_event_loop_add_fd / airy_event_loop_add_fd_lt 仅分别以
 * level_triggered=false / true 转发本钩子，避免该公共形状在三后端重复。
 */
int airy_evloop_fd_add(airy_event_loop_t *loop, int fd, uint32_t events,
                       airy_event_callback_t cb, void *user_data, bool level_triggered);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_EVENT_LOOP_INTERNAL_H */
