/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file airy_memory.h
 * @brief 统一内存管理模块 - 核心层 API（聚合入口，向后兼容）。
 *
 * 0.1.6 大文件拆分：本文件保留为聚合入口，实际声明分布到：
 *   - airy_memory_api.h       核心 API 声明（memory_init/alloc/free/...）
 *   - airy_memory_inline.h    airy_* 内联封装与安全宏
 *   - airy_memory_stats_ext.h 扩展统计与水位跟踪
 *   - airy_memory_guard.h     RAII 分配守卫宏
 */

#ifndef AIRY_RT_MEMORY_H
#define AIRY_RT_MEMORY_H

#include "airy_memory_api.h"
#include "airy_memory_inline.h"
#include "airy_memory_stats_ext.h"
#include "airy_memory_guard.h"

#endif /* AIRY_RT_MEMORY_H */
