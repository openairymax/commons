/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file atomic_compat.h
 * @brief Cross-platform atomic operations compatibility layer (聚合入口)。
 *
 * 0.1.6 大文件拆分：本文件保留为聚合入口，实际实现分布到：
 *   - atomic_compat_platform.h  平台选择与底层原子原语
 *   - atomic_compat_api.h       统一原子类型与操作 API
 */

#ifndef AIRY_RT_ATOMIC_COMPAT_H
#define AIRY_RT_ATOMIC_COMPAT_H

#pragma GCC system_header

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "atomic_compat_platform.h"
#include "atomic_compat_api.h"
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_ATOMIC_COMPAT_H */
