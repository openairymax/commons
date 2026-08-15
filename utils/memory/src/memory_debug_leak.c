// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_debug_leak.c
 * @brief Unified memory management module - leak detection domain.
 *
 * Implements memory leak detection report output, plus allocation
 * checkpoint creation and comparison analysis.
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

typedef struct {
    unsigned int id;
    char name[256];
    size_t block_count;
    size_t total_allocations;
    size_t total_frees;
    size_t error_count;
    uint64_t timestamp;
    bool valid;
} memory_checkpoint_t;

#define MAX_CHECKPOINTS 16

static memory_checkpoint_t g_checkpoints[MAX_CHECKPOINTS];
static int g_checkpoint_count = 0;

size_t memory_debug_check_leaks(memory_leak_report_t *report, bool dump_to_log)
{
    if (!g_debug_state.initialized) {
        return 0;
    }

    memory_debug_lock();

    size_t leak_count = 0;
    size_t total_leaked_bytes = 0;

    memory_debug_block_t *current = g_debug_state.block_list_head;

    while (current != NULL) {
        if (current->allocated) {
            leak_count++;
            total_leaked_bytes += current->size;

            if (report != NULL && leak_count <= 100) {
                report->leaks[leak_count - 1].address =
                    (uint8_t *)current + g_debug_state.options.redzone_size;
                report->leaks[leak_count - 1].size = current->size;
                report->leaks[leak_count - 1].tag = current->tag;
                report->leaks[leak_count - 1].file = current->file;
                report->leaks[leak_count - 1].line = current->line;
                report->leaks[leak_count - 1].function = current->function;
                report->leaks[leak_count - 1].timestamp = current->timestamp;
            }
        }
        current = current->next;
    }

    if (report != NULL) {
        report->leak_count = leak_count;
        report->total_leaked_bytes = total_leaked_bytes;
    }

    if (dump_to_log && leak_count > 0) {
        FILE *log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }

        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "=== 内存泄漏检测报告===\n");
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "时间%llu\n", (unsigned long long)memory_debug_get_timestamp());
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "泄漏块数%zu\n", leak_count);
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "泄漏字节数：%zu\n", total_leaked_bytes);

        if (g_debug_state.options.verbosity_level >= 2) {
            /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
            fprintf(log, "泄漏详情：\n");

            memory_debug_block_t *current = g_debug_state.block_list_head;
            size_t count = 0;

            while (current != NULL && count < 20) {
                if (current->allocated) {
                    count++;
                    void *user_ptr = (uint8_t *)current + g_debug_state.options.redzone_size;
                    /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                    fprintf(log, "  %p: %zu字节", user_ptr, current->size);

                    if (current->tag != NULL) {
                        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                        fprintf(log, " [%s]", current->tag);
                    }

                    if (current->file != NULL) {
                        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                        fprintf(log, " (%s:%d", current->file, current->line);
                        if (current->function != NULL) {
                            /* BAN-70 EXEMPT: diagnostic report output to configurable
                             * FILE* stream */
                            fprintf(log, " in %s", current->function);
                        }
                        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                        fprintf(log, ")");
                    }

                    /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                    fprintf(log, "\n");
                }
                current = current->next;
            }

            if (count >= 20) {
                /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                fprintf(log, "  ...（更多泄漏，总计%zu个）\n", leak_count);
            }
        }

        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "========================\n");

        if (log != stderr) {
            fclose(log);
        }
    }

    memory_debug_unlock();

    return total_leaked_bytes;
}

unsigned int memory_debug_checkpoint(const char *name)
{
    if (!g_debug_state.initialized) {
        return 0;
    }

    memory_debug_lock();

    if (g_checkpoint_count >= MAX_CHECKPOINTS) {
        memory_debug_unlock();
        return 0;
    }

    unsigned int id = g_debug_state.next_checkpoint_id++;

    memory_checkpoint_t *cp = &g_checkpoints[g_checkpoint_count];
    cp->id = id;
    cp->block_count = g_debug_state.block_count;
    cp->total_allocations = g_debug_state.total_allocations;
    cp->total_frees = g_debug_state.total_frees;
    cp->error_count = g_debug_state.error_count;
    cp->timestamp = memory_debug_get_timestamp();
    cp->valid = true;

    if (name != NULL) {
        AIRY_STRNCPY_TERM(cp->name, name, sizeof(cp->name));
        cp->name[sizeof(cp->name) - 1] = '\0';
    } else {
        cp->name[0] = '\0';
    }

    g_checkpoint_count++;

    if (g_debug_state.options.verbosity_level >= 1) {
        AIRY_LOG_INFO("[内存检查点] ID=%u, 名称=%s, 块数=%zu, 分配=%zu, 释放=%zu", id, cp->name,
                 cp->block_count, cp->total_allocations, cp->total_frees);
    } else if (g_debug_state.options.verbosity_level >= 2) {
        AIRY_LOG_DEBUG("[检查点] ID=%u, 名称=%s, 块数=%zu", id, cp->name, cp->block_count);
    }

    memory_debug_unlock();

    return id;
}

size_t memory_debug_compare_checkpoints(unsigned int checkpoint1, unsigned int checkpoint2,
                                        memory_leak_report_t *diff_report)
{
    if (diff_report != NULL) {
        __builtin_memset(diff_report, 0, sizeof(memory_leak_report_t));
    }

    if (!g_debug_state.initialized) {
        return 0;
    }

    memory_debug_lock();

    int cp1_idx = -1, cp2_idx = -1;

    for (int i = 0; i < g_checkpoint_count; i++) {
        if (g_checkpoints[i].id == checkpoint1 && g_checkpoints[i].valid) {
            cp1_idx = i;
        }
        if (g_checkpoints[i].id == checkpoint2 && g_checkpoints[i].valid) {
            cp2_idx = i;
        }
    }

    if (cp1_idx < 0 || cp2_idx < 0) {
        memory_debug_unlock();
        return 0;
    }

    memory_checkpoint_t *cp1 = &g_checkpoints[cp1_idx];
    memory_checkpoint_t *cp2 = &g_checkpoints[cp2_idx];

    size_t new_allocations = 0;
    size_t new_frees = 0;
    size_t leaked_bytes = 0;

    if (cp2->total_allocations > cp1->total_allocations) {
        new_allocations = cp2->total_allocations - cp1->total_allocations;
    }
    if (cp2->total_frees > cp1->total_frees) {
        new_frees = cp2->total_frees - cp1->total_frees;
    }
    (void)new_allocations;
    (void)new_frees;

    if (cp2->block_count > cp1->block_count && diff_report != NULL) {
        size_t leak_diff = cp2->block_count - cp1->block_count;
        diff_report->leak_count = leak_diff;

        memory_debug_block_t *current = g_debug_state.block_list_head;
        size_t report_idx = 0;

        while (current != NULL && report_idx < 100) {
            if (current->allocated && current->timestamp >= cp1->timestamp &&
                current->timestamp <= cp2->timestamp) {

                diff_report->leaks[report_idx].address =
                    (uint8_t *)current + g_debug_state.options.redzone_size;
                diff_report->leaks[report_idx].size = current->size;
                diff_report->leaks[report_idx].tag = current->tag;
                diff_report->leaks[report_idx].file = current->file;
                diff_report->leaks[report_idx].line = current->line;
                diff_report->leaks[report_idx].function = current->function;
                diff_report->leaks[report_idx].timestamp = current->timestamp;

                leaked_bytes += current->size;
                report_idx++;
            }
            current = current->next;
        }

        diff_report->total_leaked_bytes = leaked_bytes;
    }

    if (g_debug_state.options.verbosity_level >= 2) {
        AIRY_LOG_DEBUG("[检查点比较] CP1(#%u) vs CP2(#%u)", checkpoint1, checkpoint2);
        AIRY_LOG_DEBUG("  新分配: %zu次", new_allocations);
        AIRY_LOG_DEBUG("  新释放: %zu次", new_frees);
        AIRY_LOG_DEBUG("  泄漏块: %zu个", diff_report ? diff_report->leak_count : 0);
        AIRY_LOG_DEBUG("  泄漏字节: %zu", leaked_bytes);
    }

    memory_debug_unlock();

    return leaked_bytes;
}
