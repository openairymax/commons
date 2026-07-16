/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * uapi_compat.h — [SC] Shared Contract Layer: UAPI type compatibility
 *
 * Provides Linux kernel-style fixed-width types (__u8/__u16/__u32/__u64/__s32)
 * for cross-platform compatibility between agentrt (user-space) and
 * agentrt-linux (kernel) code.
 *
 * IRON-9 v2 [SC] layer — shared between agentrt and agentrt-linux.
 */

#ifndef _AIRY_UAPI_COMPAT_H
#define _AIRY_UAPI_COMPAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Linux kernel-style fixed-width types.
 *
 * d9 修复（预先存在问题）：在 Linux 平台上，__u8/__u16/__u32/__u64/__s8/__s16/__s32/__s64
 * 由 <asm/types.h> 通过 <linux/types.h> 定义。直接 typedef 会与系统头文件冲突
 *（conflicting types for '__u64'; have 'uint64_t' {aka 'long unsigned int'} vs
 * 'unsigned long long'）。
 *
 * 修复策略：Linux 平台包含 <linux/types.h> 获取系统定义；非 Linux 平台自定义。
 */
#ifdef __linux__
#include <linux/types.h>
/* Linux: __u8/__u16/__u32/__u64/__s8/__s16/__s32/__s64 由 <linux/types.h> 提供 */
#else
/* 非 Linux 平台（macOS/Windows 等）：自定义 kernel-style 类型 */
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;

typedef int8_t   __s8;
typedef int16_t  __s16;
typedef int32_t  __s32;
typedef int64_t  __s64;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _AIRY_UAPI_COMPAT_H */
