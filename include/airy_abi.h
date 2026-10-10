/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_abi.h
 * @brief 统一 C-ABI 前导机制件（SSoT）。
 *
 * 0.1.19 §282：全仓 79 个头文件此前各自重复同一段 7 行 C-ABI 前导
 * （<stdbool.h>/<stddef.h>/<stdint.h> 三件套 + `extern "C" {`），构成全仓
 * 最大的单一克隆簇（81 份），横跨 7 个子模块。按「机制上提 / SSoT 收敛」
 * 将其收敛到本头：包含本头即得三件套标准类型，并以 AIRY_ABI_BEGIN /
 * AIRY_ABI_END 成对界定 C++ 链接边界。
 *
 * 用法：
 *   #include "airy_abi.h"
 *   AIRY_ABI_BEGIN
 *   ... declarations ...
 *   AIRY_ABI_END
 *
 * 在 C 编译单元中 BEGIN/END 展开为空；在 C++ 中展开为 extern "C" { 与 }。
 * 本头不定义任何符号，仅承载编译期机制，零运行时开销。
 */

#ifndef AIRY_RT_ABI_H
#define AIRY_RT_ABI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define AIRY_ABI_BEGIN extern "C" {
#define AIRY_ABI_END }
#else
#define AIRY_ABI_BEGIN
#define AIRY_ABI_END
#endif

#endif /* AIRY_RT_ABI_H */
