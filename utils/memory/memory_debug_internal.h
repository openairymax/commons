// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_debug_internal.h
 * @brief Memory debug module internal shared definitions: debug block /
 * global state structs and cross-file helper declarations.
 */

#ifndef AIRY_MEMORY_DEBUG_INTERNAL_H
#define AIRY_MEMORY_DEBUG_INTERNAL_H

#include "memory_debug.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"
#include "airy_memory.h"

/**
 * @brief Memory debug block header (redzone).
 */
typedef struct memory_debug_block {
    size_t magic;
    size_t size;
    size_t redzone_size;
    const char *tag;
    const char *file;
    int line;
    const char *function;
    uint64_t timestamp;
    unsigned char redzone_pattern;
    bool allocated;
    void *stack_trace[16];
    size_t stack_depth;
    struct memory_debug_block *next;
    struct memory_debug_block *prev;
} memory_debug_block_t;

typedef struct {
    bool initialized;
    memory_debug_options_t options;

    memory_debug_block_t *block_list_head;
    size_t block_count;

    size_t total_allocations;
    size_t total_frees;
    size_t error_count;

#ifdef _WIN32
    airy_mtx_t lock;
#else
    airy_mtx_t lock;
#endif

    memory_debug_callback_t callback;
    void *callback_user_data;

    bool stack_trace_enabled;
    size_t max_stack_depth;

    unsigned int next_checkpoint_id;
} memory_debug_state_t;

/**
 * @brief Magic number constants.
 */
#define MEMORY_DEBUG_MAGIC 0xDEADBEEFCAFEBABEULL

extern memory_debug_state_t g_debug_state;

void memory_debug_lock(void);

void memory_debug_unlock(void);

uint64_t memory_debug_get_timestamp(void);

memory_debug_block_t *memory_debug_find_block(void *user_ptr);

#endif /* AIRY_MEMORY_DEBUG_INTERNAL_H */
