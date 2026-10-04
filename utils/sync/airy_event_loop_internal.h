// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_event_loop_internal.h
 * @brief Internal contract between the event loop core and platform backends.
 *
 * airy_event_loop.c 自 2026-08-27 起按职责域拆分：
 *
 *   airy_event_loop.c         事件循环核心（平台无关门面 + run 策略）
 *   airy_event_loop_posix.c   POSIX 共享机制（注册表 / 就绪分发 / fd 计数）
 *   airy_event_loop_epoll.c   Linux epoll 后端（IO 多路复用 + wait/notify）
 *   airy_event_loop_win.c     Windows WSAEventSelect 后端
 *   airy_event_loop_kqueue.c  macOS/BSD kqueue 后端
 *   airy_event_timer.c        平台无关定时器管理（更早拆出）
 *
 * 事件循环家族按「机制 / 策略」划分：平台无关的策略（默认容量钳制、
 * 公共 API 形状、run 主循环脚手架、定时器委托、stop 控制）收敛于核心 TU
 * airy_event_loop.c；POSIX 两后端共享的机制（fd 注册表、就绪事件分发、
 * fd 计数）收敛于 airy_event_loop_posix.c；纯平台相关的机制（多路复用
 * 等待、唤醒、fd 订阅、创建/销毁）由三后端各自实现并以下述钩子导出。
 *
 * 三后端 struct 均以 airy_evloop_head_t 为首成员（C11 6.7.2.1p15），实例
 * 地址与 airy_event_loop_t* 等同，核心层据此触达策略域而不依赖后端布局；
 * epoll / kqueue 后端进一步以 airy_evloop_posix_t 为首成员。
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

/** 后端 poll 轮询间隔（毫秒），三平台统一。 */
#define AIRY_EVLOOP_POLL_MS 100

/** 三平台后端实例的公共首成员（策略域）。 */
typedef struct {
    int max_events;
    airy_timer_state_t timers;
    volatile bool running;
    volatile bool stop_requested;
} airy_evloop_head_t;

/** POSIX 后端（epoll / kqueue）共享的 fd 注册项。 */
typedef struct {
    int fd;
    uint32_t events;
    airy_event_callback_t cb;
    void *user_data;
    bool level_triggered;
} airy_evloop_fd_t;

/** 后端 wait 阶段归一化的就绪事件。 */
typedef struct {
    int fd;
    uint32_t events;
} airy_evloop_ev_t;

/**
 * @brief POSIX 后端（epoll / kqueue）的公共前缀。
 *
 * head 必须为 struct 首成员；fd_entries 为 fd 注册表（按 fd 编号索引），
 * ready 为 wait 归一化后的就绪缓冲，二者均由核心侧公共实现
 * （airy_evloop_tbl_init / _fini / airy_evloop_dispatch）统一管理。
 */
typedef struct {
    airy_evloop_head_t head;
    airy_evloop_fd_t *fd_entries;
    airy_evloop_ev_t *ready;
} airy_evloop_posix_t;

/** 触达后端实例的策略域首成员（地址等同，C11 6.7.2.1p15）。 */
static inline airy_evloop_head_t *airy_evloop_head(airy_event_loop_t *loop)
{
    return (airy_evloop_head_t *)loop;
}

#if !defined(_WIN32)
/** 触达 POSIX 后端实例的公共前缀（地址等同）。 */
static inline airy_evloop_posix_t *airy_evloop_posix(airy_event_loop_t *loop)
{
    return (airy_evloop_posix_t *)loop;
}
#endif

/* --- 后端机制钩子（三选一由当前参与链接的后端实现） --------------------- */

/**
 * @brief 等待一次多路复用事件，把就绪结果归一化到后端就绪缓冲。
 *
 * 返回就绪事件数（>=0）；被信号中断返回 0（静默）；真实错误返回负值。
 * 核心 run 主循环对负值采取容忍语义（记 debug 日志后继续），不中止循环。
 */
int airy_evloop_wait(airy_event_loop_t *loop, int timeout_ms);

/**
 * @brief 分发本轮就绪事件（调用各 fd 注册回调）。
 *
 * POSIX 由 airy_event_loop_posix.c 实现（遍历 ready 缓冲）；win 后端自行
 * 实现（从其 wait 记录的起始索引处理）。ready_n 为 wait 返回的就绪数。
 */
void airy_evloop_dispatch(airy_event_loop_t *loop, int ready_n);

/**
 * @brief 唤醒阻塞中的轮询（async-safe，静默）。
 *
 * 返回 0 成功；loop 无效返回 AIRY_ERR_INVALID_PARAM；唤醒原语失败返回
 * AIRY_ERR_IO。供核心的 stop_async / wakeup 入口复用。
 */
int airy_evloop_notify(airy_event_loop_t *loop);

#if !defined(_WIN32)
/**
 * @brief 初始化 POSIX 共享注册表与就绪缓冲（容量 max_events）。
 *
 * 由 epoll / kqueue 后端的 create 在实例分配后调用；任一分派失败时内部
 * 先 fini 再返回非 0，调用方据此走销毁路径。
 */
int airy_evloop_tbl_init(airy_event_loop_t *loop, int max_events);

/** 释放 POSIX 共享注册表与就绪缓冲。 */
void airy_evloop_tbl_fini(airy_event_loop_t *loop);
#endif

/* --- 核心侧（airy_event_loop.c）导出的内部契约 ------------------------- */

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
