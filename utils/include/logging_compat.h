/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef AIRY_RT_LOGGING_COMPAT_H
#define AIRY_RT_LOGGING_COMPAT_H

/**
 * @file logging_compat.h
 * @brief Unified AIRY_LOG_* macros - forwarded to the LOG_* macros of logging.h.
 *
 * Unified logging entry: all modules use AIRY_LOG_ERROR / AIRY_LOG_WARN /
 * AIRY_LOG_INFO / AIRY_LOG_DEBUG, forwarded by this header to the
 * log_write() implementation in logging.h.
 *
 * When logging.h is unavailable (log library not linked), falls back to
 * direct stderr output.
 */


#if __has_include("logging.h")
#include "logging.h"
/* #ifndef guard: if observability/include/logger.h was included before
 * this header and already defined AIRY_LOG_* (based on airy_log_write),
 * skip the definitions here to avoid redefinition warnings. Both paths
 * eventually call log_write(), so the semantics are equivalent. */
#ifndef AIRY_LOG_ERROR
#define AIRY_LOG_ERROR(fmt, ...) LOG_ERROR(fmt, ##__VA_ARGS__)
#endif
#ifndef AIRY_LOG_WARN
#define AIRY_LOG_WARN(fmt, ...) LOG_WARN(fmt, ##__VA_ARGS__)
#endif
#ifndef AIRY_LOG_INFO
#define AIRY_LOG_INFO(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)
#endif
#ifndef AIRY_LOG_DEBUG
#define AIRY_LOG_DEBUG(fmt, ...) LOG_DEBUG(fmt, ##__VA_ARGS__)
#endif
#ifndef AIRY_LOG_FATAL
#define AIRY_LOG_FATAL(fmt, ...) LOG_FATAL(fmt, ##__VA_ARGS__)
#endif
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
