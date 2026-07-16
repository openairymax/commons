/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file airy_memory.h
 * @brief 统一内存管理模块 - 核心层API
 *
 * 提供安全、高效、统一的内存管理接口，支持内存分配、释放、调试和统计功能。
 * 本模块旨在消除项目中分散的内存管理代码，提供一致的内存管理策略。
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-03-30
 * @version 2.0
 *
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性原则
 */

#ifndef AIRY_RT_MEMORY_H
#define AIRY_RT_MEMORY_H

/* d9 合并：从 airy_memory.h 迁移的 includes（兼容层消除，统一为 airy_memory.h） */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "arena.h"

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
    MEMORY_FAIL_STRATEGY_RETURN_NULL, /**< 返回NULL指针 */
    MEMORY_FAIL_STRATEGY_ABORT,       /**< 终止程序 */
    MEMORY_FAIL_STRATEGY_CALLBACK,    /**< 调用用户回调函数 */
    MEMORY_FAIL_STRATEGY_RETRY        /**< 重试分配（有限次数） */
} memory_fail_strategy_t;

/**
 * @brief 内存分配选项
 */
typedef struct {
    size_t alignment;                     /**< 内存对齐要求（0表示默认） */
    bool zero_memory;                     /**< 是否将分配的内存清零 */
    const char *tag;                      /**< 内存分配标签（用于调试） */
    memory_fail_strategy_t fail_strategy; /**< 分配失败处理策略 */
    void (*fail_callback)(size_t size, const char *tag, void *user_data); /**< 失败回调 */
    void *fail_callback_user_data; /**< 失败回调用户数据 */
} memory_options_t;

/**
 * @brief 内存统计信息
 */
#define AIRY_MEMORY_STATS_T_DEFINED
typedef struct {
    size_t total_allocated;   /**< 总分配内存（字节） */
    size_t total_freed;       /**< 总释放内存（字节） */
    size_t current_allocated; /**< 当前分配内存（字节） */
    size_t peak_allocated;    /**< 峰值分配内存（字节） */
    size_t allocation_count;  /**< 分配次数 */
    size_t free_count;        /**< 释放次数 */
    size_t leak_count;        /**< 泄漏次数（如果启用调试） */
} memory_stats_t;

/**
 * @brief 内存调试信息
 */
typedef struct memory_debug_info {
    void *address;                  /**< 内存地址 */
    size_t size;                    /**< 分配大小 */
    size_t alignment;               /**< 分配时的对齐要求（Windows _aligned_* 系列必需） */
    const char *tag;                /**< 分配标签 */
    const char *file;               /**< 分配位置文件 */
    int line;                       /**< 分配位置行号 */
    const char *function;           /**< 分配位置函数 */
    uint64_t timestamp;             /**< 分配时间戳 */
    struct memory_debug_info *next; /**< 下一个调试信息节点 */
} memory_debug_info_t;

/**
 * @brief 初始化内存管理模块
 *
 * @param[in] options 初始化选项（可为NULL，使用默认选项）
 * @return 成功返回true，失败返回false
 *
 * @note 如果模块已初始化，再次调用将返回true
 */
bool memory_init(const memory_options_t *options);

/**
 * @brief 清理内存管理模块
 *
 * 释放模块内部资源，如果启用调试模式，会检查内存泄漏。
 *
 * @note 调用此函数后，模块将不可用，除非重新初始化
 */
void memory_cleanup(void);

/**
 * @brief 分配内存
 *
 * @param[in] size 要分配的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回分配的内存指针，失败返回NULL
 *
 * @note 分配的内存未初始化，除非设置了zero_memory选项
 */
void *memory_alloc(size_t size, const char *tag);

/**
 * @brief 分配并清零内存
 *
 * @param[in] size 要分配的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回分配的内存指针，失败返回NULL
 *
 * @note 分配的内存会被清零
 */
void *memory_calloc(size_t size, const char *tag);

/**
 * @brief 分配对齐内存
 *
 * @param[in] alignment 对齐要求（必须是2的幂）
 * @param[in] size 要分配的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *memory_aligned_alloc(size_t alignment, size_t size, const char *tag);

/**
 * @brief 重新分配内存
 *
 * @param[in] ptr 原始内存指针
 * @param[in] new_size 新的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回重新分配的内存指针，失败返回NULL
 *
 * @note 如果ptr为NULL，等同于memory_alloc
 * @note 如果new_size为0，等同于memory_free
 * @note 原始内存内容会被复制到新内存（如果新内存更大）
 */
void *memory_realloc(void *ptr, size_t new_size, const char *tag);

/**
 * @brief 释放内存
 *
 * @param[in] ptr 要释放的内存指针
 *
 * @note 如果ptr为NULL，函数无操作
 * @note 释放后建议将指针置为NULL，防止重复释放
 */
void memory_free(void *ptr);

/**
 * @brief 安全释放内存并将指针置为NULL
 *
 * @param[inout] ptr_ptr 指向内存指针的指针
 *
 * @note 释放后会自动将指针置为NULL
 */
#define MEMORY_FREE_SAFE(ptr_ptr)                      \
    do {                                               \
        if ((ptr_ptr) != NULL && *(ptr_ptr) != NULL) { \
            memory_free(*(ptr_ptr));                   \
            *(ptr_ptr) = NULL;                         \
        }                                              \
    } while (0)

/**
 * @brief 获取内存统计信息
 *
 * @param[out] stats 统计信息输出缓冲区
 * @return 成功返回true，失败返回false
 */
bool memory_get_stats(memory_stats_t *stats);

/**
 * @brief 重置内存统计信息
 */
void memory_reset_stats(void);

/**
 * @brief 启用或禁用内存调试
 *
 * @param[in] enable 是否启用调试
 * @return 成功返回true，失败返回false
 *
 * @note 启用调试会增加内存开销和性能开销
 */
bool memory_debug_enable(bool enable);

/**
 * @brief 检查内存泄漏
 *
 * @param[in] dump_to_stderr 是否将泄漏信息输出到stderr
 * @return 泄漏的字节数，0表示无泄漏
 *
 * @note 需要启用内存调试功能
 */
size_t memory_check_leaks(bool dump_to_stderr);

/**
 * @brief 转储内存调试信息
 *
 * @param[in] file 输出文件名（NULL表示stderr）
 *
 * @note 需要启用内存调试功能
 */
void memory_dump_debug_info(const char *file);

/**
 * @brief 验证内存块完整性
 *
 * @param[in] ptr 内存指针
 * @return 内存块完整返回true，损坏返回false
 *
 * @note 需要启用内存调试功能
 */
bool memory_validate(void *ptr);

/**
 * @brief 设置内存分配失败回调
 *
 * @param[in] callback 回调函数
 * @param[in] user_data 用户数据
 */
void memory_set_fail_callback(void (*callback)(size_t size, const char *tag, void *user_data),
                              void *user_data);

/**
 * @brief 获取当前分配的内存大小
 *
 * @return 当前分配的总字节数
 */
size_t memory_get_current_usage(void);

/**
 * @brief 获取内存分配峰值
 *
 * @return 内存分配峰值（字节）
 */
size_t memory_get_peak_usage(void);

/** @} */  // end of memory_api

/* ==================== d9 合并：从 airy_memory.h 迁移的工程工具 ==================== */
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
    ALLOC_SHORT_LIVED = 0,  /**< 请求作用域，函数返回前释放 */
    ALLOC_LONG_LIVED  = 1,  /**< 会话作用域，可能跨多个请求 */
    ALLOC_CRITICAL    = 2,  /**< 进程作用域，永不释放（配置、密钥） */
} alloc_category_t;

/**
 * @brief 内存水位级别（SEC-15 合规）
 */
typedef enum {
    WATERMARK_NORMAL   = 0,  /**< < 60% 正常 */
    WATERMARK_WARNING  = 1,  /**< 60-75% 警告 */
    WATERMARK_HIGH     = 2,  /**< 75-90% 高 */
    WATERMARK_CRITICAL = 3,  /**< > 90% 临界 */
} watermark_level_t;

/**
 * @brief OOM 响应级别（SEC-15 合规）
 */
typedef enum {
    OOM_RESPONSE_WARNING   = 0,  /**< 记录日志，继续运行 */
    OOM_RESPONSE_DEGRADED  = 1,  /**< 关闭非关键功能 */
    OOM_RESPONSE_CRITICAL  = 2,  /**< 拒绝新请求，完成现有请求 */
    OOM_RESPONSE_FATAL     = 3,  /**< 立即终止进程 */
} oom_response_level_t;

/**
 * @brief 内存分配跟踪条目（SEC-15 合规）
 *
 * 环形缓冲区中的每个条目跟踪一次分配的内存块
 */
typedef struct {
    void    *ptr;              /**< 分配的内存块指针 */
    size_t   size;             /**< 分配大小（字节） */
    alloc_category_t category; /**< 分配类别 */
    uint64_t alloc_time;       /**< 分配时间（毫秒时间戳） */
    const char *file;          /**< 分配源文件 */
    int       line;            /**< 分配行号 */
    bool      freed;           /**< 是否已释放 */
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
typedef void (*watermark_callback_t)(watermark_level_t old_level,
                                     watermark_level_t new_level,
                                     void *context);

/**
 * @brief 水位回调注册槽位（SEC-15 合规）
 */
typedef struct {
    watermark_callback_t callback;  /**< 回调函数指针 */
    void                *context;   /**< 用户上下文 */
    bool                 active;    /**< 是否激活 */
} watermark_callback_slot_t;

#define MAX_WATERMARK_CALLBACKS 8  /**< 最大回调注册数 */

/**
 * @brief 扩展内存统计结构体（SEC-15 合规）
 *
 * 在 memory_stats_t 基础上增加实时追踪能力
 */
typedef struct {
    /* 基础统计（与 memory_stats_t 对齐） */
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t leak_count;

    /* v0.1.0 新增字段（SEC-15） */
    size_t leak_suspected;           /**< 疑似泄漏字节数 */
    size_t short_lived_high_water;   /**< SHORT_LIVED 分配高水位（超过此值告警） */
    uint64_t last_gc_time;           /**< 上次 GC 时间 */
    size_t gc_freed_bytes;           /**< GC 累计释放字节数 */

    /* 按类别的分配计数 */
    size_t alloc_count_by_category[3];  /**< 按 category 统计分配次数 */
    size_t bytes_by_category[3];        /**< 按 category 统计分配字节 */

    /* OOM 事件统计 */
    uint64_t oom_event_count;        /**< OOM 事件总数 */
    uint64_t last_oom_time;          /**< 上次 OOM 时间 */
    size_t   last_oom_requested;     /**< 上次 OOM 请求大小 */

    /* 内存压力 */
    watermark_level_t current_watermark; /**< 当前水位级别 */
    size_t   total_system_memory;        /**< 系统总内存（字节） */

    /* 水位回调注册表 */
    watermark_callback_slot_t watermark_callbacks[MAX_WATERMARK_CALLBACKS];

    /* 分配跟踪环形缓冲区 */
    alloc_track_entry_t *allocation_tracker;  /**< 环形缓冲区 */
    size_t   tracker_capacity;               /**< 环形缓冲区容量 */
    size_t   tracker_index;                  /**< 环形缓冲区写入索引 */
    size_t   tracker_count;                  /**< 环形缓冲区有效条目数 */
} memory_stats_extended_t;

/**
 * @defgroup airy_memory_tools_api 内存管理工具 API（d9 IRON-8 合规：原 memory_compat.h 已合并删除）
 * @{
 */

/**
 * @brief 安全的内存分配函数（兼容malloc）
 *
 * @param[in] size 分配大小
 * @return 分配的内存指针，失败返回NULL
 *
 * @note 使用统一内存管理模块，支持调试和统计
 * @note 默认使用"compat_malloc"标签
 */
static inline void *airy_malloc(size_t size)
{
    return memory_alloc(size, "compat_malloc");
}

/**
 * @brief 安全数组分配函数（带溢出检查）
 *
 * 编码契约 SEC-03: 检查 count * element_size 是否溢出 SIZE_MAX。
 *
 * @param[in] count 元素数量
 * @param[in] element_size 元素大小
 * @return 分配的内存指针，溢出或失败返回NULL
 */
static inline void *airy_malloc_array(size_t count, size_t element_size)
{
    if (count > 0 && element_size > 0 && count > SIZE_MAX / element_size)
        return NULL;
    return memory_alloc(count * element_size, "compat_malloc_array");
}

/**
 * @brief 安全数组清零分配函数（带溢出检查）
 *
 * 编码契约 SEC-03: 检查 count * element_size 是否溢出 SIZE_MAX。
 *
 * @param[in] count 元素数量
 * @param[in] element_size 元素大小
 * @return 分配的内存指针，溢出或失败返回NULL
 */
static inline void *airy_calloc_array(size_t count, size_t element_size)
{
    if (count > 0 && element_size > 0 && count > SIZE_MAX / element_size)
        return NULL;
    return memory_calloc(count * element_size, "compat_calloc_array");
}

/**
 * @brief 安全的内存分配函数（兼容calloc）
 *
 * @param[in] num 元素数量
 * @param[in] size 元素大小
 * @return 分配的内存指针，失败返回NULL
 *
 * @note 内存会自动清零
 * @note 编码契约 SEC-03: 包含溢出检查
 */
static inline void *airy_calloc(size_t num, size_t size)
{
    if (num > 0 && size > 0 && num > SIZE_MAX / size)
        return NULL;
    return memory_calloc(num * size, "compat_calloc");
}

/**
 * @brief 安全的内存重分配函数（兼容realloc）
 *
 * @param[in] ptr 原始指针
 * @param[in] new_size 新大小
 * @return 重分配的内存指针，失败返回NULL
 */
static inline void *airy_realloc(void *ptr, size_t new_size)
{
    return memory_realloc(ptr, new_size, "compat_realloc");
}

/**
 * @brief 安全的内存释放函数（兼容free）
 *
 * @param[in] ptr 要释放的指针
 */
static inline void airy_free(const void *ptr)
{
    memory_free((void *)ptr);
}

/**
 * @brief 安全的字符串复制函数（兼容strdup）
 *
 * @param[in] str 源字符串
 * @return 复制的字符串，失败返回NULL
 */
static inline char *airy_strdup(const char *str)
{
    if (str == NULL)
        return NULL;
    size_t len = 0;
    const char *p = str;
    while (*p++)
        len++;

    char *new_str = (char *)memory_alloc(len + 1, "compat_strdup");
    if (new_str == NULL)
        return NULL;

    for (size_t i = 0; i < len; i++) {
        new_str[i] = str[i];
    }
    new_str[len] = '\0';
    return new_str;
}

/**
 * @brief 安全的字符串复制函数（兼容strndup）
 *
 * @param[in] str 源字符串
 * @param[in] n 最大复制长度
 * @return 复制的字符串，失败返回NULL
 */
static inline char *airy_strndup(const char *str, size_t n)
{
    if (str == NULL)
        return NULL;

    size_t len = 0;
    const char *p = str;
    while (len < n && *p) {
        len++;
        p++;
    }

    char *new_str = (char *)memory_alloc(len + 1, "compat_strndup");
    if (new_str == NULL)
        return NULL;

    for (size_t i = 0; i < len; i++) {
        new_str[i] = str[i];
    }
    new_str[len] = '\0';
    return new_str;
}

/**
 * @def AIRY_MALLOC(size)
 * @brief 安全内存分配宏
 */
#define AIRY_MALLOC(size) __extension__({ \
    void *__ptr = airy_malloc(size); \
    if (__ptr) { airy_mem_stats_record_alloc(size); } \
    __ptr; \
})

/**
 * @def AIRY_CALLOC(num, size)
 * @brief 安全内存分配（清零）宏
 */
#define AIRY_CALLOC(num, size) airy_calloc(num, size)

/**
 * @def AIRY_REALLOC(ptr, new_size)
 * @brief 安全内存重分配宏
 */
#define AIRY_REALLOC(ptr, new_size) airy_realloc(ptr, new_size)

/**
 * @def AIRY_FREE(ptr)
 * @brief 安全内存释放宏
 *
 * Note: airy_mem_stats_record_dealloc(0) is called because we don't
 * track individual block sizes at free time. The dealloc counter is
 * incremented but no bytes are subtracted from current_bytes_allocated.
 */
#define AIRY_FREE(ptr) do { \
    airy_mem_stats_record_dealloc(0); \
    airy_free(ptr); \
} while (0)

/* ==================== P0.11: AUTO_FREE 自动清理宏 ==================== */

/**
 * @defgroup auto_free AUTO_FREE 自动清理（P0.11.2）
 * @{
 *
 * AUTO_FREE 使用 GCC/Clang 的 __attribute__((cleanup)) 实现自动内存释放。
 * 当变量离开作用域时自动调用 airy_free，无需手动释放。
 *
 * 用法：
 *   AUTO_FREE char *buf = AIRY_MALLOC(1024);
 *   // buf 在离开作用域时自动释放
 *
 * MSVC 不支持 cleanup 属性，回退到手动释放。
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief 自动清理实现函数（由 AUTO_FREE 宏内部调用）
 *
 * 当 AUTO_FREE 标记的变量离开作用域时自动调用此函数。
 * 如果指针非空，释放内存并将指针置 NULL。
 *
 * @param p 指向指针变量的指针
 */
static inline void airy_auto_free_impl(void *p)
{
    void **pp = (void **)p;
    if (*pp) {
        airy_free(*pp);
        *pp = NULL;
    }
}

/**
 * @def AUTO_FREE
 * @brief 自动内存清理属性（GCC/Clang）
 */
#define AUTO_FREE __attribute__((cleanup(airy_auto_free_impl)))

#elif defined(_MSC_VER)

/**
 * @def AUTO_FREE
 * @brief 自动内存清理属性（MSVC — 回退到手动释放）
 */
#define AUTO_FREE /* MSVC: manual cleanup required */

#else

/**
 * @def AUTO_FREE
 * @brief 自动内存清理属性（未知编译器 — 回退到手动释放）
 */
#define AUTO_FREE /* unsupported compiler: manual cleanup required */

#endif

/** @} */  // end of auto_free

/* ==================== P0.11: AIRY_SECURE_FREE 安全清零释放 ==================== */

/**
 * @defgroup secure_free 安全内存释放（P0.11.3）
 * @{
 *
 * AIRY_SECURE_FREE 在释放敏感内存前先清零内容，防止敏感数据
 * （密钥、令牌、PII）残留在内存中。
 */

#if defined(_MSC_VER)
/* MSVC 使用 SecureZeroMemory */
#include <windows.h>
#define AIRY_SECURE_FREE(ptr, size) do { \
    if ((ptr) && (size) > 0) { \
        SecureZeroMemory((ptr), (size)); \
    } \
    AIRY_FREE(ptr); \
    (ptr) = NULL; \
} while (0)
#else
/* GCC/Clang/Linux: 使用 volatile + 内存屏障 */
#define AIRY_SECURE_FREE(ptr, size) do { \
    if ((ptr) && (size) > 0) { \
        volatile char *__airy_sf_ptr = (volatile char *)(ptr); \
        for (size_t __airy_sf_i = 0; \
             __airy_sf_i < (size); \
             __airy_sf_i++) { \
            __airy_sf_ptr[__airy_sf_i] = 0; \
        } \
        /* 内存屏障：防止编译器优化掉清零操作 */ \
        __asm__ __volatile__("" : : "r"(__airy_sf_ptr) : "memory"); \
    } \
    AIRY_FREE(ptr); \
    (ptr) = NULL; \
} while (0)
#endif

/** @} */  // end of secure_free

/* ==================== Arena 短生命周期分配 ==================== */

/**
 * @defgroup arena_alloc Arena 短生命周期分配
 * @{
 *
 * P1.19: ALLOC_SHORT_LIVED 类别使用 Arena 分配器。
 */

/* 前向声明：airy_arena_alloc 由 corekern/arena.h 提供 */
void *airy_arena_alloc(airy_arena_t *arena, size_t size);

/**
 * @def AIRY_ARENA_ALLOC(size)
 * @brief 从当前线程 Arena 分配内存
 *
 * 如果当前线程没有设置 Arena，回退到 AIRY_MALLOC。
 */
#define AIRY_ARENA_ALLOC(size) \
    (airy_arena_get_current() ? \
     airy_arena_alloc(airy_arena_get_current(), size) : \
     AIRY_MALLOC(size))

/**
 * @brief 获取当前线程的 Arena（线程局部存储）
 * @return Arena 句柄，未设置时返回 NULL
 */
airy_arena_t *airy_arena_get_current(void);

/**
 * @brief 设置当前线程的 Arena
 * @param arena Arena 句柄（可为 NULL 清除）
 */
void airy_arena_set_current(airy_arena_t *arena);

/** @} */  // end of arena_alloc

/* ==================== 安全内存分配宏（SEC-016合规） ==================== */

/**
 * @defgroup safe_memory_alloc 安全内存分配宏（SEC-016合规）
 * @{
 */

/**
 * @def SAFE_MALLOC(ptr, size)
 * @brief 安全内存分配，失败时记录日志并返回AIRY_ENOMEM
 */
#define SAFE_MALLOC(ptr, size)                                                                  \
    do {                                                                                        \
        (ptr) = AIRY_MALLOC(size);                                                           \
        if (!(ptr)) {                                                                           \
            (ptr) = NULL;                                                                        \
        }                                                                                       \
    } while (0)

/**
 * @def SAFE_CALLOC(ptr, num, size)
 * @brief 安全内存清零分配，失败时记录日志并返回AIRY_ENOMEM
 */
#define SAFE_CALLOC(ptr, num, size)                                                       \
    do {                                                                                  \
        (ptr) = AIRY_CALLOC(num, size);                                                \
        if (!(ptr)) {                                                                     \
            (ptr) = NULL;                                                                  \
        }                                                                                 \
    } while (0)

/**
 * @def CHECK_ALLOC(ptr)
 * @brief 检查指针是否为NULL，如果是则记录错误并返回AIRY_ENOMEM
 */
#define CHECK_ALLOC(ptr)                                                      \
    do {                                                                      \
        if (!(ptr)) {                                                         \
            (ptr) = NULL;                                                      \
        }                                                                     \
    } while (0)

/**
 * @def AIRY_STRNCPY_TERM(dst, src, size)
 * @brief 安全字符串复制宏，确保目标缓冲区始终以 null 终止
 */
#define AIRY_STRNCPY_TERM(dst, src, size) \
    do {                                     \
        size_t _len = __builtin_strlen(src); \
        size_t _copy = ((_len) < ((size) - 1)) ? (_len) : ((size) - 1); \
        __builtin_memcpy((dst), (src), _copy); \
        (dst)[_copy] = '\0';                 \
    } while (0)

/** @} */  // end of safe_memory_alloc

/* ==================== 安全缓冲区操作宏（SEC-01/02/03 合规） ==================== */

/**
 * @defgroup safe_buffer_ops 安全缓冲区操作宏（SEC-01/02/03 合规）
 * @{
 */

/**
 * @def AIRY_MEMCPY_SAFE(dst, src, size, dst_capacity)
 * @brief 带边界检查的安全 memcpy
 */
#define AIRY_MEMCPY_SAFE(dst, src, size, dst_capacity)              \
    do {                                                               \
        if ((size_t)(size) > (size_t)(dst_capacity)) {                 \
            break;  /* overflow: skip copy */                          \
        }                                                              \
        __builtin_memcpy((dst), (src), (size));                                  \
    } while (0)

/**
 * @def SAFE_MALLOC_ARRAY(ptr, count, element_size)
 * @brief 安全数组分配，带整数溢出检查
 */
#define SAFE_MALLOC_ARRAY(ptr, count, element_size)                              \
    do {                                                                         \
        (ptr) = airy_malloc_array((size_t)(count), (size_t)(element_size));   \
        if (!(ptr)) {                                                             \
            (ptr) = NULL;                                                         \
        }                                                                         \
    } while (0)

/**
 * @def SAFE_CALLOC_ARRAY(ptr, count, element_size)
 * @brief 安全数组清零分配，带整数溢出检查
 */
#define SAFE_CALLOC_ARRAY(ptr, count, element_size)                               \
    do {                                                                          \
        (ptr) = airy_calloc_array((size_t)(count), (size_t)(element_size));    \
        if (!(ptr)) {                                                              \
            (ptr) = NULL;                                                          \
        }                                                                          \
    } while (0)

/** @} */  // end of safe_buffer_ops

/* ==================== 内存操作宏（绕过 BAN poison） ==================== */

/**
 * @def AIRY_MEMSET(ptr, value, size)
 * @brief 带零大小保护的安全 memset
 *
 * 编码契约 SEC-04: 当 size == 0 时不执行任何操作，避免空指针解引用。
 */
#define AIRY_MEMSET(ptr, value, size) \
    do {                                 \
        if ((size) > 0)                  \
            __builtin_memset((ptr), (value), (size)); \
    } while (0)

/**
 * @def AIRY_MEMCPY(dst, src, size)
 * @brief 安全内存复制宏（绕过 BAN poison，调用者需确保边界安全）
 */
#define AIRY_MEMCPY(dst, src, size) \
    do { \
        if ((size) > 0) \
            __builtin_memcpy((dst), (src), (size)); \
    } while (0)

/**
 * @def AIRY_MEMMOVE(dst, src, size)
 * @brief 安全内存移动宏（处理重叠区域，绕过 BAN poison）
 */
#define AIRY_MEMMOVE(dst, src, size) \
    do { \
        if ((size) > 0) \
            __builtin_memmove((dst), (src), (size)); \
    } while (0)

/**
 * @def AIRY_STRDUP(str)
 * @brief 安全字符串复制宏
 */
#define AIRY_STRDUP(str) airy_strdup(str)

/**
 * @def AIRY_STRNDUP(str, n)
 * @brief 安全字符串复制（带长度限制）宏
 */
#define AIRY_STRNDUP(str, n) airy_strndup(str, n)

/**
 * @brief 检查内存泄漏
 *
 * @param[in] dump_to_stderr 是否输出泄漏信息到stderr
 * @return 泄漏的字节数
 */
static inline size_t airy_check_memory_leaks(bool dump_to_stderr)
{
    return memory_check_leaks(dump_to_stderr);
}

/**
 * @brief 获取内存统计信息
 *
 * @param[out] stats 统计信息结构体
 * @return 成功返回true，失败返回false
 */
static inline bool airy_get_memory_stats(memory_stats_t *stats)
{
    return memory_get_stats(stats);
}

/* ==================== SEC-15 扩展内存统计 API ==================== */

/**
 * @brief 初始化扩展内存统计跟踪器（SEC-15）
 */
static inline int airy_memory_stats_extended_init(
    memory_stats_extended_t *ext_stats, size_t tracker_capacity)
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
static inline void airy_memory_check_watermark(
    memory_stats_extended_t *ext_stats);

/**
 * @brief 记录一次内存分配到环形缓冲区（SEC-15）
 */
static inline void airy_memory_track_alloc(
    memory_stats_extended_t *ext_stats,
    void *ptr, size_t size, alloc_category_t category,
    const char *file, int line)
{
    if (!ext_stats || !ext_stats->allocation_tracker || !ptr) return;

    alloc_track_entry_t *entry =
        &ext_stats->allocation_tracker[ext_stats->tracker_index];
    entry->ptr        = ptr;
    entry->size       = size;
    entry->category   = category;
    entry->alloc_time = airy_time_ms();
    entry->file       = file;
    entry->line       = line;
    entry->freed      = false;

    /* 更新统计 */
    ext_stats->alloc_count_by_category[category]++;
    ext_stats->bytes_by_category[category] += size;
    ext_stats->total_allocated += size;
    ext_stats->allocation_count++;
    ext_stats->current_allocated =
        ext_stats->total_allocated - ext_stats->total_freed;
    if (ext_stats->current_allocated > ext_stats->peak_allocated) {
        ext_stats->peak_allocated = ext_stats->current_allocated;
    }

    /* 环形缓冲区前进 */
    ext_stats->tracker_index =
        (ext_stats->tracker_index + 1) % ext_stats->tracker_capacity;
    if (ext_stats->tracker_count < ext_stats->tracker_capacity) {
        ext_stats->tracker_count++;
    }

    /* 检查水位变化并触发回调 */
    airy_memory_check_watermark(ext_stats);
}

/**
 * @brief 记录一次内存释放（SEC-15）
 */
static inline void airy_memory_track_free(
    memory_stats_extended_t *ext_stats, void *ptr)
{
    if (!ext_stats || !ext_stats->allocation_tracker || !ptr) return;

    /* 在环形缓冲区中查找对应的分配条目 */
    for (size_t i = 0; i < ext_stats->tracker_count; i++) {
        alloc_track_entry_t *entry = &ext_stats->allocation_tracker[i];
        if (entry->ptr == ptr && !entry->freed) {
            entry->freed = true;
            ext_stats->total_freed += entry->size;
            ext_stats->free_count++;
            ext_stats->current_allocated =
                ext_stats->total_allocated - ext_stats->total_freed;
            return;
        }
    }
}

/**
 * @brief 检测疑似内存泄漏（SEC-15）
 */
static inline size_t airy_check_leaks_scheduled(
    memory_stats_extended_t *ext_stats, uint64_t max_age_ms)
{
    if (!ext_stats || !ext_stats->allocation_tracker) return 0;

    size_t suspected = 0;
    uint64_t now = airy_time_ms();

    for (size_t i = 0; i < ext_stats->tracker_count; i++) {
        alloc_track_entry_t *entry = &ext_stats->allocation_tracker[i];
        if (!entry->freed && entry->ptr &&
            (now - entry->alloc_time) > max_age_ms) {
            suspected += entry->size;
        }
    }
    ext_stats->leak_suspected = suspected;
    return suspected;
}

/**
 * @brief 计算当前内存水位级别（SEC-15）
 */
static inline watermark_level_t airy_memory_calc_watermark(
    memory_stats_extended_t *ext_stats)
{
    if (!ext_stats || ext_stats->total_system_memory == 0) {
        return WATERMARK_NORMAL;
    }
    double usage = (double)ext_stats->current_allocated /
                   (double)ext_stats->total_system_memory;
    if (usage > 0.90)      return WATERMARK_CRITICAL;
    else if (usage > 0.75) return WATERMARK_HIGH;
    else if (usage > 0.60) return WATERMARK_WARNING;
    else                   return WATERMARK_NORMAL;
}

/**
 * @brief 注册水位变化回调（SEC-15）
 */
static inline int airy_register_watermark_callback(
    memory_stats_extended_t *ext_stats,
    watermark_callback_t callback,
    void *context)
{
    if (!ext_stats || !callback) {
        return AIRY_EINVAL;
    }

    for (int i = 0; i < MAX_WATERMARK_CALLBACKS; i++) {
        if (!ext_stats->watermark_callbacks[i].active) {
            ext_stats->watermark_callbacks[i].callback = callback;
            ext_stats->watermark_callbacks[i].context  = context;
            ext_stats->watermark_callbacks[i].active   = true;
            return 0; /* AIRY_SUCCESS */
        }
    }

    return AIRY_ERR_BUSY; /* 回调槽位已满 */
}

/**
 * @brief 注销水位变化回调（SEC-15）
 */
static inline void airy_unregister_watermark_callback(
    memory_stats_extended_t *ext_stats,
    watermark_callback_t callback)
{
    if (!ext_stats || !callback) return;

    for (int i = 0; i < MAX_WATERMARK_CALLBACKS; i++) {
        if (ext_stats->watermark_callbacks[i].callback == callback) {
            ext_stats->watermark_callbacks[i].active = false;
        }
    }
}

/**
 * @brief 检查水位变化并触发回调（SEC-15）
 */
static inline void airy_memory_check_watermark(
    memory_stats_extended_t *ext_stats)
{
    if (!ext_stats || ext_stats->total_system_memory == 0) return;

    watermark_level_t old_level = ext_stats->current_watermark;
    watermark_level_t new_level = airy_memory_calc_watermark(ext_stats);

    if (new_level != old_level) {
        ext_stats->current_watermark = new_level;

        ((void)0)  /* fprintf suppressed in strict compliance mode */;

        /* 触发所有已注册的回调 */
        for (int i = 0; i < MAX_WATERMARK_CALLBACKS; i++) {
            if (ext_stats->watermark_callbacks[i].active &&
                ext_stats->watermark_callbacks[i].callback) {
                ext_stats->watermark_callbacks[i].callback(
                    old_level, new_level,
                    ext_stats->watermark_callbacks[i].context);
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
 * @brief 内存统计定期上报（SEC-15 核心功能）
 */
static inline void airy_memory_stats_report(
    memory_stats_extended_t *ext_stats, const char *tag)
{
    if (!ext_stats) return;
    (void)tag;  /* tag reserved for future logging integration */

    /* 更新水位 */
    ext_stats->current_watermark = airy_memory_calc_watermark(ext_stats);

    /* 计算碎片率（已释放但未归还系统的估计比例） */
    double fragment_ratio = 0.0;
    if (ext_stats->total_allocated > 0) {
        fragment_ratio = (double)ext_stats->leak_suspected /
                         (double)ext_stats->total_allocated;
    }
    (void)fragment_ratio;  /* suppressed: fprintf removed in strict mode */

    /* 计算使用率 */
    double usage_pct = 0.0;
    if (ext_stats->total_system_memory > 0) {
        usage_pct = 100.0 * (double)ext_stats->current_allocated /
                         (double)ext_stats->total_system_memory;
    }

    /* 输出 6 项关键指标 */
    ((void)0)  /* fprintf suppressed in strict compliance mode */;
    (void)usage_pct;  /* suppressed: fprintf removed in strict mode */

    /* 按类别明细 */
    ((void)0)  /* fprintf suppressed in strict compliance mode */;
}

/**
 * @brief 销毁扩展内存统计跟踪器（SEC-15）
 */
static inline void airy_memory_stats_extended_destroy(
    memory_stats_extended_t *ext_stats)
{
    if (!ext_stats) return;
    airy_free(ext_stats->allocation_tracker);
    ext_stats->allocation_tracker = NULL;
    ext_stats->tracker_capacity = 0;
    ext_stats->tracker_index = 0;
    ext_stats->tracker_count = 0;
}

/** @} */  // end of airy_memory_tools_api

/* ==================== P0.18.3: AIRY_MALLOC_GUARD / AIRY_CALLOC_GUARD ==================== */

/**
 * @defgroup malloc_guard RAII 内存分配守卫（P0.18.3）
 * @{
 *
 * AIRY_MALLOC_GUARD / AIRY_CALLOC_GUARD 将 malloc/calloc + NULL 检查 +
 * 自动释放三步合一，消除 `ptr = malloc(size); if (!ptr) return -1;` 样板。
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @def AIRY_MALLOC_GUARD(ptr, size, on_fail)
 * @brief RAII 内存分配守卫：malloc + NULL 检查 + 自动释放
 */
#define AIRY_MALLOC_GUARD(ptr, size, on_fail) \
    AUTO_FREE void *ptr = AIRY_MALLOC(size); \
    if (!(ptr)) { on_fail; }

/**
 * @def AIRY_CALLOC_GUARD(ptr, num, size, on_fail)
 * @brief RAII 内存分配守卫：calloc + NULL 检查 + 自动释放
 */
#define AIRY_CALLOC_GUARD(ptr, num, size, on_fail) \
    AUTO_FREE void *ptr = AIRY_CALLOC(num, size); \
    if (!(ptr)) { on_fail; }

#elif defined(_MSC_VER)

/**
 * @def AIRY_MALLOC_GUARD(ptr, size, on_fail)
 * @brief RAII 内存分配守卫（MSVC — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_MALLOC_GUARD(ptr, size, on_fail) \
    void *ptr = AIRY_MALLOC(size); \
    if (!(ptr)) { on_fail; }

/**
 * @def AIRY_CALLOC_GUARD(ptr, num, size, on_fail)
 * @brief RAII 内存分配守卫（MSVC — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_CALLOC_GUARD(ptr, num, size, on_fail) \
    void *ptr = AIRY_CALLOC(num, size); \
    if (!(ptr)) { on_fail; }

#else

/**
 * @def AIRY_MALLOC_GUARD(ptr, size, on_fail)
 * @brief RAII 内存分配守卫（未知编译器 — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_MALLOC_GUARD(ptr, size, on_fail) \
    void *ptr = AIRY_MALLOC(size); \
    if (!(ptr)) { on_fail; }

/**
 * @def AIRY_CALLOC_GUARD(ptr, num, size, on_fail)
 * @brief RAII 内存分配守卫（未知编译器 — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_CALLOC_GUARD(ptr, num, size, on_fail) \
    void *ptr = AIRY_CALLOC(num, size); \
    if (!(ptr)) { on_fail; }

#endif

/** @} */  // end of malloc_guard

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_MEMORY_H */