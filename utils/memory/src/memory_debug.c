// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_debug.c
 * @brief 统一内存管理模块 - 内存调试生命周期与配置域
 *
 * 本文件保留内存调试模块的入口与核心状态机：初始化、启用状态、
 * 错误回调注册、特性开关与日志级别配置，以及全局状态与锁工具定义。
 */

#include "memory_debug.h"
#include "memory_debug_internal.h"

#include "airy_memory.h"
#include "logging.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <dbghelp.h>
#else
#include <execinfo.h>
#include <sys/time.h>
#include "platform.h"
#include <stdint.h>
#endif

memory_debug_state_t g_debug_state = {0};

static bool memory_debug_lock_init(void)
{
#ifdef _WIN32
    airy_mtx_init(&g_debug_state.lock);
    return true;
#else
    return airy_mtx_init(&g_debug_state.lock) == 0;
#endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void memory_debug_lock_destroy(void)
{
#ifdef _WIN32
    airy_mtx_destroy(&g_debug_state.lock);
#else
    airy_mtx_destroy(&g_debug_state.lock);
#endif
}

void memory_debug_lock(void)
{
#ifdef _WIN32
    airy_mtx_lock(&g_debug_state.lock);
#else
    airy_mtx_lock(&g_debug_state.lock);
#endif
}

void memory_debug_unlock(void)
{
#ifdef _WIN32
    airy_mtx_unlock(&g_debug_state.lock);
#else
    airy_mtx_unlock(&g_debug_state.lock);
#endif
}

uint64_t memory_debug_get_timestamp(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ts = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ts / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void __attribute__((unused)) memory_debug_record_error(memory_error_type_t type, void *addr,
                                                              size_t size, const char *description,
                                                              const char *file, int line,
                                                              const char *function)
{
    if (!g_debug_state.initialized) {
        return;
    }

    memory_error_report_t report = {.type = type,
                                    .address = addr,
                                    .size = size,
                                    .description = description,
                                    .file = file,
                                    .line = line,
                                    .function = function,
                                    .timestamp = memory_debug_get_timestamp()};

    g_debug_state.error_count++;

    if (g_debug_state.options.verbosity_level >= 1) {
        LOG_ERROR("[内存错误] 类型%d, 地址%p, 大小%zu", type, addr, size);
        if (description != NULL) {
            LOG_ERROR("描述%s", description);
        }
        if (file != NULL && function != NULL) {
            LOG_ERROR("位置%s:%d (%s)", file, line, function);
        }
    }

    if (g_debug_state.callback != NULL) {
        g_debug_state.callback(&report, g_debug_state.callback_user_data);
    }
}

bool memory_debug_init(const memory_debug_options_t *options)
{
    if (g_debug_state.initialized) {
        return true;
    }

    LOG_INFO("memory_debug: memory_debug_init (redzone=%zu, track_alloc=%s, leak_check=%s, "
             "boundary_check=%s)",
             options ? options->redzone_size : 0,
             options && options->track_allocations ? "true" : "false",
             options && options->enable_leak_check ? "true" : "false",
             options && options->enable_boundary_check ? "true" : "false");

    if (!memory_debug_lock_init()) {
        return false;
    }

    memory_debug_lock();

    if (options != NULL) {
        __builtin_memcpy(&g_debug_state.options, options, sizeof(memory_debug_options_t));
    }

    g_debug_state.block_list_head = NULL;
    g_debug_state.block_count = 0;
    g_debug_state.total_allocations = 0;
    g_debug_state.total_frees = 0;
    g_debug_state.error_count = 0;
    g_debug_state.callback = NULL;
    g_debug_state.callback_user_data = NULL;
    g_debug_state.stack_trace_enabled = false;
    g_debug_state.max_stack_depth = 16;
    g_debug_state.next_checkpoint_id = 1;

    g_debug_state.initialized = true;

    memory_debug_unlock();

    return true;
}

bool memory_debug_enable(bool enable)
{
    return g_debug_state.initialized;
}

bool memory_debug_is_enabled(void)
{
    return g_debug_state.initialized && g_debug_state.options.track_allocations;
}

void memory_debug_set_callback(memory_debug_callback_t callback, void *user_data)
{
    memory_debug_lock();
    g_debug_state.callback = callback;
    g_debug_state.callback_user_data = user_data;
    memory_debug_unlock();
}

bool memory_debug_set_feature(const char *feature, bool enable)
{
    if (!g_debug_state.initialized || feature == NULL) {
        return false;
    }

    memory_debug_lock();

    bool success = false;

    if (strcmp(feature, "leak_check") == 0) {
        g_debug_state.options.enable_leak_check = enable;
        success = true;
    } else if (strcmp(feature, "boundary_check") == 0) {
        g_debug_state.options.enable_boundary_check = enable;
        success = true;
    } else if (strcmp(feature, "use_after_free_check") == 0) {
        g_debug_state.options.enable_use_after_free_check = enable;
        success = true;
    } else if (strcmp(feature, "double_free_check") == 0) {
        g_debug_state.options.enable_double_free_check = enable;
        success = true;
    } else if (strcmp(feature, "invalid_free_check") == 0) {
        g_debug_state.options.enable_invalid_free_check = enable;
        success = true;
    } else if (strcmp(feature, "track_allocations") == 0) {
        g_debug_state.options.track_allocations = enable;
        success = true;
    }

    memory_debug_unlock();

    return success;
}

void memory_debug_set_log_level(int level)
{
    if (level < 0)
        level = 0;
    if (level > 3)
        level = 3;

    memory_debug_lock();
    g_debug_state.options.verbosity_level = level;
    memory_debug_unlock();
}
