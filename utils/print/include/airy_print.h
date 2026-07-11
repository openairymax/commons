/**
 * @file airy_print.h
 * @brief P3.25: 运行时统一打印 API — 与 CMake 层 airy_print.cmake 命名对齐
 *
 * @details
 * 提供 8 个函数式宏，命名与构建期 `airy_print.cmake` 完全对齐，
 * 底层委托核心日志系统 `log_write()`，复用其全部能力（多输出目标、
 * 多格式、trace_id 贯穿、线程安全、运行时配置热重载）。
 *
 * **设计原则**:
 * - 状态打印（OK/NO）在 message 前自动添加 `[OK]`/`[NO]` 标签
 * - 色彩映射由 `log_write` 内部根据级别自动应用（LOG_LEVEL_INFO=蓝等）
 * - OK 状态映射到 LOG_LEVEL_INFO（信息性成功），NO 状态映射到 LOG_LEVEL_ERROR
 * - 禁止在生产代码中直接调用 fprintf/printf，必须使用本文件宏或 LOG_* 宏
 *
 * **与 CMake airy_print.cmake 的对应关系**:
 * | 运行时宏 (本文件)         | CMake 函数 (airy_print.cmake) | 级别           |
 * |--------------------------|----------------------------------|----------------|
 * | airy_print_ok()       | airy_print_ok()               | LOG_LEVEL_INFO |
 * | airy_print_no()       | (无对应，运行时独有)              | LOG_LEVEL_ERROR|
 * | airy_print_info()     | airy_print_info()             | LOG_LEVEL_INFO |
 * | airy_print_warn()     | airy_print_warn()             | LOG_LEVEL_WARN |
 * | airy_print_error()    | airy_print_error()            | LOG_LEVEL_ERROR|
 * | airy_print_fatal()    | airy_print_fatal()            | LOG_LEVEL_FATAL|
 * | airy_print_debug()    | airy_print_debug()            | LOG_LEVEL_DEBUG|
 * | airy_print_section()  | airy_print_section()          | LOG_LEVEL_INFO |
 *
 * @section 使用示例
 * @code
 * #include "airy_print.h"
 *
 * airy_print_section("Daemon Bootstrap");
 * airy_print_info("agentrt v0.1.1 starting (pid=%d)", getpid());
 * airy_print_ok("config loaded: %s", config_path);
 * airy_print_warn("deprecated option: %s", opt_name);
 * airy_print_error("init failed: %s (errno=%d)", what, errno);
 * airy_print_no("health check failed: %s", check_name);
 * airy_print_debug("trace: %s entered", __func__);
 * @endcode
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 */

#ifndef AIRY_RT_PRINT_H
#define AIRY_RT_PRINT_H

#include <logging.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 状态打印：OK / NO
 * v3.1 前完全缺失，本文件首次实现（P3.25）
 * ================================================================ */

/**
 * @brief OK 状态打印 — 成功/确认
 *
 * 映射到 LOG_LEVEL_INFO，message 前自动添加 `[OK] ` 标签。
 * 色彩由 log_write 内部按 INFO 级别应用（蓝色）。
 *
 * @param fmt  printf 风格格式字符串
 * @param ...  格式参数
 */
#define airy_print_ok(fmt, ...) \
    log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "[OK] " fmt, ##__VA_ARGS__)

/**
 * @brief NO 状态打印 — 失败/拒绝
 *
 * 映射到 LOG_LEVEL_ERROR，message 前自动添加 `[NO] ` 标签。
 * 色彩由 log_write 内部按 ERROR 级别应用（红色）。
 *
 * @param fmt  printf 风格格式字符串
 * @param ...  格式参数
 */
#define airy_print_no(fmt, ...) \
    log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, "[NO] " fmt, ##__VA_ARGS__)

/* ================================================================
 * 级别打印：与 CMake airy_print.cmake 命名对齐
 * ================================================================ */

/**
 * @brief INFO 级别打印 — 信息性输出
 * @param fmt  printf 风格格式字符串
 * @param ...  格式参数
 */
#define airy_print_info(fmt, ...) \
    log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief WARN 级别打印 — 警告
 * @param fmt  printf 风格格式字符串
 * @param ...  格式参数
 */
#define airy_print_warn(fmt, ...) \
    log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief ERROR 级别打印 — 错误（不终止）
 * @param fmt  printf 风格格式字符串
 * @param ...  格式参数
 */
#define airy_print_error(fmt, ...) \
    log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief FATAL 级别打印 — 致命错误（通常导致进程退出）
 *
 * 注意：本宏委托 log_write(LOG_LEVEL_FATAL, ...)，log_write 内部
 * 是否触发 abort() 由日志系统配置决定。如需立即终止，调用方应在
 * 本宏之后显式调用 abort() 或 exit(EXIT_FAILURE)。
 *
 * @param fmt  printf 风格格式字符串
 * @param ...  格式参数
 */
#define airy_print_fatal(fmt, ...) \
    log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief DEBUG 级别打印 — 调试信息
 * @param fmt  printf 风格格式字符串
 * @param ...  格式参数
 */
#define airy_print_debug(fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ================================================================
 * 章节标题打印
 * ================================================================ */

/**
 * @brief SECTION 章节标题 — 信息性边界标记
 *
 * 在 message 前后添加 `=== ` / ` ===` 边界，映射到 LOG_LEVEL_INFO。
 * 用于 daemon 启动 banner、模块初始化阶段分隔等。
 *
 * @param fmt  printf 风格格式字符串（通常为纯字符串）
 * @param ...  格式参数
 */
#define airy_print_section(fmt, ...) \
    log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "=== " fmt " ===", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PRINT_H */
