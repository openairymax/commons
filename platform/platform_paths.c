// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_paths.c
 * @brief AIRY_HOME 路径体系域：运行时目录解析、目录创建与环境变量兼容
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
#include <sys/stat.h>
#define strdup _strdup
#define access _access /* flawfinder: ignore */
#ifndef EEXIST
#define EEXIST 17
#endif
#pragma comment(lib, "bcrypt.lib")
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
        } else {
            const char *user_home = getenv("HOME");
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

static int paths_mkdir_p(const char *path)
{
    if (!path || path[0] == '\0')
        return AIRY_ERR_INVALID_PARAM;

    char tmp[AIRY_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0)
        return AIRY_ERR_INVALID_PARAM;

    while (len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/')
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

int airy_paths_init(void)
{
    paths_ensure_resolved();

    int rc = paths_mkdir_p(g_home_dir);
    if (rc != AIRY_SUCCESS)
        return rc;

    const char *dirs[] = {
        g_bin_dir, g_lib_dir, g_run_dir, g_log_dir, g_cfg_dir, g_data_dir, g_tmp_dir, g_cache_dir,
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
#undef AIRY_SETENV

    return AIRY_SUCCESS;
}
