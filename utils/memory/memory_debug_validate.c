// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_debug_validate.c
 * @brief Unified memory management module - memory validation domain.
 *
 * Implements debug block integrity validation (magic/redzone), single
 * pointer validation and full traversal validation.
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

static bool memory_debug_validate_block(memory_debug_block_t *block, memory_error_report_t *error)
{
    if (block == NULL) {
        if (error != NULL) {
            memory_error_report_t report = {.type = MEMORY_ERROR_INVALID_FREE,
                                            .address = NULL,
                                            .size = 0,
                                            .description = "块指针为NULL",
                                            .file = NULL,
                                            .line = 0,
                                            .function = NULL,
                                            .timestamp = memory_debug_get_timestamp()};
            __builtin_memcpy(error, &report, sizeof(memory_error_report_t));
        }
        return false;
    }

    if (block->magic != MEMORY_DEBUG_MAGIC) {
        if (error != NULL) {
            memory_error_report_t report = {.type = MEMORY_ERROR_CORRUPTION,
                                            .address = block,
                                            .size = 0,
                                            .description = "魔数不匹配，可能内存损坏",
                                            .file = NULL,
                                            .line = 0,
                                            .function = NULL,
                                            .timestamp = memory_debug_get_timestamp()};
            __builtin_memcpy(error, &report, sizeof(memory_error_report_t));
        }
        return false;
    }

    if (g_debug_state.options.enable_boundary_check &&
        g_debug_state.options.redzone_size > sizeof(memory_debug_block_t)) {
        uint8_t *redzone_start = (uint8_t *)block + sizeof(memory_debug_block_t);
        size_t redzone_size = g_debug_state.options.redzone_size - sizeof(memory_debug_block_t);

        for (size_t i = 0; i < redzone_size; i++) {
            if (redzone_start[i] != block->redzone_pattern) {
                if (error != NULL) {
                    memory_error_report_t report = {.type = MEMORY_ERROR_OUT_OF_BOUNDS,
                                                    .address = redzone_start + i,
                                                    .size = 1,
                                                    .description = "红区损坏，可能边界外访问",
                                                    .file = block->file,
                                                    .line = block->line,
                                                    .function = block->function,
                                                    .timestamp = memory_debug_get_timestamp()};
                    __builtin_memcpy(error, &report, sizeof(memory_error_report_t));
                }
                return false;
            }
        }
    }

    return true;
}

bool memory_debug_validate(void *ptr, memory_error_report_t *error)
{
    if (ptr == NULL) {
        if (error != NULL) {
            __builtin_memset(error, 0, sizeof(memory_error_report_t));
            error->type = MEMORY_ERROR_INVALID_FREE;
        }
        return false;
    }
    if (!g_debug_state.initialized) {
        if (error != NULL) {
            __builtin_memset(error, 0, sizeof(memory_error_report_t));
        }
        return false;
    }

    memory_debug_lock();

    memory_debug_block_t *block = memory_debug_find_block(ptr);
    bool valid = memory_debug_validate_block(block, error);

    if (!valid && error != NULL && error->type == MEMORY_ERROR_NONE) {
        memory_error_report_t report = {.type = MEMORY_ERROR_INVALID_FREE,
                                        .address = ptr,
                                        .size = 0,
                                        .description = "未找到调试块，可能不是调试分配的内存",
                                        .file = NULL,
                                        .line = 0,
                                        .function = NULL,
                                        .timestamp = memory_debug_get_timestamp()};
        __builtin_memcpy(error, &report, sizeof(memory_error_report_t));
    }

    memory_debug_unlock();

    return valid;
}

size_t memory_debug_validate_all(size_t *error_count, bool dump_to_log)
{
    if (!g_debug_state.initialized) {
        if (error_count != NULL) {
            *error_count = 0;
        }
        return 0;
    }

    memory_debug_lock();

    size_t errors_found = 0;
    memory_debug_block_t *current = g_debug_state.block_list_head;

    if (dump_to_log && g_debug_state.options.verbosity_level >= 1) {
        FILE *log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }

        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "=== 内存完整性验证===\n");

        if (log != stderr) {
            fclose(log);
        }
    }

    while (current != NULL) {
        if (!memory_debug_validate_block(current, NULL)) {
            errors_found++;
        }
        current = current->next;
    }

    if (dump_to_log && g_debug_state.options.verbosity_level >= 1) {
        FILE *log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }

        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "验证完成，发现错误：%zu个\n", errors_found);
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(log, "====================\n");

        if (log != stderr) {
            fclose(log);
        }
    }

    if (error_count != NULL) {
        *error_count = errors_found;
    }

    memory_debug_unlock();

    return errors_found;
}
