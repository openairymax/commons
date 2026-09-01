/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * Core memory management API (memory_init/memory_alloc/...).
 * Split from airy_memory.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_MEMORY_API_H
#define AIRY_RT_MEMORY_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "arena.h"
#include "airy_memory_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/** @} */ /* end of memory_api */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_MEMORY_API_H */
