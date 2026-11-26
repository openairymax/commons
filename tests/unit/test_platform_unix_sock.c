// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_platform_unix_sock.c
 * @brief 平台 socket 端点原语单元测试（0.1.21 §33.2 #13 / N-2）。
 *
 * 覆盖 commons/platform 的两组原语：
 *   - airy_sock_conn_unix / airy_sock_bind_unix（POSIX 专有）：端点建立与
 *     往返、权限位、阻塞/超时两条连接路径、错误分流（ENOENT 保留、
 *     ENAMETOOLONG 判非法参数）、陈旧节点回收与 backlog 兜底；
 *   - airy_sock_conn_ep / airy_sock_rcv_tmo /
 *     airy_sock_recv_block / airy_sock_send_all（两平台共用）：端点语义按
 *     平台分派（POSIX AF_UNIX 路径、Windows TCP 回环 "host:port"），
 *     整包发送、阻塞读、接收超时与 EOF 三态。
 */

#include "daemon_platform_ext.h"
#include "error.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("🧪 Running %s...\n", #test_func);              \
        if (test_func() != 0) {                                \
            fprintf(stderr, "✗Test failed: %s\n", #test_func); \
            failed_tests++;                                    \
        } else {                                               \
            printf("✔PASS: %s\n", #test_func);                 \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* 测试端点：POSIX 为 /tmp 下的 AF_UNIX 路径，Windows 为 TCP 回环端点串。
 * 两端都以 port=0 由内核挑端口，避免固定端口在 CI 上撞车。 */
typedef struct {
    airy_sock_t srv;
    char ep[128];
} test_ep_t;

#ifdef _WIN32
static int ep_open(test_ep_t *e)
{
    struct sockaddr_in sa;
    int len = (int)sizeof(sa);

    if (airy_sock_init() != AIRY_SUCCESS)
        return -1;
    e->srv = airy_sock_create_tcp_server("127.0.0.1", 0);
    if (e->srv == AIRY_INVALID_SOCKET)
        return -1;
    if (getsockname(e->srv, (struct sockaddr *)&sa, &len) != 0)
        return -1;
    snprintf(e->ep, sizeof(e->ep), "127.0.0.1:%u", (unsigned)ntohs(sa.sin_port));
    return 0;
}

static void ep_close(test_ep_t *e)
{
    airy_sock_close(e->srv);
    airy_sock_cleanup();
}
#else
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static int ep_open(test_ep_t *e)
{
    snprintf(e->ep, sizeof(e->ep), "/tmp/airy-unisock-%ld.sock", (long)getpid());
    (void)unlink(e->ep);
    e->srv = airy_sock_bind_unix(e->ep, 8, 0600);
    return e->srv >= 0 ? 0 : -1;
}

static void ep_close(test_ep_t *e)
{
    airy_sock_close(e->srv);
    (void)unlink(e->ep);
}
#endif

/* 端点原语全链：conn_endpoint 连接 → accept → send_all 整包 → recv_block
 * 分片读回（对端一次性读走，校验字节与计数）。 */
static int test_endpoint_roundtrip(void)
{
    test_ep_t e;
    char out[512];
    char in[512];
    size_t got = 0;
    int i;

    TEST_ASSERT(ep_open(&e) == 0, "endpoint should open");
    for (i = 0; i < (int)sizeof(out); i++)
        out[i] = (char)('a' + i % 26);

    airy_sock_t cli = airy_sock_conn_ep(e.ep, 0);
    TEST_ASSERT(cli != AIRY_INVALID_SOCKET, "conn_endpoint should connect");
    airy_sock_t peer = accept(e.srv, NULL, NULL);
    TEST_ASSERT(peer != AIRY_INVALID_SOCKET, "accept should succeed");

    TEST_ASSERT(airy_sock_send_all(cli, out, sizeof(out)) == AIRY_SUCCESS,
                "send_all should deliver the whole frame");

    while (got < sizeof(in)) {
        ssize_t n = airy_sock_recv_block(peer, in + got, sizeof(in) - got);
        TEST_ASSERT(n > 0, "recv_block should return data");
        got += (size_t)n;
    }
    TEST_ASSERT(memcmp(in, out, sizeof(out)) == 0, "payload should be byte-exact");

    airy_sock_close(peer);
    airy_sock_close(cli);
    ep_close(&e);
    return 0;
}

/* 接收超时：无数据时 recv_block 必须在超时后返回负值，不得永久阻塞。 */
static int test_recv_timeout(void)
{
    test_ep_t e;
    char buf[8];

    TEST_ASSERT(ep_open(&e) == 0, "endpoint should open");

    airy_sock_t cli = airy_sock_conn_ep(e.ep, 0);
    TEST_ASSERT(cli != AIRY_INVALID_SOCKET, "conn_endpoint should connect");
    airy_sock_t peer = accept(e.srv, NULL, NULL);
    TEST_ASSERT(peer != AIRY_INVALID_SOCKET, "accept should succeed");

    TEST_ASSERT(airy_sock_rcv_tmo(cli, 150) == AIRY_SUCCESS,
                "recv timeout should install");
    TEST_ASSERT(airy_sock_recv_block(cli, buf, sizeof(buf)) < 0,
                "idle recv_block should time out instead of blocking");

    airy_sock_close(peer);
    airy_sock_close(cli);
    ep_close(&e);
    return 0;
}

/* EOF 三态之一：对端关闭后阻塞读返回 0（不是 -1，也不是数据）。 */
static int test_recv_eof(void)
{
    test_ep_t e;
    char byte = 'z';
    char buf[8];

    TEST_ASSERT(ep_open(&e) == 0, "endpoint should open");

    airy_sock_t cli = airy_sock_conn_ep(e.ep, 0);
    TEST_ASSERT(cli != AIRY_INVALID_SOCKET, "conn_endpoint should connect");
    airy_sock_t peer = accept(e.srv, NULL, NULL);
    TEST_ASSERT(peer != AIRY_INVALID_SOCKET, "accept should succeed");

    TEST_ASSERT(airy_sock_send_all(peer, &byte, 1) == AIRY_SUCCESS, "peer send should succeed");
    TEST_ASSERT(airy_sock_recv_block(cli, buf, sizeof(buf)) == 1, "pending byte should be read");
    TEST_ASSERT(buf[0] == byte, "read byte should match");

    airy_sock_close(peer);
    TEST_ASSERT(airy_sock_recv_block(cli, buf, sizeof(buf)) == 0, "peer close should surface EOF");

    airy_sock_close(cli);
    ep_close(&e);
    return 0;
}

/* 空端点与不可达端点：统一返回 AIRY_INVALID_SOCKET 哨兵，绝不返回半开句柄。 */
static int test_endpoint_invalid(void)
{
    TEST_ASSERT(airy_sock_conn_ep(NULL, 0) == AIRY_INVALID_SOCKET,
                "NULL endpoint should be rejected");
    TEST_ASSERT(airy_sock_conn_ep("", 0) == AIRY_INVALID_SOCKET,
                "empty endpoint should be rejected");
#ifndef _WIN32
    TEST_ASSERT(airy_sock_conn_ep("/tmp/airy-unisock-absent.sock", 0) ==
                    AIRY_INVALID_SOCKET,
                "absent endpoint should fail with the sentinel");
#endif
    return 0;
}

#ifndef _WIN32
/* 阻塞连接与权限位：节点建立即 0600，单字节往返成立。 */
static int test_bind_connect_roundtrip(void)
{
    char path[128];
    (void)snprintf(path, sizeof(path), "/tmp/airy-unisock-%ld-rt.sock", (long)getpid());
    (void)unlink(path);

    airy_sock_t srv = airy_sock_bind_unix(path, 8, 0600);
    TEST_ASSERT(srv >= 0, "bind_unix should succeed");

    struct stat st;
    TEST_ASSERT(stat(path, &st) == 0, "socket node should exist");
    TEST_ASSERT((st.st_mode & 0777) == 0600, "socket node mode should be 0600");

    airy_sock_t cli = airy_sock_conn_unix(path, 0);
    TEST_ASSERT(cli >= 0, "blocking connect should succeed");

    airy_sock_t peer = accept(srv, NULL, NULL);
    TEST_ASSERT(peer >= 0, "accept should succeed");

    const char msg = 'x';
    TEST_ASSERT(send(cli, &msg, 1, 0) == 1, "send should deliver one byte");
    char got = 0;
    TEST_ASSERT(recv(peer, &got, 1, 0) == 1, "peer should receive one byte");
    TEST_ASSERT(got == 'x', "received byte should match");

    close(peer);
    close(cli);
    close(srv);
    (void)unlink(path);
    return 0;
}

/* 非阻塞连接 + poll 路径：对已监听端点应成功，且连接可用。 */
static int test_connect_with_timeout(void)
{
    char path[128];
    (void)snprintf(path, sizeof(path), "/tmp/airy-unisock-%ld-to.sock", (long)getpid());
    (void)unlink(path);

    airy_sock_t srv = airy_sock_bind_unix(path, 8, 0);
    TEST_ASSERT(srv >= 0, "bind_unix without chmod should succeed");

    airy_sock_t cli = airy_sock_conn_unix(path, 200);
    TEST_ASSERT(cli >= 0, "timeout connect should succeed against live endpoint");

    airy_sock_t peer = accept(srv, NULL, NULL);
    TEST_ASSERT(peer >= 0, "accept should succeed after timeout connect");

    close(peer);
    close(cli);
    close(srv);
    (void)unlink(path);
    return 0;
}

/* 不可达端点：两条路径都必须失败，且 errno 不被关闭动作覆盖。 */
static int test_connect_missing_endpoint(void)
{
    char path[128];
    (void)snprintf(path, sizeof(path), "/tmp/airy-unisock-%ld-miss.sock", (long)getpid());
    (void)unlink(path);

    TEST_ASSERT(airy_sock_conn_unix(path, 0) < 0, "blocking connect should fail");
    TEST_ASSERT(errno == ENOENT, "blocking connect should keep ENOENT");

    TEST_ASSERT(airy_sock_conn_unix(path, 100) < 0, "timeout connect should fail");
    TEST_ASSERT(errno == ENOENT, "timeout connect should keep ENOENT");
    return 0;
}

/* sun_path 超长：判非法参数并置 ENAMETOOLONG，绝不静默截断。 */
static int test_path_too_long(void)
{
    char long_path[160];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[0] = '/';
    long_path[sizeof(long_path) - 1] = '\0';

    TEST_ASSERT(airy_sock_conn_unix(long_path, 0) == AIRY_ERR_INVALID_PARAM,
                "over-long connect path should be rejected");
    TEST_ASSERT(errno == ENAMETOOLONG, "over-long connect should set ENAMETOOLONG");

    TEST_ASSERT(airy_sock_bind_unix(long_path, 8, 0) == AIRY_ERR_INVALID_PARAM,
                "over-long bind path should be rejected");
    TEST_ASSERT(errno == ENAMETOOLONG, "over-long bind should set ENAMETOOLONG");
    return 0;
}

/* 空路径：两原语均判非法参数。 */
static int test_invalid_params(void)
{
    TEST_ASSERT(airy_sock_conn_unix(NULL, 0) == AIRY_ERR_INVALID_PARAM,
                "NULL connect path should be rejected");
    TEST_ASSERT(airy_sock_conn_unix("", 0) == AIRY_ERR_INVALID_PARAM,
                "empty connect path should be rejected");
    TEST_ASSERT(airy_sock_bind_unix(NULL, 8, 0) == AIRY_ERR_INVALID_PARAM,
                "NULL bind path should be rejected");
    TEST_ASSERT(airy_sock_bind_unix("", 8, 0) == AIRY_ERR_INVALID_PARAM,
                "empty bind path should be rejected");
    return 0;
}

/* 陈旧节点回收：上一次监听已关闭但路径残留时，重新绑定须成功且可连。 */
static int test_rebind_stale_node(void)
{
    char path[128];
    (void)snprintf(path, sizeof(path), "/tmp/airy-unisock-%ld-stale.sock", (long)getpid());
    (void)unlink(path);

    airy_sock_t first = airy_sock_bind_unix(path, 4, 0);
    TEST_ASSERT(first >= 0, "first bind should succeed");
    close(first);
    TEST_ASSERT(access(path, F_OK) == 0, "socket node should remain after close");

    airy_sock_t srv = airy_sock_bind_unix(path, 0, 0);
    TEST_ASSERT(srv >= 0, "rebind over stale node should succeed (backlog fallback)");

    airy_sock_t cli = airy_sock_conn_unix(path, 100);
    TEST_ASSERT(cli >= 0, "connect should succeed against rebound endpoint");

    airy_sock_t peer = accept(srv, NULL, NULL);
    TEST_ASSERT(peer >= 0, "accept should succeed after rebind");

    close(peer);
    close(cli);
    close(srv);
    (void)unlink(path);
    return 0;
}

/* 端点 inode 归属（0.1.21 N-9）：ident 读 dev/ino，owned 仅对同一节点为真。
 * 守护退出清理据此只删自己的监听点，避免误删并发接管者的新节点。 */
static int test_sock_path_ident(void)
{
    char path[128];
    char other[128];
    uint64_t dev = 0, ino = 0;

    (void)snprintf(path, sizeof(path), "/tmp/airy-sockid-%ld-a.node", (long)getpid());
    (void)snprintf(other, sizeof(other), "/tmp/airy-sockid-%ld-b.node", (long)getpid());
    (void)unlink(path);
    (void)unlink(other);

    TEST_ASSERT(airy_sock_path_ident(path, &dev, &ino) != 0, "missing node should fail");
    TEST_ASSERT(airy_sock_path_owned(path, dev, ino) == 0, "missing node is not owned");

    FILE *fa = fopen(path, "w");
    FILE *fb = fopen(other, "w");
    TEST_ASSERT(fa != NULL && fb != NULL, "temp nodes should be creatable");
    fclose(fa);
    fclose(fb);

    TEST_ASSERT(airy_sock_path_ident(path, &dev, &ino) == 0, "ident should succeed on node");
    TEST_ASSERT(airy_sock_path_owned(path, dev, ino) == 1, "same node should be owned");
    TEST_ASSERT(airy_sock_path_owned(other, dev, ino) == 0, "other node must not be owned");
    TEST_ASSERT(airy_sock_path_owned(path, dev, ino + 1) == 0, "stale inode must not be owned");

    (void)unlink(path);
    (void)unlink(other);
    TEST_ASSERT(airy_sock_path_owned(path, dev, ino) == 0, "removed node must not be owned");
    return 0;
}
#endif /* _WIN32 */

int main(void)
{
    printf("===========================================\n");
    printf("  agentrt/commons/platform 端点原语测试\n");
    printf("===========================================\n\n");

    TEST_RUN(test_endpoint_roundtrip);
    TEST_RUN(test_recv_timeout);
    TEST_RUN(test_recv_eof);
    TEST_RUN(test_endpoint_invalid);
#ifndef _WIN32
    TEST_RUN(test_bind_connect_roundtrip);
    TEST_RUN(test_connect_with_timeout);
    TEST_RUN(test_connect_missing_endpoint);
    TEST_RUN(test_path_too_long);
    TEST_RUN(test_invalid_params);
    TEST_RUN(test_rebind_stale_node);
    TEST_RUN(test_sock_path_ident);
#endif

    printf("\n===========================================\n");
    printf("  测试结果: %d 通过, %d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");

    return failed_tests > 0 ? 1 : 0;
}
