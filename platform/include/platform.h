/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file platform.h
 * @brief Cross-platform compatibility layer - unifies OS API differences
 *
 * Supported platforms:
 * - Linux (POSIX)
 * - macOS (Darwin)
 * - Windows (Win32/Win64)
 *
 * Design principles:
 * - Single responsibility: handle platform differences only
 * - Zero overhead: inline functions + macro definitions
 * - Type safety: strongly-typed wrappers
 *
 * 2026-08-27 域拆分：本文件保留为聚合入口（向后兼容，所有
 * `#include "platform.h"` 无需修改），实际声明按功能域分布到：
 *   - platform_base.h    平台检测 / 系统头 / 核心句柄类型
 *   - platform_sync.h    互斥锁 / 条件变量 / RAII 守卫 / 原子
 *   - platform_process.h 线程 / 套接字 / 子进程
 *   - platform_paths.h   路径常量 / AIRY_HOME 路径系统
 *   - platform_misc.h    时间 / 随机数 / 文件 / 系统信息
 *   - platform_time.h    时间服务：逻辑墙钟 / 时区 / SNTP 校对（2026-08-30）
 *
 * @note Thread safety: the platform abstraction layer itself does not
 *       involve thread safety
 * @see ARCHITECTURAL_PRINCIPLES.md E-4 cross-platform consistency principle
 */

#ifndef AIRY_RT_PLATFORM_H
#define AIRY_RT_PLATFORM_H

#include "platform_base.h"
#include "platform_sync.h"
#include "platform_process.h"
#include "platform_paths.h"
#include "platform_misc.h"
#include "platform_time.h"

#endif /* AIRY_RT_PLATFORM_H */
