/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * Extended memory statistics & watermark tracking (SEC-15).
 * Split from airy_memory.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_MEMORY_STATS_EXT_H
#define AIRY_RT_MEMORY_STATS_EXT_H

#include "airy_memory_inline.h"
#include "platform_misc.h" /* airy_time_ms (commons facade, 8.2.3 SSoT) */

/**
 * @brief 初始化扩展内存统计跟踪器（SEC-15）
 */
static inline int airy_memory_stats_extended_init(memory_stats_extended_t *ext_stats,
                                                  size_t tracker_capacity)
{
    if (!ext_stats || tracker_capacity == 0) {
        return AIRY_EINVAL;
    }
    AIRY_MEMSET(ext_stats, 0, sizeof(*ext_stats));
    ext_stats->allocation_tracker =
        (alloc_track_entry_t *)AIRY_CALLOC(tracker_capacity, sizeof(alloc_track_entry_t));
    if (!ext_stats->allocation_tracker) {
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    ext_stats->tracker_capacity = tracker_capacity;
    ext_stats->tracker_index = 0;
    ext_stats->tracker_count = 0;
    return 0; /* AIRY_SUCCESS */
}

/* 前向声明 — airy_memory_check_watermark 在下方定义，
 * 但 airy_memory_track_alloc 需要调用它 */
static inline void airy_memory_check_watermark(memory_stats_extended_t *ext_stats);

/**
 * @brief 记录一次内存分配到环形缓冲区（SEC-15）
 */
static inline void airy_memory_track_alloc(memory_stats_extended_t *ext_stats, void *ptr,
                                           size_t size, alloc_category_t category, const char *file,
                                           int line)
{
    if (!ext_stats || !ext_stats->allocation_tracker || !ptr)
        return;

    alloc_track_entry_t *entry = &ext_stats->allocation_tracker[ext_stats->tracker_index];
    entry->ptr = ptr;
    entry->size = size;
    entry->category = category;
    entry->alloc_time = airy_time_ms();
    entry->file = file;
    entry->line = line;
    entry->freed = false;


    ext_stats->alloc_count_by_category[category]++;
    ext_stats->bytes_by_category[category] += size;
    ext_stats->total_allocated += size;
    ext_stats->allocation_count++;
    ext_stats->current_allocated = ext_stats->total_allocated - ext_stats->total_freed;
    if (ext_stats->current_allocated > ext_stats->peak_allocated) {
        ext_stats->peak_allocated = ext_stats->current_allocated;
    }


    ext_stats->tracker_index = (ext_stats->tracker_index + 1) % ext_stats->tracker_capacity;
    if (ext_stats->tracker_count < ext_stats->tracker_capacity) {
        ext_stats->tracker_count++;
    }


    airy_memory_check_watermark(ext_stats);
}

/**
 * @brief 计算当前内存水位级别（SEC-15）
 */
static inline watermark_level_t airy_memory_calc_watermark(memory_stats_extended_t *ext_stats)
{
    if (!ext_stats || ext_stats->total_system_memory == 0) {
        return WATERMARK_NORMAL;
    }
    double usage = (double)ext_stats->current_allocated / (double)ext_stats->total_system_memory;
    if (usage > 0.90)
        return WATERMARK_CRITICAL;
    else if (usage > 0.75)
        return WATERMARK_HIGH;
    else if (usage > 0.60)
        return WATERMARK_WARNING;
    else
        return WATERMARK_NORMAL;
}

/**
 * @brief 检查水位变化并触发回调（SEC-15）
 */
static inline void airy_memory_check_watermark(memory_stats_extended_t *ext_stats)
{
    if (!ext_stats || ext_stats->total_system_memory == 0)
        return;

    watermark_level_t old_level = ext_stats->current_watermark;
    watermark_level_t new_level = airy_memory_calc_watermark(ext_stats);

    if (new_level != old_level) {
        ext_stats->current_watermark = new_level;

        ((void)0) /* fprintf suppressed in strict compliance mode */;

        for (int i = 0; i < MAX_WATERMARK_CALLBACKS; i++) {
            if (ext_stats->watermark_callbacks[i].active &&
                ext_stats->watermark_callbacks[i].callback) {
                ext_stats->watermark_callbacks[i].callback(
                    old_level, new_level, ext_stats->watermark_callbacks[i].context);
            }
        }
    }
}

/**
 * @brief 确定 OOM 响应级别（SEC-15）
 *
 * 完整实现见 oom_handler.h / oom_handler.c（支持五级 FATAL_TERMINATE）。
 * 此处仅保留声明，避免与 oom_handler.h 的 AIRY_API 定义冲突。
 */
oom_response_level_t airy_oom_determine_response(watermark_level_t level);

/**
 * @brief 销毁扩展内存统计跟踪器（SEC-15）
 */
static inline void airy_memory_stats_extended_destroy(memory_stats_extended_t *ext_stats)
{
    if (!ext_stats)
        return;
    airy_free(ext_stats->allocation_tracker);
    ext_stats->allocation_tracker = NULL;
    ext_stats->tracker_capacity = 0;
    ext_stats->tracker_index = 0;
    ext_stats->tracker_count = 0;
}

/** @} */ /* end of airy_memory_tools_api */

#endif /* AIRY_RT_MEMORY_STATS_EXT_H */
