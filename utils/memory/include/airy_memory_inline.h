/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * Compatibility wrappers & safe inline helpers (airy_malloc family,
 * secure free, arena macros, string macros).
 * Split from airy_memory.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_MEMORY_INLINE_H
#define AIRY_RT_MEMORY_INLINE_H

#include "airy_memory_api.h"

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
#define AIRY_MALLOC(size)                      \
    __extension__({                            \
        void *__ptr = airy_malloc(size);       \
        if (__ptr) {                           \
            airy_mem_stats_record_alloc(size); \
        }                                      \
        __ptr;                                 \
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
#define AIRY_FREE(ptr)                    \
    do {                                  \
        airy_mem_stats_record_dealloc(0); \
        airy_free(ptr);                   \
    } while (0)


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

/** @} */ /* end of auto_free */

#if defined(_MSC_VER)

#include <windows.h>
#define AIRY_SECURE_FREE(ptr, size)          \
    do {                                     \
        if ((ptr) && (size) > 0) {           \
            SecureZeroMemory((ptr), (size)); \
        }                                    \
        AIRY_FREE(ptr);                      \
        (ptr) = NULL;                        \
    } while (0)
#else

#define AIRY_SECURE_FREE(ptr, size)                                             \
    do {                                                                        \
        if ((ptr) && (size) > 0) {                                              \
            volatile char *__airy_sf_ptr = (volatile char *)(ptr);              \
            for (size_t __airy_sf_i = 0; __airy_sf_i < (size); __airy_sf_i++) { \
                __airy_sf_ptr[__airy_sf_i] = 0;                                 \
            }                                                                   \
            /* 内存屏障：防止编译器优化掉清零操作 */                            \
            __asm__ __volatile__("" : : "r"(__airy_sf_ptr) : "memory");         \
        }                                                                       \
        AIRY_FREE(ptr);                                                         \
        (ptr) = NULL;                                                           \
    } while (0)
#endif

/** @} */ /* end of secure_free */

void *airy_arena_alloc(airy_arena_t *arena, size_t size);

/**
 * @def AIRY_ARENA_ALLOC(size)
 * @brief 从当前线程 Arena 分配内存
 *
 * 如果当前线程没有设置 Arena，回退到 AIRY_MALLOC。
 */
#define AIRY_ARENA_ALLOC(size)                                                     \
    (airy_arena_get_current() ? airy_arena_alloc(airy_arena_get_current(), size) : \
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

/** @} */ /* end of arena_alloc */

/**
 * @def SAFE_MALLOC(ptr, size)
 * @brief 安全内存分配，失败时记录日志并返回AIRY_ENOMEM
 */
#define SAFE_MALLOC(ptr, size)     \
    do {                           \
        (ptr) = AIRY_MALLOC(size); \
        if (!(ptr)) {              \
            (ptr) = NULL;          \
        }                          \
    } while (0)

/**
 * @def SAFE_CALLOC(ptr, num, size)
 * @brief 安全内存清零分配，失败时记录日志并返回AIRY_ENOMEM
 */
#define SAFE_CALLOC(ptr, num, size)     \
    do {                                \
        (ptr) = AIRY_CALLOC(num, size); \
        if (!(ptr)) {                   \
            (ptr) = NULL;               \
        }                               \
    } while (0)

/**
 * @def CHECK_ALLOC(ptr)
 * @brief 检查指针是否为NULL，如果是则记录错误并返回AIRY_ENOMEM
 */
#define CHECK_ALLOC(ptr)  \
    do {                  \
        if (!(ptr)) {     \
            (ptr) = NULL; \
        }                 \
    } while (0)

/**
 * @def AIRY_STRNCPY_TERM(dst, src, size)
 * @brief 安全字符串复制宏，确保目标缓冲区始终以 null 终止
 */
#define AIRY_STRNCPY_TERM(dst, src, size)                               \
    do {                                                                \
        size_t _len = __builtin_strlen(src);                            \
        size_t _copy = ((_len) < ((size) - 1)) ? (_len) : ((size) - 1); \
        __builtin_memcpy((dst), (src), _copy);                          \
        (dst)[_copy] = '\0';                                            \
    } while (0)

/** @} */ /* end of safe_memory_alloc */

/**
 * @def AIRY_MEMCPY_SAFE(dst, src, size, dst_capacity)
 * @brief 带边界检查的安全 memcpy
 */
#define AIRY_MEMCPY_SAFE(dst, src, size, dst_capacity) \
    do {                                               \
        if ((size_t)(size) > (size_t)(dst_capacity)) { \
            break; /* overflow: skip copy */           \
        }                                              \
        __builtin_memcpy((dst), (src), (size));        \
    } while (0)

/**
 * @def SAFE_MALLOC_ARRAY(ptr, count, element_size)
 * @brief 安全数组分配，带整数溢出检查
 */
#define SAFE_MALLOC_ARRAY(ptr, count, element_size)                         \
    do {                                                                    \
        (ptr) = airy_malloc_array((size_t)(count), (size_t)(element_size)); \
        if (!(ptr)) {                                                       \
            (ptr) = NULL;                                                   \
        }                                                                   \
    } while (0)

/**
 * @def SAFE_CALLOC_ARRAY(ptr, count, element_size)
 * @brief 安全数组清零分配，带整数溢出检查
 */
#define SAFE_CALLOC_ARRAY(ptr, count, element_size)                         \
    do {                                                                    \
        (ptr) = airy_calloc_array((size_t)(count), (size_t)(element_size)); \
        if (!(ptr)) {                                                       \
            (ptr) = NULL;                                                   \
        }                                                                   \
    } while (0)

/** @} */ /* end of safe_buffer_ops */

/**
 * @def AIRY_MEMSET(ptr, value, size)
 * @brief 带零大小保护的安全 memset
 *
 * 编码契约 SEC-04: 当 size == 0 时不执行任何操作，避免空指针解引用。
 */
#define AIRY_MEMSET(ptr, value, size)                 \
    do {                                              \
        if ((size) > 0)                               \
            __builtin_memset((ptr), (value), (size)); \
    } while (0)

/**
 * @def AIRY_MEMCPY(dst, src, size)
 * @brief 安全内存复制宏（绕过 BAN poison，调用者需确保边界安全）
 */
#define AIRY_MEMCPY(dst, src, size)                 \
    do {                                            \
        if ((size) > 0)                             \
            __builtin_memcpy((dst), (src), (size)); \
    } while (0)

/**
 * @def AIRY_MEMMOVE(dst, src, size)
 * @brief 安全内存移动宏（处理重叠区域，绕过 BAN poison）
 */
#define AIRY_MEMMOVE(dst, src, size)                 \
    do {                                             \
        if ((size) > 0)                              \
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

#endif /* AIRY_RT_MEMORY_INLINE_H */
