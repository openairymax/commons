// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_debug_stats.c
 * @brief Unified memory management module - stats and info dump domain.
 *
 * Implements memory debug info dump and allocation/free/error count
 * statistics interfaces.
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

void memory_debug_dump_info(const char *file, bool include_stack_trace)
{
    if (!g_debug_state.initialized) {
        return;
    }

    memory_debug_lock();

    const char *output_file = file ? file : g_debug_state.options.log_file;
    FILE *output = output_file ? fopen(output_file, "w") : stderr;

    if (output == NULL) {
        memory_debug_unlock();
        return;
    }

    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "=== 内存调试信息转储 ===\n");
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "时间%llu\n", (unsigned long long)memory_debug_get_timestamp());
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "总分配次数：%zu\n", g_debug_state.total_allocations);
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "总释放次数：%zu\n", g_debug_state.total_frees);
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "当前分配块数%zu\n", g_debug_state.block_count);
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "错误数量%zu\n", g_debug_state.error_count);
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "调试选项：\n");
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "  泄漏检查：%s\n", g_debug_state.options.enable_leak_check ? "启用" : "禁用");
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "  边界检查：%s\n",
            g_debug_state.options.enable_boundary_check ? "启用" : "禁用");
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "  红区大小%zu字节\n", g_debug_state.options.redzone_size);
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "堆栈跟踪%s\n", g_debug_state.stack_trace_enabled ? "启用" : "禁用");

    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "当前分配块：\n");

    memory_debug_block_t *current = g_debug_state.block_list_head;
    size_t count = 0;

    while (current != NULL && count < 50) {
        count++;
        void *user_ptr = (uint8_t *)current + g_debug_state.options.redzone_size;

        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "：#%zu:\n", count);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "  用户地址%p\n", user_ptr);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "  块地址%p\n", (void *)current);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "  大小%zu字节\n", current->size);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "  已分配：%s\n", current->allocated ? "yes" : "no");
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "  标签%s\n", current->tag ? current->tag : "(null)");

        if (current->file != NULL) {
            /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
            fprintf(output, "  位置%s:%d", current->file, current->line);
            if (current->function != NULL) {
                /* BAN-70 EXEMPT: memory diagnostic report/dump output to
                 * configurable FILE* stream */
                fprintf(output, " (%s)", current->function);
            }
            /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
            fprintf(output, "\n");
        }

        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "  时间戳：%llu\n", (unsigned long long)current->timestamp);

        if (include_stack_trace && current->stack_depth > 0) {
            /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
            fprintf(output, "  堆栈跟踪%zu帧）：\n", current->stack_depth);
            for (size_t i = 0; i < current->stack_depth && i < 8; i++) {
                /* BAN-70 EXEMPT: memory diagnostic report/dump output to
                 * configurable FILE* stream */
                fprintf(output, "    [%zu] %p\n", i, current->stack_trace[i]);
            }
            if (current->stack_depth > 8) {
                /* BAN-70 EXEMPT: memory diagnostic report/dump output to
                 * configurable FILE* stream */
                fprintf(output, "    ...%zu更多帧）\n", current->stack_depth - 8);
            }
        }

        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "\n");

        current = current->next;
    }

    if (count >= 50) {
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "...（更多块，总计%zu个）\n", g_debug_state.block_count);
    }

    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "========================\n");

    if (output != stderr) {
        fclose(output);
    }

    memory_debug_unlock();
}

bool memory_debug_get_stats(size_t *total_allocations, size_t *total_frees,
                            size_t *current_allocations, size_t *error_count)
{
    if (!g_debug_state.initialized) {
        return false;
    }

    memory_debug_lock();

    if (total_allocations != NULL) {
        *total_allocations = g_debug_state.total_allocations;
    }

    if (total_frees != NULL) {
        *total_frees = g_debug_state.total_frees;
    }

    if (current_allocations != NULL) {
        *current_allocations = g_debug_state.block_count;
    }

    if (error_count != NULL) {
        *error_count = g_debug_state.error_count;
    }

    memory_debug_unlock();

    return true;
}

void memory_debug_reset_stats(void)
{
    if (!g_debug_state.initialized) {
        return;
    }

    memory_debug_lock();

    g_debug_state.total_allocations = 0;
    g_debug_state.total_frees = 0;
    g_debug_state.error_count = 0;

    memory_debug_unlock();
}
