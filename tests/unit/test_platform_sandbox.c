// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_platform_sandbox.c
 * @brief platform_sandbox 单元测试（机制 SSoT 唯一测试，§204 自 cupolas 迁入）
 *
 * 覆盖：配置契约（init 清零 / global_read 默认 / NULL 与 disabled 零开销
 * no-op）与真实 Landlock/seccomp 行为（fork 子进程 exec、rw_paths 写
 * 放行 + 外部写拒绝、deny_network 下 socket EPERM）。内核不支持时按
 * 平台语义 SKIP，非 Linux 平台真实行为测试整体 SKIP。
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#ifdef __linux__
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "FAIL: %s\n", message);  \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("Running %s...\n", #test_func);                 \
        if (test_func() != 0) {                                \
            fprintf(stderr, "Test failed: %s\n", #test_func); \
            failed_tests++;                                    \
        } else {                                               \
            printf("PASS: %s\n", #test_func);                  \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

static int test_sandbox_init(void)
{
    airy_native_sandbox_t sb;
    airy_native_sandbox_init(&sb);
    TEST_ASSERT(sb.enabled == 0, "init clears enabled");
    TEST_ASSERT(sb.deny_network == 0, "init clears deny_network");
    TEST_ASSERT(sb.global_read == 1, "init defaults global_read to 1");
    TEST_ASSERT(sb.ro_paths == NULL && sb.rw_paths == NULL,
                "init clears path lists");
    airy_native_sandbox_init(NULL); /* NULL 安全 */
    return 0;
}

static int test_sandbox_noop(void)
{
    airy_native_sandbox_t sb;
    airy_native_sandbox_init(&sb);
    TEST_ASSERT(airy_native_sandbox_apply(&sb) == 0,
                "disabled sandbox apply must be a no-op");
    return 0;
}

static int test_sandbox_null(void)
{
    TEST_ASSERT(airy_native_sandbox_apply(NULL) == 0, "NULL sandbox must be a no-op");
    TEST_ASSERT(airy_native_sandbox_landlock_available() == 0 ||
                    airy_native_sandbox_landlock_available() == 1,
                "landlock_available must return a boolean");
    return 0;
}

#if defined(__linux__)

/* 子进程内应用沙箱并执行命令；返回子进程退出码（-1 = fork/wait 失败）。 */
static int run_in_sandbox(const airy_native_sandbox_t *sb, char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        if (airy_native_sandbox_apply(sb) != 0)
            _exit(126); /* 沙箱应用失败（如内核不支持 Landlock） */
        execvp(argv[0], argv);
        _exit(127); /* exec 失败 */
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -2; /* signaled */
}

/* 子进程内：验证 rw_paths 可写（目录 + 非目录文件）+ 外部只读路径写被拒。
 * 退出码：0 = 断言全部成立；其他 = 失败类型。 */
static void probe_write(const char *rw_file, const char *rw_node,
                        const char *outside_file)
{
    int fd = open(rw_file, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0)
        _exit(21); /* rw_paths 内写入被拒 */
    close(fd);

    fd = open(rw_node, O_WRONLY);
    if (fd < 0)
        _exit(23); /* 非目录 rw 路径写入被拒（目录专属位未正确裁剪） */
    close(fd);

    fd = open(outside_file, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd >= 0) {
        close(fd);
        unlink(outside_file);
        _exit(22); /* 外部路径本应被 Landlock 拒绝 */
    }
    _exit(0);
}

static int test_sandbox_enabled_exec(void)
{
    char *const argv[] = {"/bin/echo", "sandbox-ok", NULL};
    const char *rw_paths[] = {"/tmp", NULL};
    airy_native_sandbox_t sb;
    airy_native_sandbox_init(&sb);
    sb.enabled = 1;
    sb.rw_paths = rw_paths;

    int code = run_in_sandbox(&sb, argv);
    if (code == 126) {
        printf("SKIP: enabled exec (Landlock unavailable)\n");
        return 0;
    }
    TEST_ASSERT(code == 0, "echo succeeds inside sandbox");
    return 0;
}

static int test_sandbox_write_restriction(void)
{
    char rw_dir[] = "/tmp/sandbox_rw_XXXXXX";
    if (mkdtemp(rw_dir) == NULL) {
        printf("SKIP: write restriction (mkdtemp failed)\n");
        return 0;
    }
    char rw_file[256];
    char outside_file[] = "/tmp/sandbox_outside_denied.txt";
    snprintf(rw_file, sizeof(rw_file), "%s/probe.txt", rw_dir);

    /* "/dev/null" 为非目录 rw 路径：回归目录专属位 EINVAL 裁剪（§204）。 */
    const char *rw_paths[] = {rw_dir, "/dev/null", NULL};
    airy_native_sandbox_t sb;
    airy_native_sandbox_init(&sb);
    sb.enabled = 1;
    sb.rw_paths = rw_paths;

    pid_t pid = fork();
    if (pid == 0) {
        if (airy_native_sandbox_apply(&sb) != 0)
            _exit(126); /* 内核不支持 Landlock：父进程 SKIP */
        probe_write(rw_file, "/dev/null", outside_file);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    unlink(rw_file);
    rmdir(rw_dir);
    unlink(outside_file);

    if (code == 126) {
        printf("SKIP: write restriction (Landlock unavailable)\n");
        return 0;
    }
    TEST_ASSERT(code == 0, "rw_paths writable, outside read-only denied");
    return 0;
}

static int test_sandbox_deny_network(void)
{
    airy_native_sandbox_t sb;
    airy_native_sandbox_init(&sb);
    sb.enabled = 1;
    sb.deny_network = 1;

    pid_t pid = fork();
    if (pid == 0) {
        if (airy_native_sandbox_apply(&sb) != 0)
            _exit(126);
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            close(fd);
            _exit(31); /* 网络未被阻止 */
        }
        _exit(errno == EPERM ? 0 : 32); /* 0 = seccomp 返回 EPERM */
    }
    int status = 0;
    waitpid(pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (code == 126) {
        printf("SKIP: deny_network (sandbox apply unavailable)\n");
        return 0;
    }
    TEST_ASSERT(code == 0, "socket() denied with EPERM under deny_network");
    return 0;
}

#else /* !__linux__ */

static int test_sandbox_enabled_exec(void)
{
    printf("SKIP: enabled exec (non-Linux)\n");
    return 0;
}

static int test_sandbox_write_restriction(void)
{
    printf("SKIP: write restriction (non-Linux)\n");
    return 0;
}

static int test_sandbox_deny_network(void)
{
    printf("SKIP: deny_network (non-Linux)\n");
    return 0;
}

#endif /* __linux__ */

int main(void)
{
    printf("===========================================\n");
    printf("  agentrt/commons/platform_sandbox 单元测试\n");
    printf("===========================================\n\n");

    TEST_RUN(test_sandbox_init);
    TEST_RUN(test_sandbox_noop);
    TEST_RUN(test_sandbox_null);
    TEST_RUN(test_sandbox_enabled_exec);
    TEST_RUN(test_sandbox_write_restriction);
    TEST_RUN(test_sandbox_deny_network);

    printf("\n===========================================\n");
    printf("  测试结果: %d 通过, %d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");

    return failed_tests > 0 ? 1 : 0;
}
