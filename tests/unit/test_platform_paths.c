// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_platform_paths.c
 * @brief AIRY_HOME 路径体系契约测试：运行时目录覆盖优先级
 *
 * airy_runtime_dir() 的解析是进程级一次性状态，解析后不再重算，因此
 * 「显式覆盖」与「安装根派生」两种环境必须在不同进程中验证：同一可执行
 * 体由 ctest 以不同的 AIRY_HOME / AIRY_RUNTIME_DIR 注册两次
 * （见 tests/CMakeLists.txt）。
 *
 * 守护的契约：显式非空 AIRY_RUNTIME_DIR（部署方 -r / var/run 场景）为
 * 权威；未设置时回落到 $AIRY_HOME/run。其余子目录永远从 AIRY_HOME 派生
 * ——覆盖只允许作用于易失的运行时目录，分发物与数据面跨根会撕裂安装。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "airy_types.h"
#include "platform_paths.h"

static int expect_path(const char *got, const char *want, const char *what)
{
    if (!got || strcmp(got, want) != 0) {
        fprintf(stderr, "✗FAIL: %s: got '%s' want '%s'\n", what, got ? got : "(null)", want);
        return 1;
    }
    return 0;
}

static int check_basic_paths(const char *home, const char *want_run)
{
    char want_sock[AIRY_PATH_MAX];
    char want_bin[AIRY_PATH_MAX];
    char want_log[AIRY_PATH_MAX];
    snprintf(want_sock, sizeof(want_sock), "%s/monit.sock", want_run);
    snprintf(want_bin, sizeof(want_bin), "%s/bin", home);
    snprintf(want_log, sizeof(want_log), "%s/data/agentrt/logs", home);

    if (expect_path(airy_home_dir(), home, "home dir"))
        return 1;
    if (expect_path(airy_runtime_dir(), want_run, "runtime dir"))
        return 1;
    if (expect_path(airy_runtime_dir_socket("monit.sock"), want_sock, "socket path"))
        return 1;
    if (expect_path(airy_bin_dir(), want_bin, "bin dir"))
        return 1;
    if (expect_path(airy_log_dir(), want_log, "log dir"))
        return 1;
    return 0;
}

/* S31-P1-3：用户主目录与安装根是两条独立解析链。输入路径里的 `~` 只按
 * 登录用户主目录展开，AIRY_HOME 指向分发实例时也不得改变用户语义；宿主
 * 未提供主目录时返回 NULL，禁止回退 CWD 相对路径（回落由调用方决定）。 */
static int check_user_home(void)
{
#ifdef _WIN32
    const char *want = getenv("USERPROFILE");
#else
    const char *want = getenv("HOME");
#endif
    const char *got = airy_user_home_dir();
    if (!want || want[0] == '\0') {
        if (got) {
            fprintf(stderr, "✗FAIL: user home must be NULL without host home\n");
            return 1;
        }
        return 0;
    }
    return expect_path(got, want, "user home dir");
}

/* 0.1.21 A-6：只读快照契约。十项必须与逐项访问器同源（指针同一），
 * 否则医生/门禁与安装器会看到两套根；且查询不得触发目录创建。 */
static int check_query_view(const char *home, const char *want_run)
{
    char want_bin[AIRY_PATH_MAX], want_lib[AIRY_PATH_MAX], want_cfg[AIRY_PATH_MAX];
    char want_data[AIRY_PATH_MAX], want_tmp[AIRY_PATH_MAX], want_cache[AIRY_PATH_MAX];
    char want_log[AIRY_PATH_MAX], want_ws[AIRY_PATH_MAX];
    snprintf(want_bin, sizeof(want_bin), "%s/bin", home);
    snprintf(want_lib, sizeof(want_lib), "%s/lib", home);
    snprintf(want_cfg, sizeof(want_cfg), "%s/config", home);
    snprintf(want_data, sizeof(want_data), "%s/data", home);
    snprintf(want_tmp, sizeof(want_tmp), "%s/data/agentrt/tmp", home);
    snprintf(want_cache, sizeof(want_cache), "%s/data/agentrt/cache", home);
    snprintf(want_log, sizeof(want_log), "%s/data/agentrt/logs", home);
    snprintf(want_ws, sizeof(want_ws), "%s/data/agentrt/workspaces", home);

    airy_paths_view_t v;
    if (airy_paths_query(NULL) != AIRY_EINVAL) {
        fprintf(stderr, "✗FAIL: airy_paths_query(NULL) must be AIRY_EINVAL\n");
        return 1;
    }
    if (airy_paths_query(&v) != AIRY_SUCCESS) {
        fprintf(stderr, "✗FAIL: airy_paths_query failed\n");
        return 1;
    }
    const struct {
        const char *got;
        const char *want;
        const char *what;
    } rows[] = {
        {v.home, home, "query home"},
        {v.bin, want_bin, "query bin"},
        {v.lib, want_lib, "query lib"},
        {v.run, want_run, "query run"},
        {v.log, want_log, "query log"},
        {v.config, want_cfg, "query config"},
        {v.data, want_data, "query data"},
        {v.tmp, want_tmp, "query tmp"},
        {v.cache, want_cache, "query cache"},
        {v.workspace, want_ws, "query workspace"},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        if (expect_path(rows[i].got, rows[i].want, rows[i].what))
            return 1;
    }
    if (v.home != airy_home_dir() || v.run != airy_runtime_dir() ||
        v.log != airy_log_dir() || v.workspace != airy_workspace_root_dir()) {
        fprintf(stderr, "✗FAIL: query view must alias accessor buffers\n");
        return 1;
    }
    return 0;
}

/* 初始化会把解析结果回写环境变量供 getenv 消费方使用：回写值必须与
 * 解析值一致，否则 daemon 绑定目录与编排脚本探测目录分叉（-r 失效
 * 的原始形态）。 */
static int check_export(const char *want_run)
{
    if (airy_paths_init() != AIRY_SUCCESS) {
        fprintf(stderr, "✗FAIL: airy_paths_init failed\n");
        return 1;
    }
    if (expect_path(getenv("AIRY_RUNTIME_DIR"), want_run, "exported AIRY_RUNTIME_DIR"))
        return 1;
    if (expect_path(airy_runtime_dir(), want_run, "runtime dir after init"))
        return 1;
    return 0;
}

int main(void)
{
    const char *home = getenv("AIRY_HOME");
    const char *run_env = getenv("AIRY_RUNTIME_DIR");
    if (!home || home[0] == '\0') {
        fprintf(stderr, "✗FAIL: AIRY_HOME must be set by ctest\n");
        return 1;
    }

    const int overridden = (run_env && run_env[0] != '\0');
    char want_run[AIRY_PATH_MAX];
    if (overridden)
        snprintf(want_run, sizeof(want_run), "%s", run_env);
    else
        snprintf(want_run, sizeof(want_run), "%s/run", home);

    printf("===========================================\n");
    printf("  platform paths: %s\n",
           overridden ? "explicit AIRY_RUNTIME_DIR" : "derived from AIRY_HOME");
    printf("===========================================\n");

    if (check_basic_paths(home, want_run))
        return 1;
    if (check_user_home())
        return 1;
    if (check_query_view(home, want_run))
        return 1;
    if (check_export(want_run))
        return 1;

    printf("✔PASS: runtime=%s home=%s\n", want_run, home);
    return 0;
}
