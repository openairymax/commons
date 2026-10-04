// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_event_loop_epoll.c
 * @brief Event loop Linux backend: epoll multiplexing and wakeup.
 *
 * Linux 平台后端（纯机制域）：epoll 实例的创建/销毁、fd 订阅与修改、
 * 一次 poll 的等待与就绪归一化（airy_evloop_wait）、唤醒（airy_evloop_notify）。
 *
 * fd 直接作为 posix.fd_entries 数组索引（fd < max_events）；wakeup 用 eventfd
 * （EFD_NONBLOCK），notify 内仅 write 一个 syscall，保持 async-signal-safe。
 *
 * fd 注册表、就绪分发与 fd 计数由 POSIX 共享机制 TU
 * airy_event_loop_posix.c 提供；run 主循环脚手架与 stop 控制见核心
 * airy_event_loop.c。
 */

#include "airy_event_loop.h"
#include "airy_event_loop_internal.h"

#include "airy_memory.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && defined(__linux__)

#include <sys/epoll.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/eventfd.h>
#endif

#include "svc_logger.h"

struct airy_event_loop {
    airy_evloop_posix_t posix;
    int epoll_fd;
    int wakeup_fd;
    struct epoll_event *epoll_events;
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

    struct epoll_event ev;
    __builtin_memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;

    if (events & AIRY_EVENT_TYPE_READ)
        ev.events |= EPOLLIN;
    if (events & AIRY_EVENT_TYPE_WRITE)
        ev.events |= EPOLLOUT;
    if (!level_triggered)
        ev.events |= EPOLLET;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        if (errno == EEXIST) {
            if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                AIRY_LOG_DEBUG("epoll_ctl MOD failed for fd=%d: %s", fd, strerror(errno));
                return AIRY_ERR_IO;
            }
        } else {
            AIRY_LOG_DEBUG("epoll_ctl ADD failed for fd=%d: %s", fd, strerror(errno));
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

    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        AIRY_FREE(loop);
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    loop->epoll_events =
        (struct epoll_event *)AIRY_CALLOC((size_t)max_events, sizeof(struct epoll_event));
    if (!loop->epoll_events) {
        close(loop->epoll_fd);
        AIRY_FREE(loop);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    if (airy_evloop_tbl_init(loop, max_events) != 0) {
        close(loop->epoll_fd);
        AIRY_FREE(loop->epoll_events);
        AIRY_FREE(loop);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    loop->wakeup_fd = -1;
#ifdef __linux__
    loop->wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (loop->wakeup_fd >= 0) {
        struct epoll_event ev;
        __builtin_memset(&ev, 0, sizeof(ev));
        ev.data.fd = loop->wakeup_fd;
        ev.events = EPOLLIN;
        if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->wakeup_fd, &ev) < 0) {
            close(loop->wakeup_fd);
            loop->wakeup_fd = -1;
        }
    }
#endif

    airy_timer_init(&loop->posix.head.timers);

    AIRY_LOG_DEBUG("Event loop created (epoll_fd=%d, wakeup_fd=%d, max_events=%d)", loop->epoll_fd,
                   loop->wakeup_fd, max_events);
    return loop;
}

void airy_event_loop_destroy(airy_event_loop_t *loop)
{
    if (!loop)
        return;
    if (loop->wakeup_fd >= 0)
        close(loop->wakeup_fd);
    close(loop->epoll_fd);
    AIRY_FREE(loop->epoll_events);
    airy_evloop_tbl_fini(loop);
    AIRY_FREE(loop);
}

int airy_event_loop_mod_fd(airy_event_loop_t *loop, int fd, uint32_t events)
{
    if (!loop || fd < 0 || fd >= loop->posix.head.max_events)
        return AIRY_ERR_INVALID_PARAM;

    struct epoll_event ev;
    __builtin_memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;

    if (events & AIRY_EVENT_TYPE_READ)
        ev.events |= EPOLLIN;
    if (events & AIRY_EVENT_TYPE_WRITE)
        ev.events |= EPOLLOUT;

    if (!loop->posix.fd_entries[fd].level_triggered)
        ev.events |= EPOLLET;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        AIRY_ERROR(AIRY_ERR_IO, "epoll_ctl MOD failed");
    }

    loop->posix.fd_entries[fd].events = events;
    return 0;
}

void airy_event_loop_remove_fd(airy_event_loop_t *loop, int fd)
{
    if (!loop || fd < 0 || fd >= loop->posix.head.max_events)
        return;
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    __builtin_memset(&loop->posix.fd_entries[fd], 0, sizeof(airy_evloop_fd_t));
}

int airy_evloop_wait(airy_event_loop_t *loop, int timeout_ms)
{
    airy_evloop_posix_t *posix = airy_evloop_posix(loop);

    int nfds = epoll_wait(loop->epoll_fd, loop->epoll_events, posix->head.max_events, timeout_ms);
    if (nfds <= 0)
        return nfds;

    int ready_n = 0;
    for (int i = 0; i < nfds; i++) {
        int fd = loop->epoll_events[i].data.fd;
        uint32_t revents = loop->epoll_events[i].events;

        if (loop->wakeup_fd >= 0 && fd == loop->wakeup_fd) {
            uint64_t val;
            while (read(loop->wakeup_fd, &val, sizeof(val)) > 0) {
            }
            continue;
        }

        uint32_t user_events = 0;
        if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR))
            user_events |= AIRY_EVENT_TYPE_READ;
        if (revents & EPOLLOUT)
            user_events |= AIRY_EVENT_TYPE_WRITE;

        posix->ready[ready_n].fd = fd;
        posix->ready[ready_n].events = user_events;
        ready_n++;
    }
    return ready_n;
}

int airy_evloop_notify(airy_event_loop_t *loop)
{
    if (!loop || loop->wakeup_fd < 0)
        return AIRY_ERR_INVALID_PARAM;
    uint64_t val = 1;
    if (write(loop->wakeup_fd, &val, sizeof(val)) < 0)
        return AIRY_ERR_IO;
    return 0;
}

#else

/* 本 TU 在非 Linux 平台为空：后端选择见 airy_event_loop_win.c /
 * airy_event_loop_kqueue.c。保留一个外部引用锚点防止 ISO 对空翻译单元
 * 的诊断。 */
extern int airy_event_loop_epoll_not_selected;

#endif /* !defined(_WIN32) && defined(__linux__) */
