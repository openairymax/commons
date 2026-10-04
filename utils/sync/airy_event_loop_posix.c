// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_event_loop_posix.c
 * @brief Event loop POSIX shared mechanism: fd table, ready dispatch, fd count.
 *
 * epoll（Linux）与 kqueue（macOS/BSD）两后端共享的平台无关机制：
 *   - fd 注册表与就绪缓冲的分配/释放（最大容量 max_events）；
 *   - 就绪事件回调分发（wait 归一化结果 → 用户回调）；
 *   - fd 计数（遍历注册表统计已登记 fd）。
 *
 * 因该机制不被 Windows 后端共享，本 TU 在 _WIN32 下为空（保留锚点）。
 * 纯平台机制（多路复用等待、唤醒、fd 订阅、创建/销毁）见
 * airy_event_loop_epoll.c / airy_event_loop_kqueue.c。
 */

#include "airy_event_loop.h"
#include "airy_event_loop_internal.h"

#include "airy_memory.h"

#if !defined(_WIN32)

#include "svc_logger.h"

int airy_evloop_tbl_init(airy_event_loop_t *loop, int max_events)
{
    airy_evloop_posix_t *posix = airy_evloop_posix(loop);

    posix->head.max_events = max_events;
    posix->fd_entries =
        (airy_evloop_fd_t *)AIRY_CALLOC((size_t)max_events, sizeof(airy_evloop_fd_t));
    posix->ready =
        (airy_evloop_ev_t *)AIRY_CALLOC((size_t)max_events, sizeof(airy_evloop_ev_t));

    if (!posix->fd_entries || !posix->ready) {
        airy_evloop_tbl_fini(loop);
        return AIRY_ERR_INVALID_PARAM;
    }
    return 0;
}

void airy_evloop_tbl_fini(airy_event_loop_t *loop)
{
    airy_evloop_posix_t *posix = airy_evloop_posix(loop);

    AIRY_FREE(posix->fd_entries);
    AIRY_FREE(posix->ready);
    posix->fd_entries = NULL;
    posix->ready = NULL;
}

void airy_evloop_dispatch(airy_event_loop_t *loop, int ready_n)
{
    airy_evloop_posix_t *posix = airy_evloop_posix(loop);
    int max_events = posix->head.max_events;

    for (int i = 0; i < ready_n; i++) {
        int fd = posix->ready[i].fd;
        uint32_t events = posix->ready[i].events;

        if (fd >= 0 && fd < max_events && posix->fd_entries[fd].cb) {
            posix->fd_entries[fd].cb(fd, events, posix->fd_entries[fd].user_data);
        } else if (fd >= max_events) {
            AIRY_LOG_DEBUG("event for fd=%d >= max_events=%d, dropping", fd, max_events);
        }
    }
}

int airy_event_loop_get_fd_count(airy_event_loop_t *loop)
{
    if (!loop)
        return 0;

    airy_evloop_posix_t *posix = airy_evloop_posix(loop);
    int count = 0;
    for (int i = 0; i < posix->head.max_events; i++) {
        if (posix->fd_entries[i].fd > 0)
            count++;
    }
    return count;
}

#else

/* 本 TU 仅在 POSIX 平台参与编译：Windows 后端的对应机制见
 * airy_event_loop_win.c。保留一个类型锚点防止 ISO 对空翻译单元的诊断。 */
typedef int airy_event_loop_posix_not_selected_t;

#endif /* !defined(_WIN32) */
