/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef AIRY_RT_LOGGING_COMPAT_H
#define AIRY_RT_LOGGING_COMPAT_H

/**
 * @file logging_compat.h
 * @brief AIRY_LOG_* 宏转发头 - 转发到 observability/logger.h 的权威定义。
 *
 * S-2 收敛 (2026-08-14, 用户决策: 与 [SC] log_types.h 枚举名对齐):
 * AIRY_LOG_* 的唯一权威定义在 commons/utils/observability/logger.h
 * （基于 airy_log_write → log_write_va → log_write）。本头文件仅作为
 * 兼容入口，把旧 include 路径（logging_compat.h）转发到权威源。
 *
 * When observability/logger.h is unavailable (log library not linked),
 * falls back to direct stderr output.
 */

#if __has_include("../observability/include/logger.h")
#include "../observability/logger.h"
#else

#include <stdio.h>
#include <stdlib.h>
#ifndef AIRY_LOG_ERROR
#define AIRY_LOG_ERROR(fmt, ...)                                                           \
    do {                                                                                   \
        fprintf(stderr, "[AIRY][ERROR] %s:%d %s: " fmt "\n", __FILE__, __LINE__, __func__, \
                ##__VA_ARGS__);                                                            \
    } while (0)
#endif
#ifndef AIRY_LOG_WARN
#define AIRY_LOG_WARN(fmt, ...)                                                            \
    do {                                                                                   \
        fprintf(stderr, "[AIRY][WARN]  %s:%d %s: " fmt "\n", __FILE__, __LINE__, __func__, \
                ##__VA_ARGS__);                                                            \
    } while (0)
#endif
#ifndef AIRY_LOG_INFO
#define AIRY_LOG_INFO(fmt, ...)                                                            \
    do {                                                                                   \
        fprintf(stderr, "[AIRY][INFO]  %s:%d %s: " fmt "\n", __FILE__, __LINE__, __func__, \
                ##__VA_ARGS__);                                                            \
    } while (0)
#endif
#ifndef AIRY_LOG_DEBUG
#define AIRY_LOG_DEBUG(fmt, ...)                                                           \
    do {                                                                                   \
        fprintf(stderr, "[AIRY][DEBUG] %s:%d %s: " fmt "\n", __FILE__, __LINE__, __func__, \
                ##__VA_ARGS__);                                                            \
    } while (0)
#endif
#ifndef AIRY_LOG_FATAL
#define AIRY_LOG_FATAL(fmt, ...)                                                           \
    do {                                                                                   \
        fprintf(stderr, "[AIRY][FATAL] %s:%d %s: " fmt "\n", __FILE__, __LINE__, __func__, \
                ##__VA_ARGS__);                                                            \
        abort();                                                                           \
    } while (0)
#endif
#endif

#endif /* AIRY_RT_LOGGING_COMPAT_H */
