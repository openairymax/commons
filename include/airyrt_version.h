/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airyrt_version.h
 * @brief AgentRT 版本单一权威源（C 侧 SSoT）。
 *
 * 版本唯一来源为仓库根 VERSION 文件。构建系统（顶层 CMakeLists.txt）
 * 读取 VERSION 并经 add_compile_definitions 注入 AIRYRT_VERSION，
 * 覆盖本目录及全部子目录；所有 C 模块（各 daemon svc_adapter /
 * svc_common / CLI / 示例）一律引用该宏，禁止散落硬编码版本串
 * （Unify Design SSoT：10-unify-design.md）。
 *
 * 本头仅提供**漂移免疫回退值**：未走 CMake 注入时（如独立语法检查、
 * IDE 单文件编译）AIRYRT_VERSION 取 "0.0.0-dev"，明确标识"非发布
 * 构建"。该回退值在源码内不携带任何真实发布号，故结构上不存在
 * "人肉同步版本号"这一漂移根因——先前 0.1.8 审计发现的
 * "构建后才发现版本硬编码漂移"即源自定义无保护的 0.1.10 手工副本。
 * 发布构建的版本一致性由 version-consistency-check.sh 门禁保障。
 *
 * @note DT-13 大声失败：若在发布产物中发现 "0.0.0-dev"，即表示构建
 *       管线未注入 VERSION，属必须修复的配置错误而非可容忍缺省。
 */

#ifndef AIRY_RT_COMMONS_AIRYRT_VERSION_H
#define AIRY_RT_COMMONS_AIRYRT_VERSION_H

#ifndef AIRYRT_VERSION
#define AIRYRT_VERSION "0.0.0-dev"
#endif

#endif /* AIRY_RT_COMMONS_AIRYRT_VERSION_H */
