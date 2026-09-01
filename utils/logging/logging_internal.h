// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logging_internal.h
 * @brief Unified layered logging system - internal sharing header.
 *
 * 2026-08-27 域拆分：原 logging.c（969 行）按职责拆为四个翻译单元，
 * 全局状态经本头的结构体/访问器声明衔接：
 *   - logging_core.c          日志核心：状态、级别、写入路径、生命周期
 *   - logging_format.c        格式化：控制台行格式化 + 终端/颜色探测
 *   - logging_backend_file.c  输出后端：文件打开/写入 + 轮转管理
 *   - logging_control.c       运行时控制：热加载、节流、采样
 * 所有拆分文件沿用 AIRY_COMPLIANCE_IMPL 编译定义（见 commons/CMakeLists.txt）。
 */

#ifndef LOGGING_INTERNAL_H
#define LOGGING_INTERNAL_H

#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> /* GetSystemTimeAsFileTime / GetCurrentProcessId */
#include <io.h>
#include <errno.h>
#else
#include <unistd.h>
#include <errno.h>
#endif

/* Unified base library compatibility layer */
#include "atomic_compat.h"
#include "airy_memory.h"
#include "platform.h"
#include "string_compat.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "../error/error.h"

#define MAX_MESSAGE_LEN 4096

/* ==================== Shared state layout ==================== */

typedef struct {

    log_config_t manager;

    bool initialized;

    airy_mtx_t mutex;

    log_config_t default_config;

    struct {
        char pattern[128];
        log_level_t level;
    } module_levels[32];

    size_t module_level_count;
} logging_state_t;

/* logging_core.c 定义的全局状态访问器 */
logging_state_t *log_internal_state(void);
const char *const *log_internal_level_names(size_t *count);
bool log_internal_color_enabled(void);

/* 文件后端状态（logging_backend_file.c 定义） */
typedef struct {
    FILE *file;
    size_t current_size;
    airy_mtx_t mutex;
    bool mutex_init;
} log_file_state_t;

log_file_state_t *log_internal_file_state(void);

/* ==================== logging_format.c ==================== */

bool log_internal_is_terminal(int fd);
size_t log_internal_format_message(const log_record_t *record, char *buffer, size_t buffer_size);

/* ==================== logging_backend_file.c ==================== */

int log_internal_file_open(const char *path);
void log_internal_file_write(const log_record_t *record, const char *formatted_message,
                             size_t formatted_len);
void log_internal_file_cleanup(void);

/* ==================== logging_control.c ==================== */

void log_internal_throttle_init_mutex(void);
void log_internal_throttle_cleanup(void);
bool log_internal_throttle_suppress(const char *module, int line, const char *message,
                                    uint64_t now_sec);

#endif /* LOGGING_INTERNAL_H */
