// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_paths.c
 * @brief AIRY_HOME path system domain: runtime directory resolution,
 * directory creation and environment variable compatibility.
 */

#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#include <sys/stat.h>
#define strdup _strdup
#define access _access /* flawfinder: ignore */
#ifndef EEXIST
#define EEXIST 17
#endif
/* MSVC 未提供 S_ISDIR；以 _S_IFMT 宏补齐（对齐 utils/io file_utils.c）。 */
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__)
#include <errno.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "error.h"
#include "platform.h"
#include "cancel_token.h"
#include "airy_dirent.h"

#include "airy_memory.h"

/* ==================== AIRY_HOME path system ====================
 *
 * Unified install root: $AIRY_HOME or $HOME/.airymaxrt (consistent with
 * the ~/.<tool> convention of mainstream CLIs such as Claude Code). All
 * runtime artifacts are consolidated under it, keeping non-root
 * deployments, containerization and uninstall clean. airy_paths_init()
 * also setenvs compatibility variables so existing getenv-style consumers
 * (AIRY_RUNTIME_DIR etc.) take effect immediately. */

static airy_mtx_t g_paths_lock;
static int g_paths_initialized = 0;
static char g_home_dir[AIRY_PATH_MAX];
static char g_bin_dir[AIRY_PATH_MAX];
static char g_lib_dir[AIRY_PATH_MAX];
static char g_run_dir[AIRY_PATH_MAX];
static char g_log_dir[AIRY_PATH_MAX];
static char g_cfg_dir[AIRY_PATH_MAX];
static char g_data_dir[AIRY_PATH_MAX];
static char g_tmp_dir[AIRY_PATH_MAX];
static char g_cache_dir[AIRY_PATH_MAX];
static char g_workspace_dir[AIRY_PATH_MAX];

static void paths_resolve_one(const char *subdir, char *out, size_t out_size)
{
    if (subdir && subdir[0] != '\0')
        snprintf(out, out_size, "%s/%s", g_home_dir, subdir);
    else
        snprintf(out, out_size, "%s", g_home_dir);
}

static void paths_resolve_all(void)
{
    paths_resolve_one(AIRY_HOME_SUBDIR_BIN, g_bin_dir, sizeof(g_bin_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_LIB, g_lib_dir, sizeof(g_lib_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_RUN, g_run_dir, sizeof(g_run_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_LOG, g_log_dir, sizeof(g_log_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_CONFIG, g_cfg_dir, sizeof(g_cfg_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_DATA, g_data_dir, sizeof(g_data_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_TMP, g_tmp_dir, sizeof(g_tmp_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_CACHE, g_cache_dir, sizeof(g_cache_dir));
    paths_resolve_one(AIRY_HOME_SUBDIR_WORKSPACE, g_workspace_dir, sizeof(g_workspace_dir));
}

/* 当前可执行文件所在目录（用于定位安装根 install.env：安装布局为
 * $AIRY_HOME/bin/<bin> 与 $AIRY_HOME/config/install.env 同级上溯）。
 * 返回 1=成功（out 填充目录，不含尾斜杠），0=失败。 */
static int paths_self_dir(char *out, size_t out_size)
{
#if defined(_WIN32) || defined(_WIN64)
    char exe[AIRY_PATH_MAX];
    DWORD n = GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));
    if (n == 0 || n >= sizeof(exe))
        return 0;
    char *slash = strrchr(exe, '\\');
    if (!slash)
        slash = strrchr(exe, '/');
    if (!slash)
        return 0;
    size_t dlen = (size_t)(slash - exe);
    if (dlen == 0 || dlen >= out_size)
        return 0;
    __builtin_memcpy(out, exe, dlen);
    out[dlen] = '\0';
    return 1;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)out_size;
    if (_NSGetExecutablePath(out, &size) != 0)
        return 0;
    char *slash = strrchr(out, '/');
    if (!slash)
        return 0;
    *slash = '\0';
    return 1;
#else
    char link[AIRY_PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", link, sizeof(link) - 1);
    if (n <= 0)
        return 0;
    link[n] = '\0';
    char *slash = strrchr(link, '/');
    if (!slash)
        return 0;
    *slash = '\0';
    if (link[0] == '\0' || strlen(link) >= out_size)
        return 0;
    snprintf(out, out_size, "%s", link);
    return 1;
#endif
}

/* 读取单个 install.env 文件的 AIRY_HOME= 行（去引号/换行）。 */
static int paths_read_install_home(const char *path, char *out, size_t out_size);

/* 最后一个路径分隔符（Windows 兼容 '\\' 与 '/'，POSIX 仅 '/'）。 */
static char *paths_last_sep(char *s)
{
    char *last = NULL;
    for (; *s; s++) {
        if (*s == '/' || *s == '\\')
            last = s;
    }
    return last;
}

/* 从可执行文件位置逐级上溯查找 config/install.env。覆盖两种布局：
 *   安装布局  $AIRY_HOME/bin/<bin>                → $AIRY_HOME/config/install.env
 *   dev 布局  <runtime_root>/build/<sub>/<bin>    → <runtime_root>/config/install.env
 * 上溯上限 6 级防误伤；仅匹配含 AIRY_HOME= 的 install.env（文件内容特异性高）。 */
static int paths_walkup_install_home(char *out, size_t out_size)
{
    char dir[AIRY_PATH_MAX];
    if (!paths_self_dir(dir, sizeof(dir)))
        return 0;
    for (int depth = 0; depth < 6; depth++) {
        char cand[AIRY_PATH_MAX * 2];
        snprintf(cand, sizeof(cand), "%s/config/install.env", dir);
        if (paths_read_install_home(cand, out, out_size))
            return 1;
        char *sep = paths_last_sep(dir);
        if (!sep || sep == dir)
            break;
        *sep = '\0';
    }
    return 0;
}

/* 用户主目录：Windows 用 USERPROFILE（HOME 通常未设置），POSIX 用 HOME。 */
static const char *paths_user_home(void)
{
#if AIRY_PLATFORM_WINDOWS
    const char *h = getenv("USERPROFILE");
    if (h && h[0] != '\0')
        return h;
#endif
    return getenv("HOME");
}

/* 从 install.env（build.sh/install.sh 生成的安装信息文件）发现固化安装根。
 * 候选顺序与 airymaxrt 启动器一致：可执行文件逐级上溯（安装根/dev 布局）
 * → $HOME/.airymaxrt → $HOME/.local/share/airymaxrt。仅当环境变量
 * AIRY_HOME 未设置时兜底：直接运行二进制（无 AIRY_HOME 的独立调用）时
 * 定位到真实运行时根，避免解析到 $HOME/.airymaxrt 默认值导致 llm.sock
 * 等路径 404（2026-08-16 实测 airy_cli 直接运行失败）。
 * 返回 1 表示发现（out 填充 home 路径），0 表示未发现。 */
static int paths_discover_install_home(char *out, size_t out_size)
{
    if (paths_walkup_install_home(out, out_size))
        return 1;

    const char *uhome = paths_user_home();
    if (!uhome || uhome[0] == '\0')
        return 0;

    char cand[AIRY_PATH_MAX * 2];
    static const char *const rels[] = {
        ".airymaxrt/config/install.env",
        ".local/share/airymaxrt/config/install.env",
    };
    for (size_t i = 0; i < sizeof(rels) / sizeof(rels[0]); i++) {
        snprintf(cand, sizeof(cand), "%s/%s", uhome, rels[i]);
        if (paths_read_install_home(cand, out, out_size))
            return 1;
    }
    return 0;
}

/* 读取单个 install.env 文件的 AIRY_HOME= 行（去引号/换行）。 */
static int paths_read_install_home(const char *path, char *out, size_t out_size)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    char line[512];
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "AIRY_HOME=", 10) != 0)
            continue;
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        const char *v = line + 10;
        size_t vl = strlen(v);
        /* 去掉可能存在的引号（install.env 生成时可能带引号） */
        if (vl >= 2 && v[0] == '"' && v[vl - 1] == '"') {
            v++;
            vl -= 2;
        }
        if (vl > 0 && vl < out_size) {
            __builtin_memcpy(out, v, vl);
            out[vl] = '\0';
        }
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

static void paths_ensure_resolved(void)
{
    if (g_paths_initialized)
        return;
    airy_mtx_lock(&g_paths_lock);
    if (!g_paths_initialized) {
        const char *home_env = getenv("AIRY_HOME");
        if (home_env && home_env[0] != '\0') {
            snprintf(g_home_dir, sizeof(g_home_dir), "%s", home_env);
        } else if (paths_discover_install_home(g_home_dir, sizeof(g_home_dir))) {
            /* 安装根发现（install.env）：解析完成，跳过默认值分支 */
        } else {
            const char *user_home = paths_user_home();
            if (user_home && user_home[0] != '\0') {
                snprintf(g_home_dir, sizeof(g_home_dir), "%s/%s", user_home, AIRY_DEFAULT_HOME_DIR);
            } else {
                snprintf(g_home_dir, sizeof(g_home_dir), "%s", AIRY_DEFAULT_HOME_DIR);
            }
        }
        paths_resolve_all();
        g_paths_initialized = 1;
    }
    airy_mtx_unlock(&g_paths_lock);
}

const char *airy_home_dir(void)
{
    paths_ensure_resolved();
    return g_home_dir;
}

const char *airy_bin_dir(void)
{
    paths_ensure_resolved();
    return g_bin_dir;
}

const char *airy_lib_dir(void)
{
    paths_ensure_resolved();
    return g_lib_dir;
}

const char *airy_runtime_dir(void)
{
    paths_ensure_resolved();
    return g_run_dir;
}

const char *airy_runtime_dir_socket(const char *name)
{
    static char g_sock_buf[AIRY_PATH_MAX];
    if (!name || name[0] == '\0')
        return airy_runtime_dir();
    snprintf(g_sock_buf, sizeof(g_sock_buf), "%s/%s", airy_runtime_dir(), name);
    return g_sock_buf;
}

const char *airy_log_dir(void)
{
    paths_ensure_resolved();
    return g_log_dir;
}

const char *airy_config_dir(void)
{
    paths_ensure_resolved();
    return g_cfg_dir;
}

const char *airy_data_dir(void)
{
    paths_ensure_resolved();
    return g_data_dir;
}

const char *airy_tmp_dir(void)
{
    paths_ensure_resolved();
    return g_tmp_dir;
}

const char *airy_cache_dir(void)
{
    paths_ensure_resolved();
    return g_cache_dir;
}

const char *airy_workspace_root_dir(void)
{
    paths_ensure_resolved();
    return g_workspace_dir;
}

static int paths_mkdir_p(const char *path)
{
    if (!path || path[0] == '\0')
        return AIRY_ERR_INVALID_PARAM;

    char tmp[AIRY_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0)
        return AIRY_ERR_INVALID_PARAM;

    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        tmp[--len] = '\0';

    /* 同时识别 '/' 与 '\\'，兼容 Windows 风格 AIRY_HOME 路径 */
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/' && *p != '\\')
            continue;
        *p = '\0';
#if AIRY_PLATFORM_WINDOWS
        _mkdir(tmp);
#else
        mkdir(tmp, 0755);
#endif
        *p = '/';
    }
#if AIRY_PLATFORM_WINDOWS
    if (_mkdir(tmp) != 0 && errno != EEXIST)
        return AIRY_ERR_SYS_FILE;
#else
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return AIRY_ERR_SYS_FILE;
#endif
    return AIRY_SUCCESS;
}

/* ==================== stale tmp cleanup ====================
 *
 * 2.1.2.4：$AIRY_HOME/tmp 陈旧条目自动维护。工作大厅取消执行、测试残留
 * 等会在 tmp 下留下临时目录；长期运行后累积污染运行时根。每次路径初始化
 * 时清理 mtime 超过阈值（7 天）的条目，并设单次扫描上限，避免大目录
 * 拖慢任何进程的启动（platform 是所有进程的公共底座）。 */

#define AIRY_TMP_STALE_DAYS 7
#define AIRY_TMP_CLEAN_CAP 256

/* 递归删除目录树（内联实现，避免 platform→utils/io 的层次倒置）。 */
static int paths_rm_rf(const char *path)
{
    if (!path || !path[0])
        return -1;
    struct stat st;
    if (stat(path, &st) != 0)
        return 0; /* not exists → idempotent */
    if (!S_ISDIR(st.st_mode))
        return (remove(path) == 0) ? 0 : -1;

    DIR *dir = opendir(path);
    if (!dir)
        return -1;
    struct dirent *entry;
    char child[AIRY_PATH_MAX];
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        struct stat cst;
        if (stat(child, &cst) != 0)
            continue;
        if (S_ISDIR(cst.st_mode)) {
            if (paths_rm_rf(child) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (remove(child) != 0) {
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);
#ifdef _WIN32
    if (_rmdir(path) != 0)
        return -1;
#else
    if (rmdir(path) != 0)
        return -1;
#endif
    return 0;
}

static void paths_cleanup_stale_tmp(void)
{
    const time_t cutoff = time(NULL) - (time_t)AIRY_TMP_STALE_DAYS * 24 * 3600;
    DIR *dir = opendir(g_tmp_dir);
    if (!dir)
        return;
    struct dirent *entry;
    char full[AIRY_PATH_MAX];
    int cleaned = 0;
    while (cleaned < AIRY_TMP_CLEAN_CAP && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(full, sizeof(full), "%s/%s", g_tmp_dir, entry->d_name);
        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (st.st_mtime >= cutoff)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (paths_rm_rf(full) == 0)
                cleaned++;
        } else if (remove(full) == 0) {
            cleaned++;
        }
    }
    closedir(dir);
}

int airy_paths_init(void)
{
    paths_ensure_resolved();

    int rc = paths_mkdir_p(g_home_dir);
    if (rc != AIRY_SUCCESS)
        return rc;

    const char *dirs[] = {
        g_bin_dir, g_lib_dir, g_run_dir, g_log_dir, g_cfg_dir, g_data_dir, g_tmp_dir, g_cache_dir,
        g_workspace_dir,
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        rc = paths_mkdir_p(dirs[i]);
        if (rc != AIRY_SUCCESS)
            return rc;
    }

    /* setenv compatibility variables so existing getenv-style consumers
     * (including runtime overrides of compile-time macros) take effect.
     * Windows has no setenv; use _putenv_s (preprocessor branch). */
#if AIRY_PLATFORM_WINDOWS
#define AIRY_SETENV(key, val) _putenv_s((key), (val))
#else
#define AIRY_SETENV(key, val) setenv((key), (val), 1)
#endif
    AIRY_SETENV("AIRY_HOME", g_home_dir);
    AIRY_SETENV("AIRY_BIN_DIR", g_bin_dir);
    AIRY_SETENV("AIRY_LIB_DIR", g_lib_dir);
    AIRY_SETENV("AIRY_RUNTIME_DIR", g_run_dir);
    AIRY_SETENV("AIRY_LOG_DIR", g_log_dir);
    AIRY_SETENV("AIRY_CONFIG_DIR", g_cfg_dir);
    AIRY_SETENV("AIRY_DATA_DIR", g_data_dir);
    AIRY_SETENV("AIRY_TMP_DIR", g_tmp_dir);
    AIRY_SETENV("AIRY_CACHE_DIR", g_cache_dir);
    AIRY_SETENV("AIRY_WORKSPACE_DIR", g_workspace_dir);
#undef AIRY_SETENV

    /* 2.1.2.4：tmp 目录就绪后自动清理陈旧条目（7 天以上），
     * 保持运行时根干净（幂等，失败不影响初始化）。 */
    paths_cleanup_stale_tmp();

    return AIRY_SUCCESS;
}
