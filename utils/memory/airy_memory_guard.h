/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * RAII malloc/calloc guard macros (AIRY_MALLOC_GUARD).
 * Split from airy_memory.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_MEMORY_GUARD_H
#define AIRY_RT_MEMORY_GUARD_H

#include "airy_memory_inline.h"

#if defined(__GNUC__) || defined(__clang__)

/**
 * @def AIRY_MALLOC_GUARD(ptr, size, on_fail)
 * @brief RAII 内存分配守卫：malloc + NULL 检查 + 自动释放
 */
#define AIRY_MALLOC_GUARD(ptr, size, on_fail) \
    AUTO_FREE void *ptr = AIRY_MALLOC(size);  \
    if (!(ptr)) {                             \
        on_fail;                              \
    }

/**
 * @def AIRY_CALLOC_GUARD(ptr, num, size, on_fail)
 * @brief RAII 内存分配守卫：calloc + NULL 检查 + 自动释放
 */
#define AIRY_CALLOC_GUARD(ptr, num, size, on_fail) \
    AUTO_FREE void *ptr = AIRY_CALLOC(num, size);  \
    if (!(ptr)) {                                  \
        on_fail;                                   \
    }

#elif defined(_MSC_VER)

/**
 * @def AIRY_MALLOC_GUARD(ptr, size, on_fail)
 * @brief RAII 内存分配守卫（MSVC — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_MALLOC_GUARD(ptr, size, on_fail) \
    void *ptr = AIRY_MALLOC(size);            \
    if (!(ptr)) {                             \
        on_fail;                              \
    }

/**
 * @def AIRY_CALLOC_GUARD(ptr, num, size, on_fail)
 * @brief RAII 内存分配守卫（MSVC — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_CALLOC_GUARD(ptr, num, size, on_fail) \
    void *ptr = AIRY_CALLOC(num, size);            \
    if (!(ptr)) {                                  \
        on_fail;                                   \
    }

#else

/**
 * @def AIRY_MALLOC_GUARD(ptr, size, on_fail)
 * @brief RAII 内存分配守卫（未知编译器 — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_MALLOC_GUARD(ptr, size, on_fail) \
    void *ptr = AIRY_MALLOC(size);            \
    if (!(ptr)) {                             \
        on_fail;                              \
    }

/**
 * @def AIRY_CALLOC_GUARD(ptr, num, size, on_fail)
 * @brief RAII 内存分配守卫（未知编译器 — 无自动释放，需手动 AIRY_FREE）
 */
#define AIRY_CALLOC_GUARD(ptr, num, size, on_fail) \
    void *ptr = AIRY_CALLOC(num, size);            \
    if (!(ptr)) {                                  \
        on_fail;                                   \
    }

#endif

#endif /* AIRY_RT_MEMORY_GUARD_H */
