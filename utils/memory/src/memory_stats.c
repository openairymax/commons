// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory_stats.c
 * @brief 内存统计查询/重置/用量（0.1.6 大文件拆分自 memory.c）。
 */

#include "memory_internal.h"

bool memory_get_stats(memory_stats_t *stats)
{
    if (stats == NULL) {
        return false;
    }

    if (!g_state.initialized) {
        __builtin_memset(stats, 0, sizeof(memory_stats_t));
        return true;
    }

    memory_lock();
    __builtin_memcpy(stats, &g_state.stats, sizeof(memory_stats_t));

    if (g_state.debug_enabled) {
        struct memory_debug_info *current = g_state.debug_list_head;
        size_t leak_count = 0;
        while (current != NULL) {
            leak_count++;
            current = current->next;
        }
        stats->leak_count = leak_count;
    }

    memory_unlock();

    return true;
}

void memory_reset_stats(void)
{
    if (!g_state.initialized) {
        return;
    }

    memory_lock();
    __builtin_memset(&g_state.stats, 0, sizeof(memory_stats_t));
    memory_unlock();
}


size_t memory_get_current_usage(void)
{
    if (!g_state.initialized) {
        return 0;
    }

    memory_lock();
    size_t usage = g_state.stats.current_allocated;
    memory_unlock();

    return usage;
}

size_t memory_get_peak_usage(void)
{
    if (!g_state.initialized) {
        return 0;
    }

    memory_lock();
    size_t peak = g_state.stats.peak_allocated;
    memory_unlock();

    return peak;
}