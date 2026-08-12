// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logger.c
 * @brief airy_log_* API 实现（基于统一分层日志系统 logging.h）
 *
 * @details
 * d6 清理（IRON-8 兼容层清理）：本文件从 logging_compat.c 迁移 airy_log_* 实现，
 * 消除兼容层包装。logging_compat.h/.c 已删除。
 *
 * 设计要点：
 *   1. AIRY_LOG_LEVEL_* 与 LOG_LEVEL_* 值完全对齐（DEBUG=0/INFO=1/WARN=2/ERROR=3/FATAL=4），
 *      无需级别转换函数（原 logging_compat.c 的 convert_old_level_to_new 存在级别映射 bug，
 *      把 AIRY_LOG_LEVEL_ERROR=3 错误映射为 LOG_LEVEL_DEBUG，已随迁移自动修复）。
 *   2. log_set_trace_id 在 logging.c 中已使用 AIRY_THREAD_LOCAL g_tls_trace_id，
 *      与原 logging_compat.c 的 _Thread_local g_thread_trace_id 语义等价，直接委托即可。
 *   3. 自动初始化使用 atomic_compare_exchange_strong 确保线程安全。
 */

#include "logger.h"

#include "logging.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdarg.h>

/**
 * @brief 日志模块一次性初始化
 *
 * 使用原子 CAS 确保多线程下 log_init 只调用一次。
 * 替代原 logging_compat.c 的 ensure_compat_initialized + logging_compat_init 双层调用。
 */
static void ensure_log_initialized(void)
{
    static atomic_int initialized = 0;
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&initialized, &expected, 1, memory_order_seq_cst,
                                                memory_order_seq_cst)) {
        log_init(NULL);
    }
}

const char *airy_log_set_trace_id(const char *trace_id)
{
    ensure_log_initialized();
    return log_set_trace_id(trace_id);
}

const char *airy_log_get_trace_id(void)
{
    ensure_log_initialized();
    return log_get_trace_id();
}

void airy_log_write(int level, const char *file, int line, const char *fmt, ...)
{
    ensure_log_initialized();

    /* AIRY_LOG_LEVEL_* 与 LOG_LEVEL_* 值完全对齐，直接传递。
     * 修复原 logging_compat.c convert_old_level_to_new 的级别映射 bug
     * （原实现把 0→ERROR/1→WARN/2→INFO/3→DEBUG 反向映射，与 logger.h 定义的
     *   DEBUG=0/INFO=1/WARN=2/ERROR=3 矛盾，导致 AIRY_LOG_ERROR 实际写入 DEBUG）。 */
    log_level_t new_level = (log_level_t)level;

    va_list args;
    va_start(args, fmt);
    log_write_va(new_level, file, line, fmt, args);
    va_end(args);
}

void airy_log_write_va(int level, const char *file, int line, const char *fmt, va_list args)
{
    ensure_log_initialized();
    log_level_t new_level = (log_level_t)level;
    log_write_va(new_level, file, line, fmt, args);
}

#if defined(__GNUC__) || defined(__clang__)
static void __attribute__((constructor)) logging_module_constructor(void)
{
    ensure_log_initialized();
}
#endif

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl logging_module_constructor(void)
{
    ensure_log_initialized();
}
__declspec(allocate(".CRT$XCU")) void(__cdecl *logging_module_constructor_ptr)(void) =
    logging_module_constructor;
#endif
