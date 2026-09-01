// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_debug_track.c
 * @brief Unified memory management module - allocation tracking domain.
 *
 * Implements debug block linked-list management, allocation point
 * location, tag setting, stack trace capture and operation logging.
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

memory_debug_block_t *memory_debug_find_block(void *user_ptr)
{
    if (user_ptr == NULL) {
        return NULL;
    }

    uint8_t *block_ptr = (uint8_t *)user_ptr - g_debug_state.options.redzone_size;
    memory_debug_block_t *block = (memory_debug_block_t *)block_ptr;

    if (block->magic != MEMORY_DEBUG_MAGIC)
        return NULL;

    return block;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void memory_debug_add_block(memory_debug_block_t *block)
{
    if (block == NULL) {
        return;
    }

    block->next = g_debug_state.block_list_head;
    block->prev = NULL;

    if (g_debug_state.block_list_head != NULL) {
        g_debug_state.block_list_head->prev = block;
    }

    g_debug_state.block_list_head = block;
    g_debug_state.block_count++;
}

static void memory_debug_remove_block(memory_debug_block_t *block)
{
    if (block == NULL) {
        return;
    }

    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        g_debug_state.block_list_head = block->next;
    }

    if (block->next != NULL) {
        block->next->prev = block->prev;
    }

    block->next = NULL;
    block->prev = NULL;
    g_debug_state.block_count--;
}
#pragma GCC diagnostic pop

static size_t __attribute__((unused)) memory_debug_capture_stack_trace(void **frames,
                                                                       size_t max_frames)
{
    if (!g_debug_state.stack_trace_enabled || max_frames == 0) {
        return 0;
    }

#ifdef _WIN32
    return CaptureStackBackTrace(0, (DWORD)max_frames, frames, NULL);
#else
    return backtrace(frames, max_frames);
#endif
}

bool memory_debug_get_allocation_info(void *ptr, const char **file, int *line,
                                      const char **function, const char **tag)
{
    if (!g_debug_state.initialized || ptr == NULL) {
        return false;
    }

    memory_debug_lock();

    memory_debug_block_t *block = memory_debug_find_block(ptr);
    bool found = (block != NULL);

    if (found) {
        if (file != NULL)
            *file = block->file;
        if (line != NULL)
            *line = block->line;
        if (function != NULL)
            *function = block->function;
        if (tag != NULL)
            *tag = block->tag;
    }

    memory_debug_unlock();

    return found;
}

bool memory_debug_set_tag(void *ptr, const char *tag)
{
    if (!g_debug_state.initialized || ptr == NULL) {
        return false;
    }

    memory_debug_lock();

    memory_debug_block_t *block = memory_debug_find_block(ptr);
    bool success = (block != NULL);

    if (success) {
        /* Note: do not free the old tag; it is a static string or
         * managed by memory.c. */
        block->tag = tag;
    }

    memory_debug_unlock();

    return success;
}

bool memory_debug_enable_stack_trace(bool enable, size_t max_depth)
{
    if (!g_debug_state.initialized) {
        return false;
    }

    memory_debug_lock();

    g_debug_state.stack_trace_enabled = enable;
    if (max_depth > 0 && max_depth <= 64) {
        g_debug_state.max_stack_depth = max_depth;
    }

    memory_debug_unlock();

    return true;
}

size_t memory_debug_get_stack_trace(void *ptr, void **frames, size_t max_frames)
{
    if (!g_debug_state.initialized || ptr == NULL || frames == NULL || max_frames == 0) {
        return 0;
    }

    memory_debug_lock();

    memory_debug_block_t *block = memory_debug_find_block(ptr);
    size_t depth = 0;

    if (block != NULL && block->stack_depth > 0) {
        depth = (block->stack_depth < max_frames) ? block->stack_depth : max_frames;
        __builtin_memcpy(frames, block->stack_trace, depth * sizeof(void *));
    }

    memory_debug_unlock();

    return depth;
}

void memory_debug_log_operation(const char *operation, void *ptr, size_t size, const char *file,
                                int line, const char *function)
{
    if (!g_debug_state.initialized || g_debug_state.options.verbosity_level < 3) {
        return;
    }

    /* Build the full message for the debug log */
    char log_buf[512];
    int offset = 0;
    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "[内存操作] %s", operation);
    if (ptr != NULL) {
        offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, " %p", ptr);
    }
    if (size > 0) {
        offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, " (%zu字节)", size);
    }
    if (file != NULL && function != NULL) {
        offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, " at %s:%d (%s)", file, line,
                           function);
    }
    AIRY_LOG_DEBUG("%s", log_buf);
}
