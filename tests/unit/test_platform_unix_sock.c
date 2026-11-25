// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_platform_unix_sock.c
 * @brief AF_UNIX 连接/绑定原语单元测试（0.1.21 §33.2 #13）。
 *
 * 覆盖 commons/platform 的 airy_sock_conn_unix / airy_sock_bind_unix：
 * 端点建立与往返、权限位、两条连接路径（阻塞 / 非阻塞+poll）、错误分流
 * （ENOENT 保留、ENAMETOOLONG 判非法参数）、陈旧节点回收与 backlog 兜底。
 */

#include "daemon_platform_ext.h"
#include "error.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    printf("AF_UNIX 原语在 Windows 走 TCP 回退，本用例跳过\n");
    return 0;
}
#else

#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

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

/* 用例端点名：/tmp 下按 pid 隔离，长度远小于 sun_path 上限。 */
static void sock_path(char *buf, size_t cap, const char *name)
{
    snprintf(buf, cap, "/tmp/airy-unisock-%ld-%s", (long)getpid(), name);
}

/* 端点建立 → 权限位 → 阻塞连接 → 接受 → 单字节往返。 */
static int test_bind_connect_roundtrip(void)
{
    char path[128];
    sock_path(path, sizeof(path), "rt.sock");
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
    sock_path(path, sizeof(path), "to.sock");
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
    sock_path(path, sizeof(path), "missing.sock");
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
    sock_path(path, sizeof(path), "stale.sock");
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

int main(void)
{
    printf("===========================================\n");
    printf("  agentrt/commons/platform AF_UNIX 原语测试\n");
    printf("===========================================\n\n");

    TEST_RUN(test_bind_connect_roundtrip);
    TEST_RUN(test_connect_with_timeout);
    TEST_RUN(test_connect_missing_endpoint);
    TEST_RUN(test_path_too_long);
    TEST_RUN(test_invalid_params);
    TEST_RUN(test_rebind_stale_node);

    printf("\n===========================================\n");
    printf("  测试结果: %d 通过, %d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");

    return failed_tests > 0 ? 1 : 0;
}

#endif /* _WIN32 */
