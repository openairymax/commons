/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file airy_memory_types.h
 * @brief Memory subsystem type definitions (SEC-15).
 */

#ifndef AIRY_RT_MEMORY_TYPES_H
#define AIRY_RT_MEMORY_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_api 内存管理API
 * @{
 */

/**
 * @brief 内存分配失败处理策略
 */
typedef enum {
    MEMORY_FAIL_STRATEGY_RETURN_NULL,
    MEMORY_FAIL_STRATEGY_ABORT,
    MEMORY_FAIL_STRATEGY_CALLBACK,
    MEMORY_FAIL_STRATEGY_RETRY
} memory_fail_strategy_t;

/**
 * @brief 内存分配选项
 */
typedef struct {
    size_t alignment;
    bool zero_memory;
    const char *tag;
    memory_fail_strategy_t fail_strategy;
    void (*fail_callback)(size_t size, const char *tag, void *user_data);
    void *fail_callback_user_data;
} memory_options_t;

/**
 * @brief 内存统计信息
 */
#define AIRY_MEMORY_STATS_T_DEFINED
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t leak_count;
} memory_stats_t;

/**
 * @brief 内存调试信息
 */
typedef struct memory_debug_info {
    void *address;
    size_t size;
    size_t alignment;
    const char *tag;
    const char *file;
    int line;
    const char *function;
    uint64_t timestamp;
    struct memory_debug_info *next;
} memory_debug_info_t;

/* IRON-8 合规：消除 airy_memory.h 兼容层定位，所有 API 统一为 airy_memory.h 正式 API。
 * 原 airy_memory.h 已删除，404 个引用文件批量替换为 #include "airy_memory.h"。 */

/* Forward declarations for memory stats reporter (memory_stats_reporter.h) */
extern void airy_mem_stats_record_alloc(size_t bytes);
extern void airy_mem_stats_record_dealloc(size_t bytes);

#ifndef AIRY_MEMORY_STATS_T_DEFINED
#define AIRY_MEMORY_STATS_T_DEFINED
/* 注：memory_stats_t 已在上方 @file 顶部定义（AIRY_MEMORY_STATS_T_DEFINED guard）。
 * 此 guard 保证与原 airy_memory.h 的定义不冲突。 */
#endif

/**
 * @brief 内存分配类别枚举（SEC-15 合规）
 *
 * 用于区分不同生命周期的内存分配
 */
typedef enum {
    ALLOC_SHORT_LIVED = 0,
    ALLOC_LONG_LIVED = 1,
    ALLOC_CRITICAL = 2,
} alloc_category_t;

/**
 * @brief 内存水位级别（SEC-15 合规）
 */
typedef enum {
    WATERMARK_NORMAL = 0,
    WATERMARK_WARNING = 1,
    WATERMARK_HIGH = 2,
    WATERMARK_CRITICAL = 3,
} watermark_level_t;

/**
 * @brief OOM 响应级别（SEC-15 合规）
 */
typedef enum {
    OOM_RESPONSE_WARNING = 0,
    OOM_RESPONSE_DEGRADED = 1,
    OOM_RESPONSE_CRITICAL = 2,
    OOM_RESPONSE_FATAL = 3,
} oom_response_level_t;

/**
 * @brief 内存分配跟踪条目（SEC-15 合规）
 *
 * 环形缓冲区中的每个条目跟踪一次分配的内存块
 */
typedef struct {
    void *ptr;
    size_t size;
    alloc_category_t category;
    uint64_t alloc_time;
    const char *file;
    int line;
    bool freed;
} alloc_track_entry_t;

/**
 * @brief 水位回调函数类型（SEC-15 合规）
 *
 * 每次水位变化时调用。回调函数不应执行长时间操作。
 *
 * @param old_level 之前的水位级别
 * @param new_level 当前的水位级别
 * @param context   回调注册时的用户上下文
 */
typedef void (*watermark_callback_t)(watermark_level_t old_level, watermark_level_t new_level,
                                     void *context);

/**
 * @brief 水位回调注册槽位（SEC-15 合规）
 */
typedef struct {
    watermark_callback_t callback;
    void *context;
    bool active;
} watermark_callback_slot_t;

#define MAX_WATERMARK_CALLBACKS 8
/**
 * @brief 扩展内存统计结构体（SEC-15 合规）
 *
 * 在 memory_stats_t 基础上增加实时追踪能力
 */
typedef struct {

    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t leak_count;


    size_t leak_suspected;
    size_t short_lived_high_water;
    uint64_t last_gc_time;
    size_t gc_freed_bytes;

    size_t alloc_count_by_category[3];
    size_t bytes_by_category[3];

    uint64_t oom_event_count;
    uint64_t last_oom_time;
    size_t last_oom_requested;

    watermark_level_t current_watermark;
    size_t total_system_memory;

    watermark_callback_slot_t watermark_callbacks[MAX_WATERMARK_CALLBACKS];


    alloc_track_entry_t *allocation_tracker;
    size_t tracker_capacity;
    size_t tracker_index;
    size_t tracker_count;
} memory_stats_extended_t;

/**
 * @defgroup airy_memory_tools_api 内存管理工具 API（d9 IRON-8 合规：原 memory_compat.h 已合并删除）
 * @{
 */

/**
 * @defgroup auto_free AUTO_FREE 自动清理（P0.11.2）
 * @{
 *
 * AUTO_FREE 使用 GCC/Clang 的 __attribute__((cleanup)) 实现自动内存释放。
 * 当变量离开作用域时自动调用 airy_free，无需手动释放。
 *
 * 用法：
 *   AUTO_FREE char *buf = AIRY_MALLOC(1024);
 *
 *
 * MSVC 不支持 cleanup 属性，回退到手动释放。
 */

/**
 * @defgroup secure_free 安全内存释放（P0.11.3）
 * @{
 *
 * AIRY_SECURE_FREE 在释放敏感内存前先清零内容，防止敏感数据
 * （密钥、令牌、PII）残留在内存中。
 */

/**
 * @defgroup arena_alloc Arena 短生命周期分配
 * @{
 *
 * P1.19: ALLOC_SHORT_LIVED 类别使用 Arena 分配器。
 */

/**
 * @defgroup safe_memory_alloc 安全内存分配宏（SEC-016合规）
 * @{
 */

/**
 * @defgroup safe_buffer_ops 安全缓冲区操作宏（SEC-01/02/03 合规）
 * @{
 */

/* ==================== P0.18.3: AIRY_MALLOC_GUARD / AIRY_CALLOC_GUARD ==================== */
/**
 * @defgroup malloc_guard RAII 内存分配守卫（P0.18.3）
 * @{
 *
 * AIRY_MALLOC_GUARD / AIRY_CALLOC_GUARD 将 malloc/calloc + NULL 检查 +
 * 自动释放三步合一，消除 `ptr = malloc(size); if (!ptr) return -1;` 样板。
 */

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_MEMORY_TYPES_H */
