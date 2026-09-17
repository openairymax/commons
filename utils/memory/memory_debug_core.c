// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_debug_core.c
 * @brief 内存调试核心：使能/泄漏检查/转储/校验/失败回调（拆分自 memory.c）。
 */

#include "memory_internal.h"

bool memory_debug_enable(bool enable)
{
    if (!g_state.initialized) {
        return false;
    }

    AIRY_LOG_INFO("memory: memory_debug_enable (enable=%s, prev=%s)", enable ? "true" : "false",
             g_state.debug_enabled ? "true" : "false");

    memory_lock();

    if (enable == g_state.debug_enabled) {
        memory_unlock();
        return true;
    }

    g_state.debug_enabled = enable;

    if (!enable && g_state.debug_list_head != NULL) {
        struct memory_debug_info *current = g_state.debug_list_head;
        while (current != NULL) {
            struct memory_debug_info *next = current->next;

            if (current->tag)
                free((void *)current->tag);
            if (current->file)
                free((void *)current->file);
            if (current->function)
                free((void *)current->function);
            free(current);

            current = next;
        }
        g_state.debug_list_head = NULL;
    }

    memory_unlock();

    return true;
}

size_t memory_check_leaks(bool dump_to_stderr)
{
    if (!g_state.initialized || !g_state.debug_enabled) {
        return 0;
    }

    memory_lock();

    size_t leak_size = 0;
    size_t leak_count = 0;
    struct memory_debug_info *current = g_state.debug_list_head;

    struct memory_debug_info *tmp = current;
    while (tmp != NULL) {
        leak_count++;
        tmp = tmp->next;
    }

    if (dump_to_stderr && current != NULL) {
        fprintf(stderr, "=== Memory Leak Detection Report ===\n");
        fprintf(stderr, "Time: %llu\n", (unsigned long long)memory_get_timestamp());
        fprintf(stderr, "Current allocated: %zu bytes\n", g_state.stats.current_allocated);
        fprintf(stderr, "Leak blocks: %zu\n", leak_count);
    }

    while (current != NULL) {
        leak_size += current->size;

        if (dump_to_stderr) {
            fprintf(stderr, "  %p: %zu字节", current->address, current->size);
            if (current->tag) {
                fprintf(stderr, " [%s]", current->tag);
            }
            if (current->file) {
                fprintf(stderr, " (%s:%d)", current->file, current->line);
            }
            fprintf(stderr, "\n");
        }

        current = current->next;
    }

    if (dump_to_stderr && leak_size > 0) {
        fprintf(stderr, "Total leaks: %zu bytes\n", leak_size);
        fprintf(stderr, "========================\n");
    }

    memory_unlock();

    return leak_size;
}

void memory_dump_debug_info(const char *file)
{
    if (!g_state.initialized || !g_state.debug_enabled) {
        return;
    }

    memory_lock();

    FILE *output = file ? fopen(file, "w") : stderr;
    if (output == NULL) {
        memory_unlock();
        return;
    }

    fprintf(output, "=== Memory Debug Info Dump ===\n");
    fprintf(output, "Timestamp: %llu\n", (unsigned long long)memory_get_timestamp());
    fprintf(output, "Current allocation blocks:\n");

    struct memory_debug_info *current = g_state.debug_list_head;
    size_t count = 0;

    while (current != NULL) {
        count++;
        fprintf(output, "  [#%zu]:\n", count);
        fprintf(output, "    address: %p\n", current->address);
        fprintf(output, "    size: %zu bytes\n", current->size);
        fprintf(output, "    tag: %s\n", current->tag ? current->tag : "(null)");
        fprintf(output, "    location: %s:%d (%s)\n", current->file ? current->file : "(unknown)",
                current->line, current->function ? current->function : "(unknown)");
        fprintf(output, "    timestamp: %llu\n", (unsigned long long)current->timestamp);
        fprintf(output, "\n");

        current = current->next;
    }

    fprintf(output, "Total: %zu memory blocks\n", count);
    fprintf(output, "=======================\n");

    if (file) {
        fclose(output);
    }

    memory_unlock();
}

bool memory_validate(void *ptr)
{
    if (!g_state.initialized || !g_state.debug_enabled || ptr == NULL) {
        return true;
    }

    memory_lock();

    bool valid = (memory_find_debug_info(ptr) != NULL);

    memory_unlock();

    return valid;
}

void memory_set_fail_callback(void (*callback)(size_t size, const char *tag, void *user_data),
                              void *user_data)
{

    if (!g_state.initialized) {
        return;
    }

    memory_lock();

    g_state.fail_callback = callback;
    g_state.fail_callback_user_data = user_data;

    memory_unlock();
}