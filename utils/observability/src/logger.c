// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logger.c
 * @brief airy_log_* API implementation (built on the unified logging.h).
 *
 * d6 cleanup (IRON-8 compat layer cleanup): the airy_log_* implementation
 * was migrated here from logging_compat.c and the compat wrapper removed;
 * logging_compat.h/.c have been deleted.
 *
 * Design points:
 *   1. AIRY_LOG_LEVEL_* and LOG_LEVEL_* values align exactly
 *      (DEBUG=0/INFO=1/WARN=2/ERROR=3/FATAL=4), so no level conversion is
 *      needed (the old convert_old_level_to_new in logging_compat.c had a
 *      mapping bug, turning AIRY_LOG_LEVEL_ERROR=3 into LOG_LEVEL_DEBUG;
 *      fixed by the migration).
 *   2. log_set_trace_id uses AIRY_THREAD_LOCAL g_tls_trace_id in logging.c,
 *      semantically equivalent to the old _Thread_local g_thread_trace_id
 *      in logging_compat.c, so delegate directly.
 *   3. Auto-init uses atomic_compare_exchange_strong for thread safety.
 */

#include "logger.h"

#include "logging.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdarg.h>

/**
 * @brief One-time log module initialization.
 *
 * Uses atomic CAS so log_init runs exactly once across threads.
 * Replaces the old ensure_compat_initialized + logging_compat_init
 * two-layer call chain in logging_compat.c.
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

    /* AIRY_LOG_LEVEL_* and LOG_LEVEL_* values align exactly, pass through.
     * Fixes the level-mapping bug in the old logging_compat.c
     * convert_old_level_to_new, which mapped 0->ERROR/1->WARN/2->INFO/
     * 3->DEBUG in reverse, contradicting logger.h (DEBUG=0/INFO=1/WARN=2/
     * ERROR=3) and making AIRY_LOG_ERROR actually write at DEBUG level. */
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
