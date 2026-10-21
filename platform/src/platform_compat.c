// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_compat.c
 * @brief 平台扩展实现（airy_dl_* / airy_sock_* / 进程 / 线程别名等）。
 *
 * IRON-6 归位：实现从 daemons/common/src/util/ 迁入
 * commons/platform，声明见 platform/include/daemon_platform_ext.h。
 */

/* _GNU_SOURCE: defined via CMakeLists.txt target_compile_definitions */
#include "atomic_compat.h"
#include "daemon_platform_ext.h"
#include "airy_memory.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "error.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <errno.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <poll.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#endif

#include <cjson/cJSON.h>

#ifndef AIRY_TIMESTAMP_T_DEFINED
typedef uint64_t airy_timestamp_t;
#endif

#ifndef AIRY_DL_T_DEFINED
typedef void *airy_dl_t;
#define AIRY_DL_T_DEFINED
#endif

int airy_time_now(airy_timestamp_t *ts)
{
    if (!ts)
        return AIRY_ERR_INVALID_PARAM;
    *ts = airy_time_ns();
    return 0;
}

int airy_time_monotonic(airy_timestamp_t *ts)
{
    return airy_time_now(ts);
}

uint64_t airy_time_to_ms(const airy_timestamp_t *ts)
{
    if (!ts)
        return 0;
    return *ts / 1000000ULL;
}

airy_dl_t airy_dl_open(const char *path)
{
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW);
#endif
}

int airy_dl_close(airy_dl_t dl)
{
#ifdef _WIN32
    return FreeLibrary(dl) ? 0 : AIRY_ERR_UNKNOWN;
#else
    return dlclose(dl);
#endif
}

void *airy_dl_sym(airy_dl_t dl, const char *symbol)
{
#ifdef _WIN32
    return (void *)GetProcAddress(dl, symbol);
#else
    return dlsym(dl, symbol);
#endif
}

const char *airy_dl_error(void)
{
#ifdef _WIN32
    /* P3-2：线程局部缓冲，多线程下错误消息互不覆盖（POSIX dlerror
     * 语义一致） */
    static AIRY_THREAD_LOCAL char buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, buf, sizeof(buf), NULL);
    return buf;
#else
    return dlerror();
#endif
}

#include <errno.h>
#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
/* macOS/BSD compatibility: SOCK_NONBLOCK / MSG_NOSIGNAL / MSG_DONTWAIT are
   Linux-specific flags; defined as 0 on non-Linux POSIX platforms; after
   socket creation, fcntl / SO_NOSIGPIPE are used instead. */
#ifndef __linux__
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#endif
#else

#ifndef _WINSOCK2API_
#include <winsock2.h>
#endif
#ifndef _WS2TCPIP_H_
#include <ws2tcpip.h>
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#endif

static atomic_int g_socket_initialized = 0;

int airy_sock_init(void)
{
    int expected = 0;
    atomic_compare_exchange_strong_explicit(&g_socket_initialized, &expected, 1,
                                            memory_order_seq_cst, memory_order_seq_cst);
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        return AIRY_ERR_IO;
#endif
    return 0;
}

void airy_sock_cleanup(void)
{
    atomic_store_explicit(&g_socket_initialized, 0, memory_order_seq_cst);
#ifdef _WIN32
    WSACleanup();
#endif
}

#if AIRY_PLATFORM_POSIX
/* 新建/接受套接字的统一调优：SOCK_NONBLOCK 在 macOS/BSD 上为空定义，
 * 且 accept() 不继承监听 fd 的非阻塞标志，故此处以 fcntl 统一补齐；
 * SO_NOSIGPIPE 存在时抑制对端关闭引发的 SIGPIPE。 */
static void airy_sock_tune(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int nosig_on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE,
               (const char *)&nosig_on, sizeof(nosig_on));
#endif
}
#endif

airy_sock_t airy_sock_create_tcp_server(const char *host, uint16_t port)
{
    if (!host)
        return AIRY_ERR_INVALID_PARAM;

#ifdef _WIN32
    SOCKET fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET)
        return AIRY_INVALID_SOCKET;
    {
        u_long mode = 1;
        ioctlsocket(fd, FIONBIO, &mode);
    }
#else
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (fd < 0)
        return AIRY_ERR_IO;
    airy_sock_tune(fd);
#endif

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt));
    /* 对外 TCP 长连接开内核 keepalive——半开连接（客户端崩溃/
     * 网络闪断）由内核探活回收，避免连接数只增不减。服务端 listen fd
     * 的 keepalive 会经 accept 继承到每个连接。 */
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&opt, sizeof(opt));
#if defined(__APPLE__) && defined(__MACH__)
    {
        /* macOS 用户态无 struct tcp_keepalive（netinet/tcp.h 中该结构体
         * 位于 _KERNEL 段，仅内核可见）。Apple TN1446 方案：TCP_KEEPALIVE
         * 选项直接收 int 秒数（等价 Linux TCP_KEEPIDLE）；探测间隔与次数
         * 自 10.2 起分别经 TCP_KEEPINTVL/TCP_KEEPCNT 提供，按存在性设置。 */
        int idle = 30; /* 空闲 30s 后开始探测 */
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, (const char *)&idle, sizeof(idle));
#ifdef TCP_KEEPINTVL
        int intvl = 10; /* 每 10s 重试 */
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (const char *)&intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
        int cnt = 3; /* 3 次失败判死 */
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (const char *)&cnt, sizeof(cnt));
#endif
    }
#elif defined(__linux__) || defined(__FreeBSD__)
    {
        int idle = 30;   /* 空闲 30s 后开始探测 */
        int intvl = 10;  /* 每 10s 重试 */
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (const char *)&idle, sizeof(idle));
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (const char *)&intvl, sizeof(intvl));
#ifdef TCP_KEEPCNT
        int cnt = 3; /* 3 次失败判死 */
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (const char *)&cnt, sizeof(cnt));
#endif
    }
#endif

    struct sockaddr_in addr;
    __builtin_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(fd);
        return AIRY_INVALID_SOCKET;
#else
        close(fd);
        return AIRY_ERR_IO;
#endif
    }

    if (listen(fd, SOMAXCONN) != 0) {
#ifdef _WIN32
        closesocket(fd);
        return AIRY_INVALID_SOCKET;
#else
        close(fd);
        return AIRY_ERR_IO;
#endif
    }

    return fd;
}

#if AIRY_PLATFORM_POSIX
airy_sock_t airy_sock_create_unix_server(const char *path)
{
    if (!path)
        return AIRY_ERR_INVALID_PARAM;

    {
        char dir_buf[256];
        size_t len = strlen(path);
        if (len >= sizeof(dir_buf))
            return AIRY_ERR_INVALID_PARAM;
        AIRY_MEMCPY(dir_buf, path, len + 1);
        char *slash = strrchr(dir_buf, '/');
        if (slash) {
            *slash = '\0';
            airy_mkdir_p(dir_buf);
        }
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0)
        return AIRY_ERR_IO;
    airy_sock_tune(fd);

    struct sockaddr_un addr;
    __builtin_memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    AIRY_STRNCPY_TERM(addr.sun_path, path, sizeof(addr.sun_path));

    unlink(path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return AIRY_ERR_IO;
    }

    if (listen(fd, SOMAXCONN) != 0) {
        close(fd);
        return AIRY_ERR_IO;
    }

    return fd;
}
#endif

airy_sock_t airy_sock_accept(airy_sock_t server_fd, uint32_t timeout_ms)
{
#ifdef _WIN32
    if (server_fd == AIRY_INVALID_SOCKET)
        return AIRY_ERR_INVALID_PARAM;

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(server_fd, &read_set);

    struct timeval tv;
    struct timeval *ptv = NULL;
    if (timeout_ms > 0) {
        tv.tv_sec = (long)(timeout_ms / 1000);
        tv.tv_usec = (long)((timeout_ms % 1000) * 1000);
        ptv = &tv;
    }

    int ret = select(0, &read_set, NULL, NULL, ptv);
    if (ret <= 0 || !FD_ISSET(server_fd, &read_set))
        return AIRY_INVALID_SOCKET;

    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    SOCKET client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd != INVALID_SOCKET) {
        u_long mode = 1;
        ioctlsocket(client_fd, FIONBIO, &mode);
    }

    return client_fd;
#else
    if (server_fd < 0)
        return AIRY_ERR_INVALID_PARAM;

    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;

    /* timeout_ms==0 means a non-blocking probe (return immediately), not an
     * infinite block: historical defect — 0 was mapped to poll(-1) infinite
     * wait; when a signal (e.g. SIGTERM) arrived exactly while the
     * event-loop thread was in accept, poll returned EINTR and then re-entered
     * infinite blocking, so the loop could never check stop_requested and the
     * daemon could not exit gracefully. */
    int timeout = timeout_ms == 0 ? 0 : (int)timeout_ms;
    int ret;
    do {
        ret = poll(&pfd, 1, timeout);
    } while (ret < 0 && errno == EINTR);

    if (ret <= 0 || !(pfd.revents & POLLIN))
        return AIRY_ERR_IO;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd >= 0)
        airy_sock_tune(client_fd);

    return client_fd;
#endif
}

ssize_t airy_sock_recv(airy_sock_t sock, void *buf, size_t len)
{
#ifdef _WIN32
    if (sock == AIRY_INVALID_SOCKET || !buf || len == 0)
        return AIRY_ERR_INVALID_PARAM;
    ssize_t ret = recv(sock, buf, (int)len, 0);
    if (ret < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
        return 0;
    return ret;
#else
    if (sock < 0 || !buf || len == 0)
        return AIRY_ERR_INVALID_PARAM;
    ssize_t ret = recv(sock, buf, len, MSG_DONTWAIT);
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return 0;
    return ret;
#endif
}

ssize_t airy_sock_send(airy_sock_t sock, const void *buf, size_t len)
{
#ifdef _WIN32
    if (sock == AIRY_INVALID_SOCKET || !buf || len == 0)
        return AIRY_ERR_INVALID_PARAM;
    ssize_t total_sent = 0;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (total_sent < (ssize_t)len) {
        ssize_t sent = send(sock, ptr + total_sent, (int)(len - total_sent), 0);
        if (sent <= 0) {
            if (total_sent > 0)
                break;
            return sent;
        }
        total_sent += sent;
    }
    return total_sent;
#else
    if (sock < 0 || !buf || len == 0)
        return AIRY_ERR_INVALID_PARAM;
    ssize_t total_sent = 0;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (total_sent < (ssize_t)len) {
        ssize_t sent = send(sock, ptr + total_sent, len - total_sent, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent <= 0) {
            if (total_sent > 0)
                break;
            return sent;
        }
        total_sent += sent;
    }
    return total_sent;
#endif
}

/* ---------------------------------------------------------------------------
 * Daemon request frame reading
 * ---------------------------------------------------------------------------
 *
 * Airymax daemons are "single-request-single-response-then-close": the
 * client sends one JSON-RPC request frame over a fresh connection and keeps
 * the socket open while waiting for the reply, so there is no EOF/close to
 * delimit the request. The historical daemon_main.h handler performed a
 * SINGLE airy_sock_recv() capped at MAX_BUFFER (64 KiB): any request that
 * grew past 64 KiB was truncated, failed the JSON parse and was rejected.
 * Multi-round tool loops hit this in practice -- llm.complete after feeding
 * back web_fetch results carries tens/hundreds of KiB of HTML, so the final
 * tool-loop round was silently killed (daily chat showed tool cards but
 * never streamed the final answer).
 *
 * airy_daemon_read_request() mirrors the client-side rpc_recv_response()
 * loop (daemon_rpc_client.c): poll in slices, recv what is available, probe
 * cJSON_Parse for a complete frame, grow the buffer up to
 * AIRY_DAEMON_REQ_MAX_CAP (8 MiB). A parseable frame ends the request; a
 * read budget (5 s) guards against stalled peers. On success the caller owns
 * the returned AIRY_MALLOC buffer (*out_len set); on failure NULL is
 * returned and *err (if non-NULL) points to a static reason string.
 */
#define AIRY_DAEMON_REQ_INITIAL_CAP (64 * 1024)
#define AIRY_DAEMON_REQ_MAX_CAP (8 * 1024 * 1024)
/* 请求读取预算默认 5s 对慢客户端/gateway 转发链过紧，可经
 * AIRY_DAEMON_REQ_TIMEOUT_MS 覆盖（1000..60000），默认保持 5s。 */
#define AIRY_DAEMON_REQ_FIRST_POLL_MS 5000
#define AIRY_DAEMON_REQ_SLICE_MS 200

static uint32_t airy_daemon_req_timeout_ms(void)
{
    const char *env = getenv("AIRY_DAEMON_REQ_TIMEOUT_MS");
    if (env && *env) {
        long v = strtol(env, NULL, 10);
        if (v >= 1000 && v <= 60000)
            return (uint32_t)v;
    }
    return AIRY_DAEMON_REQ_FIRST_POLL_MS;
}

/* 读取缓冲区按需翻倍扩容；达上限或分配失败时释放缓冲区并返回 -1
 * （*err 置静态原因串）。buf/cap 为 in-out。 */
static int airy_req_grow(char **buf, size_t *cap, size_t used,
                         const char **err)
{
    if (used < *cap - 1)
        return 0;
    size_t new_cap = *cap * 2;
    if (new_cap > AIRY_DAEMON_REQ_MAX_CAP)
        new_cap = AIRY_DAEMON_REQ_MAX_CAP;
    if (new_cap <= *cap) {
        if (err)
            *err = "Request too large";
        AIRY_FREE(*buf);
        return -1;
    }
    char *np = (char *)AIRY_REALLOC(*buf, new_cap);
    if (!np) {
        if (err)
            *err = "Out of memory";
        AIRY_FREE(*buf);
        return -1;
    }
    *buf = np;
    *cap = new_cap;
    return 0;
}

/* 统一失败出口：置静态原因串（err 可空）、释放半成品缓冲并返回 NULL，
 * 避免读帧路径上多处手写清理各自漂移。 */
static char *airy_req_fail(char *buf, const char **err, const char *msg)
{
    if (err)
        *err = msg;
    AIRY_FREE(buf);
    return NULL;
}

/* 累计缓冲是否已构成完整 JSON 帧（客户端等待应答期间不断链，无 EOF 可用）。 */
static int airy_req_ready(const char *buf)
{
    cJSON *probe = cJSON_Parse(buf);
    if (!probe)
        return 0;
    cJSON_Delete(probe);
    return 1;
}

#if AIRY_PLATFORM_POSIX
/* 首字节等待：1 可读、0 超时、-1 出错。 */
static int airy_req_first(airy_sock_t fd, int timeout_ms)
{
    struct pollfd pfd;
    pfd.fd = (int)fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr <= 0)
        return pr;
    return (pfd.revents & POLLIN) ? 1 : -1;
}

/* 单次 recv：有数据则追加并补 NUL；瞬时无数据返回 0，硬错误返回 -1。 */
static int airy_req_recv(airy_sock_t fd, char *buf, size_t cap, size_t *used)
{
    ssize_t n = airy_sock_recv(fd, buf + *used, cap - *used - 1);

    if (n < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
    if (n > 0) {
        *used += (size_t)n;
        buf[*used] = '\0';
    }
    return 0;
}

/* 读预算内的切片等待：1 可读、0 已消费本轮切片（含 EINTR）、-1 断链或出错。 */
static int airy_req_wait(airy_sock_t fd, uint32_t left, uint32_t *elapsed)
{
    uint32_t slice = left < AIRY_DAEMON_REQ_SLICE_MS ? left : AIRY_DAEMON_REQ_SLICE_MS;
    struct pollfd pfd;
    pfd.fd = (int)fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int pr = poll(&pfd, 1, (int)slice);
    if (pr < 0)
        return (errno == EINTR) ? 0 : -1;
    if (pr == 0) {
        *elapsed += slice;
        return 0;
    }
    if (!(pfd.revents & POLLIN))
        return -1;
    *elapsed += 1;
    return 1;
}
#else
/* Windows 无 poll：以 SLICE_MS 切片 select 轮询，语义对齐 POSIX 切片。 */
static int airy_req_recv(airy_sock_t fd, char *buf, size_t cap, size_t *used)
{
    ssize_t n = airy_sock_recv(fd, buf + *used, cap - *used - 1);

    if (n <= 0)
        return 0;
    *used += (size_t)n;
    buf[*used] = '\0';
    return 0;
}

static int airy_req_wait(airy_sock_t fd, uint32_t left, uint32_t *elapsed)
{
    fd_set rfds;
    struct timeval tv;
    int pr;

    (void)left;
    FD_ZERO(&rfds);
    FD_SET((SOCKET)fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = AIRY_DAEMON_REQ_SLICE_MS * 1000;
    pr = select(0, &rfds, NULL, NULL, &tv);
    if (pr < 0)
        return -1;
    /* 可读但 recv 无进展时同样计入预算，避免空转（原实现只累计超时轮）。 */
    *elapsed += (pr == 0) ? AIRY_DAEMON_REQ_SLICE_MS : 1;
    return (pr == 0) ? 0 : 1;
}
#endif

char *airy_daemon_read_request(airy_sock_t client_fd, size_t *out_len, const char **err)
{
    if (out_len)
        *out_len = 0;
    if (err)
        *err = NULL;
    if (client_fd < 0)
        return NULL;

#if AIRY_PLATFORM_POSIX
    /* 事件驱动交来的 fd 未必已有数据，airy_sock_recv 为 MSG_DONTWAIT，
     * 立即 recv 的 EAGAIN 会被误判为断链（SIGPIPE / 请求丢失竞态）。 */
    if (airy_req_first(client_fd, (int)airy_daemon_req_timeout_ms()) != 1)
        return airy_req_fail(NULL, err, "Request read timeout");
#endif

    /* 两平台共用「循环读 + JSON 完整性探测」：单次 recv 会在 64 KiB 处
     * 截断大 tool 结果，命名管道/TCP 均须按完整帧语义读取。 */
    uint32_t budget = airy_daemon_req_timeout_ms();
    uint32_t elapsed = 0;
    size_t cap = AIRY_DAEMON_REQ_INITIAL_CAP;
    size_t used = 0;
    char *buf = (char *)AIRY_MALLOC(cap);

    if (!buf)
        return airy_req_fail(NULL, err, "Out of memory");

    for (;;) {
        if (airy_req_grow(&buf, &cap, used, err) != 0)
            return NULL;
        if (airy_req_recv(client_fd, buf, cap, &used) != 0)
            return airy_req_fail(buf, err, "Request read error");
        if (used > 0 && airy_req_ready(buf))
            break;
        if (elapsed >= budget)
            return airy_req_fail(buf, err, "Request read timeout");
        if (airy_req_wait(client_fd, budget - elapsed, &elapsed) < 0)
            return airy_req_fail(buf, err, "Request read error");
    }

    *out_len = used;
    return buf;
}
