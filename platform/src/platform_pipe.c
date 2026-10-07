// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_pipe.c
 * @brief 匿名管道原语域：跨平台 pipe 创建/读写/关闭，供进程域与协作面共用。
 */

#include "platform_internal.h"

int airy_pipe_create(int fds[2])
{
#if AIRY_PLATFORM_WINDOWS
    /* _pipe handles are inheritable by default; the child end must be
     * inheritable for STARTF_USESTDHANDLES redirection. */
    return _pipe(fds, 65536, _O_BINARY) == 0 ? 0 : -1;
#else
    return pipe(fds) == 0 ? 0 : -1;
#endif
}

void airy_pipe_close(int *fd)
{
    if (fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

long airy_pipe_read(int fd, void *buf, size_t len)
{
    if (fd < 0)
        return -1;
    long n = (long)read(fd, buf, len);
    return n < 0 ? -1 : n;
}

int airy_pipe_write(int fd, const void *buf, size_t len)
{
    if (fd < 0)
        return -1;
    const char *p = (const char *)buf;
    while (len > 0) {
        long n = (long)write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}
