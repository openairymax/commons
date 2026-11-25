// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_sock_conn.c
 * @brief 出站连接与阻塞整包收发原语（A-IPC 客户端共用的唯一平台出口）。
 *
 * 上层只描述端点：POSIX 上是 AF_UNIX 路径，Windows 上是 "host:port" 的
 * TCP 回环端点（daemon_main 的既成约定）。端点解析、连接、接收超时与
 * 整包发送只在本文件出现一次，gateway 与 daemons 的 RPC 客户端不再各自
 * 手写 socket 仪式，换平台时改动面只有这一处。
 *
 * 与 airy_sock_recv/airy_sock_send 的分工：那两个是「一次系统调用」的
 * 非阻塞原语（服务端事件循环按就绪掩码驱动）；本文件面向单请求单响应
 * 的阻塞客户端，提供 AIRY_INVALID_SOCKET 哨兵与整包语义。
 */

#include "daemon_platform_ext.h"
#include "airy_memory.h"
#include "error.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if AIRY_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#define SOCK_CONN_HOST_MAX 128
#define SOCK_CONN_PORT_MAX 8

#if AIRY_PLATFORM_WINDOWS

/* 端点 "host:port"：解析只在此处，主机名交给 getaddrinfo，连接失败逐个
 * 候选地址尝试。回环端点的拒绝是即时的，故不做非阻塞超时连接。 */
static airy_sock_t sock_conn_tcp(const char *endpoint)
{
    char host[SOCK_CONN_HOST_MAX];
    char port_str[SOCK_CONN_PORT_MAX];
    const char *colon = endpoint ? strrchr(endpoint, ':') : NULL;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    airy_sock_t fd = AIRY_INVALID_SOCKET;
    size_t hlen;

    if (!colon || colon == endpoint || strlen(colon + 1) >= sizeof(port_str))
        return AIRY_INVALID_SOCKET;
    hlen = (size_t)(colon - endpoint);
    if (hlen >= sizeof(host))
        return AIRY_INVALID_SOCKET;
    AIRY_MEMCPY(host, endpoint, hlen);
    host[hlen] = '\0';
    snprintf(port_str, sizeof(port_str), "%s", colon + 1);

    AIRY_MEMSET(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return AIRY_INVALID_SOCKET;

    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        airy_sock_t s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == AIRY_INVALID_SOCKET)
            continue;
        if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) == 0) {
            fd = s;
            break;
        }
        closesocket(s);
    }
    freeaddrinfo(res);
    return fd;
}

#endif

airy_sock_t airy_sock_conn_ep(const char *endpoint, uint32_t timeout_ms)
{
    if (!endpoint || !endpoint[0])
        return AIRY_INVALID_SOCKET;
#if AIRY_PLATFORM_WINDOWS
    (void)timeout_ms;
    return sock_conn_tcp(endpoint);
#else
    {
        airy_sock_t fd = airy_sock_conn_unix(endpoint, timeout_ms);
        return ((int)fd < 0) ? AIRY_INVALID_SOCKET : fd;
    }
#endif
}

int airy_sock_rcv_tmo(airy_sock_t sock, uint32_t ms)
{
    if (sock == AIRY_INVALID_SOCKET)
        return AIRY_ERR_INVALID_PARAM;
#if AIRY_PLATFORM_WINDOWS
    {
        DWORD tv = (DWORD)ms;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) != 0)
            return AIRY_ERR_IO;
    }
#else
    {
        struct timeval tv;
        tv.tv_sec = (time_t)(ms / 1000u);
        tv.tv_usec = (suseconds_t)((ms % 1000u) * 1000u);
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
            return AIRY_ERR_IO;
    }
#endif
    return AIRY_SUCCESS;
}

ssize_t airy_sock_recv_block(airy_sock_t sock, void *buf, size_t len)
{
    if (sock == AIRY_INVALID_SOCKET || !buf || len == 0)
        return -1;
    for (;;) {
#if AIRY_PLATFORM_WINDOWS
        int n = recv(sock, (char *)buf, (int)(len > 0x7fffffffu ? 0x7fffffffu : len), 0);
        return (ssize_t)n;
#else
        ssize_t n = recv(sock, buf, len, 0);
        if (n < 0 && errno == EINTR)
            continue;
        return n;
#endif
    }
}

int airy_sock_send_all(airy_sock_t sock, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;

    if (sock == AIRY_INVALID_SOCKET || (!buf && len > 0))
        return AIRY_ERR_INVALID_PARAM;
    while (sent < len) {
        ssize_t n;
#if AIRY_PLATFORM_WINDOWS
        size_t rest = len - sent;
        n = (ssize_t)send(sock, (const char *)p + sent,
                          (int)(rest > 0x7fffffffu ? 0x7fffffffu : rest), 0);
#else
        n = send(sock, p + sent, len - sent, 0);
#endif
        if (n < 0) {
#if !AIRY_PLATFORM_WINDOWS
            if (errno == EINTR)
                continue;
#endif
            return AIRY_ERR_IO;
        }
        if (n == 0)
            return AIRY_ERR_IO;
        sent += (size_t)n;
    }
    return AIRY_SUCCESS;
}
