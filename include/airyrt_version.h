/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airyrt_version.h
 * @brief AgentRT 版本单一权威源（C 侧 SSoT）。
 *
 * 对应仓库根 VERSION 文件与 CMakeLists project()。所有 C 模块（各 daemon
 * svc_adapter / svc_common / CLI / 示例）一律引用 AIRYRT_VERSION 宏，
 * 禁止散落硬编码版本串（Unify Design SSoT：10-unify-design.md）。
 *
 * 升级流程：版本单一来源为仓库根 VERSION 文件。构建系统（顶层
 * CMakeLists.txt）经 add_compile_definitions 注入 AIRYRT_VERSION，本头
 * 仅在**未走 CMake 构建**（如独立语法检查、非构建工具链）时提供回退
 * 缺省值。此前本宏为无保护的手工副本，每次发版须人肉同步，是"构建后
 * 才发现版本硬编码漂移"的系统性根因（0.1.8 审计修复）。
 */

#ifndef AIRY_RT_COMMONS_AIRYRT_VERSION_H
#define AIRY_RT_COMMONS_AIRYRT_VERSION_H

#ifndef AIRYRT_VERSION
#define AIRYRT_VERSION "0.1.8"
#endif

#endif /* AIRY_RT_COMMONS_AIRYRT_VERSION_H */
