/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file platform_paths.h
 * @brief Cross-platform compatibility layer - path constants & AIRY_HOME
 *
 * Compile-time directory macros and the runtime AIRY_HOME path system.
 * Domain split of platform.h (2026-08-27).
 *
 * @see platform.h aggregate entry
 */

#ifndef AIRY_RT_PLATFORM_PATHS_H
#define AIRY_RT_PLATFORM_PATHS_H

#include "platform_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/* ==================== Path separators & limits ==================== */

#if AIRY_PLATFORM_WINDOWS
#define AIRY_PATH_SEP '\\'
#define AIRY_PATH_SEP_STR "\\"
#ifndef AIRY_PATH_MAX
#define AIRY_PATH_MAX 260
#endif
#else
#define AIRY_PATH_SEP '/'
#define AIRY_PATH_SEP_STR "/"
#ifndef AIRY_PATH_MAX
#define AIRY_PATH_MAX 4096
#endif
#endif


/* ==================== Compile-time directory defaults ==================== */

#if AIRY_PLATFORM_WINDOWS

#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "C:\\ProgramData\\agentrt\\run"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "C:\\ProgramData\\agentrt\\logs"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "C:\\ProgramData\\agentrt\\config"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "C:\\ProgramData\\agentrt\\data"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "C:\\ProgramData\\agentrt\\tmp"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "C:\\ProgramData\\agentrt\\cache"
#endif
#elif AIRY_PLATFORM_LINUX

#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "/tmp/agentrt"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "/var/log/agentrt"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "/etc/agentrt"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "/var/lib/agentrt"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "/var/tmp/agentrt"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "/var/cache/agentrt"
#endif
#elif AIRY_PLATFORM_MACOS

#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "/tmp/agentrt"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "/var/log/agentrt"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "/etc/agentrt"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "/var/lib/agentrt"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "/var/tmp/agentrt"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "/var/cache/agentrt"
#endif
#else

#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "/tmp/agentrt"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "/tmp/agentrt/logs"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "/etc/agentrt"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "/var/lib/agentrt"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "/var/tmp/agentrt"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "/var/cache/agentrt"
#endif
#endif


/* ==================== AIRY_HOME path system (production-ready) ====================
 *
 * Unified install root: defaults to $HOME/.airymaxrt, overridable via the
 * AIRY_HOME environment variable. All runtime artifacts (socket/pid/log/
 * config/data) are consolidated under it, keeping non-root deployments,
 * containerization and uninstall clean, replacing scattered FHS paths
 * (/tmp, /var/log, /etc...).
 *
 * Directory layout:
 *   $AIRY_HOME/bin      - executables (agentrt, agent_d, llm_d, mem_d, airy_cli)
 *   $AIRY_HOME/lib      - runtime dependencies (airymax_agents, openlab, sdk-python)
 *   $AIRY_HOME/run      - Unix sockets, PID files (volatile)
 *   $AIRY_HOME/config   - agentrt.yaml, model.yaml, secrets.env (user config)
 *   $AIRY_HOME/agents   - user agent definitions
 *   $AIRY_HOME/data/agentrt - ★ runtime data unified root (2026-08-25):
 *                        logs / cache / tmp / workspaces / heapstore / memory /
 *                        hall / state / roadmap / cli, all consolidated here.
 *
 * Runtime data unification (2.1.2.5, 2026-08-25): ALL runtime data lives
 * under $AIRY_HOME/data/agentrt (single root). Top level keeps only
 * read-only distribution artifacts (bin/lib/include/share/modules/scripts),
 * user config/agents, and the volatile run/ directory (sockets/pids).
 * Durable module data (heapstore/memory/hall/state/roadmap/workspaces),
 * logs (daemon + agent subprocess), caches (incl. PYTHONPYCACHEPREFIX) and
 * temp files all live under the unified data root.
 */

#define AIRY_DEFAULT_HOME_DIR ".airymaxrt"
#define AIRY_HOME_SUBDIR_BIN "bin"
#define AIRY_HOME_SUBDIR_LIB "lib"
#define AIRY_HOME_SUBDIR_RUN "run"
#define AIRY_HOME_SUBDIR_LOG "data/agentrt/logs"
#define AIRY_HOME_SUBDIR_CONFIG "config"
#define AIRY_HOME_SUBDIR_DATA "data"
#define AIRY_HOME_SUBDIR_TMP "data/agentrt/tmp"
#define AIRY_HOME_SUBDIR_CACHE "data/agentrt/cache"
/* 2.1.2.5：持久化工作区（GRAD 决策链 trace / 任务工作区）随统一数据根
 * 归入 $AIRY_HOME/data/agentrt/workspaces（2026-08-25 从顶层 workspace 迁入）。 */
#define AIRY_HOME_SUBDIR_WORKSPACE "data/agentrt/workspaces"


const char *airy_home_dir(void);

const char *airy_bin_dir(void);

const char *airy_lib_dir(void);

const char *airy_runtime_dir(void);

/**
 * @brief Resolve runtime socket path: $AIRY_HOME/run/<name>
 *        (AIRY_HOME path system).
 *
 * Consolidates daemon socket locations: the legacy macro
 * AIRY_RUNTIME_DIR "/<name>.sock" is a compile-time constant (default
 * /tmp/agentrt) and does not follow AIRY_HOME deployments. This helper
 * appends airy_runtime_dir() at runtime so the socket always lands in
 * $AIRY_HOME/run.
 *
 * @param name socket file name (e.g. "agent.sock")
 * @return static buffer (read once at startup; safe during the daemon's
 *         single-threaded init phase)
 */
const char *airy_runtime_dir_socket(const char *name);

const char *airy_log_dir(void);

const char *airy_config_dir(void);

const char *airy_data_dir(void);

const char *airy_tmp_dir(void);

const char *airy_cache_dir(void);

/**
 * @brief Resolve the persistent workspace root directory: $AIRY_HOME/workspace
 *        (2.1.2.5 path-system normalization).
 *
 * Holds durable task artifacts: GRAD decision-chain workspaces
 * (<plan_id>/t2|c_verify|b_arbiter|trace), task execution scratch and
 * future work-hall artifacts. Kept separate from airy_runtime_dir()
 * ($AIRY_HOME/run), which is reserved for volatile runtime sockets/pids.
 *
 * Named *root* to avoid clashing with the coreloopthree workspace object
 * accessor airy_workspace_dir(const airy_workspace_t *).
 */
const char *airy_workspace_root_dir(void);

/**
 * @brief Initialize the AIRY_HOME path system
 *
 * Resolves each directory path and creates them (mkdir -p), while also
 * setenv-ing AIRY_RUNTIME_DIR/AIRY_LOG_DIR/AIRY_CONFIG_DIR/AIRY_DATA_DIR/
 * AIRY_TMP_DIR/AIRY_CACHE_DIR so existing getenv-style consumers take
 * effect immediately. Each daemon calls this once early in main (before
 * log initialization).
 *
 * @return AIRY_SUCCESS on success; AIRY_ERR_SYS_FILE on directory creation failure
 */
int airy_paths_init(void);


#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_PATHS_H */
