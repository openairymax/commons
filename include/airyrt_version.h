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
 * 升级流程：修改本宏时同步修改 VERSION 与根 CMakeLists project(VERSION)。
 */

#ifndef AIRY_RT_COMMONS_AIRYRT_VERSION_H
#define AIRY_RT_COMMONS_AIRYRT_VERSION_H

#define AIRYRT_VERSION "0.1.5"

#endif /* AIRY_RT_COMMONS_AIRYRT_VERSION_H */
