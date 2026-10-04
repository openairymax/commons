// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_event_loop_kqueue.c
 * @brief Event loop macOS/BSD backend: kqueue multiplexing and wakeup.
 *
 * macOS / BSD kqueue 后端（纯机制域，Airymax 0.1.2 新增）：kqueue 实例的
 * 创建/销毁、fd 订阅与修改、一次 poll 的等待与就绪归一化（airy_evloop_wait）、
 * 唤醒（airy_evloop_notify）。设计对齐 epoll 后端：
 *   - fd 直接作为 kevent.ident 与 posix.fd_entries 数组索引（fd < max_events）；
 *   - 定时器沿用 process_timers 轮询模式（kevent 超时），不引入 EVFILT_TIMER，
 *     保证与 Linux/Windows 后端行为完全一致；
 *   - wakeup 用 EVFILT_USER（macOS 10.6+）：notify 内仅 kevent 一个 syscall，
 *     保持 async-signal-safe（对齐 epoll 后端 eventfd 语义）；
 *   - kqueue 天然 level-triggered；level_triggered=false 时加 EV_CLEAR
 *     获得 edge-triggered 语义（对齐 EPOLLET）。
 *
 * fd 注册表、就绪分发与 fd 计数由 POSIX 共享机制 TU
 * airy_event_loop_posix.c 提供；run 主循环脚手架与 stop 控制见核心
 * airy_event_loop.c。
 */

#include "airy_event_loop.h"
#include "airy_event_loop_internal.h"

#include "airy_memory.h"

#if !defined(_WIN32) && !defined(__linux__)

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "svc_logger.h"

/* EVFILT_USER 的独立 ident：选一个远离真实 fd 的值，避免与 fd_entries
 * 索引（[0, max_events)）冲突。 */
#define AIRY_KQ_WAKEUP_IDENT 0x40000001u

struct airy_event_loop {
    airy_evloop_posix_t posix;
    int kq;
    struct kevent *kq_events;
};

int airy_evloop_fd_add(airy_event_loop_t *loop, int fd, uint32_t events,
                       airy_event_callback_t cb, void *user_data, bool level_triggered)
{
    if (!loop || fd < 0 || !cb)
        return AIRY_ERR_INVALID_PARAM;
    if (fd >= loop->posix.head.max_events) {
        AIRY_LOG_DEBUG("fd=%d exceeds max_events=%d, cannot track callback", fd,
                       loop->posix.head.max_events);
        return AIRY_ERR_INVALID_PARAM;
    }

    struct kevent kev[2];
    int n = 0;
    if (events & AIRY_EVENT_TYPE_READ) {
        EV_SET(&kev[n++], (uintptr_t)fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    if (events & AIRY_EVENT_TYPE_WRITE) {
        EV_SET(&kev[n++], (uintptr_t)fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    if (!level_triggered) {
        /* kqueue 默认 level-triggered；非 level 模式追加 EV_CLEAR 获得
         * edge-triggered 语义（对齐 EPOLLET）。 */
        for (int i = 0; i < n; i++)
            kev[i].flags |= EV_CLEAR;
    }

    if (n > 0) {
        if (kevent(loop->kq, kev, n, NULL, 0, NULL) < 0) {
            AIRY_LOG_DEBUG("kevent ADD failed for fd=%d: %s", fd, strerror(errno));
            return AIRY_ERR_IO;
        }
    }

    loop->posix.fd_entries[fd].fd = fd;
    loop->posix.fd_entries[fd].events = events;
    loop->posix.fd_entries[fd].cb = cb;
    loop->posix.fd_entries[fd].user_data = user_data;
    loop->posix.fd_entries[fd].level_triggered = level_triggered;

    return 0;
}

airy_event_loop_t *airy_event_loop_create(int max_events)
{
    airy_event_loop_t *loop = airy_evloop_alloc(sizeof(airy_event_loop_t), &max_events);
    if (!loop)
        return NULL;

    loop->kq = kqueue();
    if (loop->kq < 0) {
        AIRY_FREE(loop);
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    loop->kq_events = (struct kevent *)AIRY_CALLOC((size_t)max_events, sizeof(struct kevent));
    if (!loop->kq_events) {
        close(loop->kq);
        AIRY_FREE(loop);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    if (airy_evloop_tbl_init(loop, max_events) != 0) {
        close(loop->kq);
        AIRY_FREE(loop->kq_events);
        AIRY_FREE(loop);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    /* wakeup 事件：EVFILT_USER 用户事件过滤器（macOS 10.6+）。 */
    struct kevent wake_kev;
    EV_SET(&wake_kev, AIRY_KQ_WAKEUP_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, NOTE_FFNOP, 0, NULL);
    if (kevent(loop->kq, &wake_kev, 1, NULL, 0, NULL) < 0) {
        close(loop->kq);
        AIRY_FREE(loop->kq_events);
        airy_evloop_tbl_fini(loop);
        AIRY_FREE(loop);
        AIRY_ERROR_NULL(AIRY_ERR_IO, "io error");
    }

    airy_timer_init(&loop->posix.head.timers);

    AIRY_LOG_DEBUG("Event loop created (kq=%d, max_events=%d)", loop->kq, max_events);
    return loop;
}

void airy_event_loop_destroy(airy_event_loop_t *loop)
{
    if (!loop)
        return;
    close(loop->kq);
    AIRY_FREE(loop->kq_events);
    airy_evloop_tbl_fini(loop);
    AIRY_FREE(loop);
}

int airy_event_loop_mod_fd(airy_event_loop_t *loop, int fd, uint32_t events)
{
    if (!loop || fd < 0 || fd >= loop->posix.head.max_events)
        return AIRY_ERR_INVALID_PARAM;

    struct kevent kev[2];
    int n = 0;
    if (events & AIRY_EVENT_TYPE_READ) {
        EV_SET(&kev[n++], (uintptr_t)fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    if (events & AIRY_EVENT_TYPE_WRITE) {
        EV_SET(&kev[n++], (uintptr_t)fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    if (n > 0 && !loop->posix.fd_entries[fd].level_triggered) {
        for (int i = 0; i < n; i++)
            kev[i].flags |= EV_CLEAR;
    }

    if (n > 0) {
        if (kevent(loop->kq, kev, n, NULL, 0, NULL) < 0) {
            AIRY_ERROR(AIRY_ERR_IO, "kevent MOD failed");
        }
    }

    loop->posix.fd_entries[fd].events = events;
    return 0;
}

void airy_event_loop_remove_fd(airy_event_loop_t *loop, int fd)
{
    if (!loop || fd < 0 || fd >= loop->posix.head.max_events)
        return;

    struct kevent kev;
    EV_SET(&kev, (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(loop->kq, &kev, 1, NULL, 0, NULL);
    EV_SET(&kev, (uintptr_t)fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(loop->kq, &kev, 1, NULL, 0, NULL);

    __builtin_memset(&loop->posix.fd_entries[fd], 0, sizeof(airy_evloop_fd_t));
}

int airy_evloop_wait(airy_event_loop_t *loop, int timeout_ms)
{
    airy_evloop_posix_t *posix = airy_evloop_posix(loop);

    struct timespec timeout = {.tv_sec = timeout_ms / 1000,
                               .tv_nsec = (long)(timeout_ms % 1000) * 1000000L};
    int nev = kevent(loop->kq, NULL, 0, loop->kq_events, posix->head.max_events, &timeout);
    if (nev < 0) {
        if (errno == EINTR)
            return 0;
        return AIRY_ERR_IO;
    }

    int ready_n = 0;
    for (int i = 0; i < nev; i++) {
        struct kevent *kev = &loop->kq_events[i];
        uintptr_t ident = (uintptr_t)kev->ident;

        if (ident == AIRY_KQ_WAKEUP_IDENT)
            continue; /* wakeup 事件：仅用于打断 kevent 阻塞 */

        int fd = (int)ident;
        uint32_t user_events = 0;
        if (kev->filter == EVFILT_READ)
            user_events |= AIRY_EVENT_TYPE_READ;
        if (kev->filter == EVFILT_WRITE)
            user_events |= AIRY_EVENT_TYPE_WRITE;

        posix->ready[ready_n].fd = fd;
        posix->ready[ready_n].events = user_events;
        ready_n++;
    }
    return ready_n;
}

int airy_evloop_notify(airy_event_loop_t *loop)
{
    if (!loop)
        return AIRY_ERR_INVALID_PARAM;
    struct kevent kev;
    EV_SET(&kev, AIRY_KQ_WAKEUP_IDENT, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    if (kevent(loop->kq, &kev, 1, NULL, 0, NULL) < 0)
        return AIRY_ERR_IO;
    return 0;
}

#else

/* 本 TU 仅在 macOS/BSD 参与编译：后端选择见 airy_event_loop_epoll.c /
 * airy_event_loop_win.c。 */
typedef int airy_event_loop_kqueue_not_selected_t;

#endif /* !defined(_WIN32) && !defined(__linux__) */
