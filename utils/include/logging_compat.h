// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#ifndef AIRY_RT_LOGGING_COMPAT_H
#define AIRY_RT_LOGGING_COMPAT_H

/**
 * @file logging_compat.h
 * @brief AIRY_LOG_* 统一日志宏 — 转发至 logging.h 的 LOG_* 宏
 *
 * 统一日志入口：所有模块使用 AIRY_LOG_ERROR / AIRY_LOG_WARN /
 * AIRY_LOG_INFO / AIRY_LOG_DEBUG，由本头文件转发至
 * logging.h 的 log_write() 实现。
 *
 * 当 logging.h 不可用时（未链接日志库），回退至 stderr 直接输出。
 */

/* 尝试包含完整日志系统头文件 */
#if __has_include("logging.h")
  #include "logging.h"
  #define AIRY_LOG_ERROR(fmt, ...) LOG_ERROR(fmt, ##__VA_ARGS__)
  #define AIRY_LOG_WARN(fmt, ...)  LOG_WARN(fmt, ##__VA_ARGS__)
  #define AIRY_LOG_INFO(fmt, ...)  LOG_INFO(fmt, ##__VA_ARGS__)
  #define AIRY_LOG_DEBUG(fmt, ...) LOG_DEBUG(fmt, ##__VA_ARGS__)
  #define AIRY_LOG_FATAL(fmt, ...) LOG_FATAL(fmt, ##__VA_ARGS__)
#else
  /* 回退：logging.h 不可用时直接输出到 stderr */
  #include <stdio.h>
  #include <stdlib.h>
  #define AIRY_LOG_ERROR(fmt, ...)                                                  \
      do {                                                                          \
          fprintf(stderr, "[AIRY][ERROR] %s:%d %s: " fmt "\n", __FILE__, __LINE__, \
                  __func__, ##__VA_ARGS__);                                         \
      } while (0)
  #define AIRY_LOG_WARN(fmt, ...)                                                  \
      do {                                                                         \
          fprintf(stderr, "[AIRY][WARN]  %s:%d %s: " fmt "\n", __FILE__, __LINE__, \
                  __func__, ##__VA_ARGS__);                                        \
      } while (0)
  #define AIRY_LOG_INFO(fmt, ...)                                                  \
      do {                                                                         \
          fprintf(stderr, "[AIRY][INFO]  %s:%d %s: " fmt "\n", __FILE__, __LINE__, \
                  __func__, ##__VA_ARGS__);                                        \
      } while (0)
  #define AIRY_LOG_DEBUG(fmt, ...)                                                  \
      do {                                                                          \
          fprintf(stderr, "[AIRY][DEBUG] %s:%d %s: " fmt "\n", __FILE__, __LINE__, \
                  __func__, ##__VA_ARGS__);                                         \
      } while (0)
  #define AIRY_LOG_FATAL(fmt, ...)                                                  \
      do {                                                                          \
          fprintf(stderr, "[AIRY][FATAL] %s:%d %s: " fmt "\n", __FILE__, __LINE__, \
                  __func__, ##__VA_ARGS__);                                         \
          abort();                                                                  \
      } while (0)
#endif

#endif /* AIRY_RT_LOGGING_COMPAT_H */
