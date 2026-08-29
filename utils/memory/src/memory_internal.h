/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file memory_internal.h
 * @brief 内存子系统内部共享定义（0.1.6 大文件拆分）。
 *
 * 拆分自 memory.c（统一内存管理核心层），跨文件共享：
 *   - g_state 全局状态机
 *   - 锁原语（memory_lock*）
 *   - 调试信息查找（memory_find_debug_info）
 *
 * 文件归属：
 *   - memory_core.c      状态机/锁/内部分配/生命周期/alloc-free
 *   - memory_stats.c     统计查询/重置/用量
 *   - memory_debug_core.c 调试使能/泄漏检查/转储/校验/失败回调
 */

#ifndef AIRY_RT_MEMORY_INTERNAL_H
#define AIRY_RT_MEMORY_INTERNAL_H

#include "../include/airy_memory.h"
#include "platform.h"
#include "logging.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool initialized;
    bool debug_enabled;
    memory_options_t options;

    memory_stats_t stats;

    /* Thread synchronization */
    airy_mtx_t lock;

    struct memory_debug_info *debug_list_head;

    void (*fail_callback)(size_t size, const char *tag, void *user_data);
    void *fail_callback_user_data;
} memory_state_t;

extern memory_state_t g_state;

bool memory_lock_init(void);
void memory_lock_destroy(void);
void memory_lock(void);
void memory_unlock(void);
struct memory_debug_info *memory_find_debug_info(void *addr);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_MEMORY_INTERNAL_H */
