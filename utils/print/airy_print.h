/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_print.h
 * @brief P3.25: runtime unified print API - naming aligned with the CMake
 * airy_print.cmake layer.
 *
 * Provides 8 functional macros whose names align exactly with the
 * build-time `airy_print.cmake`; underneath they delegate to the core
 * logging system `log_write()`, reusing all its capabilities (multiple
 * output targets, multiple formats, trace_id propagation, thread safety,
 * runtime config hot reload).
 *
 * **Design principles**:
 * - Status printing (OK/NO) automatically prefixes `[OK]`/`[NO]` labels
 * - Color mapping is applied internally by `log_write` per level
 *   (LOG_LEVEL_INFO = blue, etc.)
 * - OK maps to LOG_LEVEL_INFO (informational success), NO to
 *   LOG_LEVEL_ERROR
 * - Direct fprintf/printf calls are forbidden in production code; use
 *   these macros or the LOG_* macros instead
 *
 * **Correspondence with CMake airy_print.cmake**:
 * | Runtime macro (this file) | CMake function (airy_print.cmake) | Level           |
 * |---------------------------|-----------------------------------|-----------------|
 * | airy_print_ok()        | airy_print_ok()                | LOG_LEVEL_INFO  |
 * | airy_print_no()        | (none, runtime-only)              | LOG_LEVEL_ERROR |
 * | airy_print_info()      | airy_print_info()              | LOG_LEVEL_INFO  |
 * | airy_print_warn()      | airy_print_warn()              | LOG_LEVEL_WARN  |
 * | airy_print_error()     | airy_print_error()             | LOG_LEVEL_ERROR |
 * | airy_print_fatal()     | airy_print_fatal()             | LOG_LEVEL_FATAL |
 * | airy_print_debug()     | airy_print_debug()             | LOG_LEVEL_DEBUG |
 * | airy_print_section()   | airy_print_section()           | LOG_LEVEL_INFO  |
 *
 * @section usage example
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
 */

#ifndef AIRY_RT_PRINT_H
#define AIRY_RT_PRINT_H

#include <logging.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Status printing: OK / NO
 * Completely missing before v3.1; first implemented here (P3.25)
 * ================================================================ */

/**
 * @brief OK status print - success/confirmation.
 *
 * Maps to LOG_LEVEL_INFO; `[OK] ` is prefixed automatically.
 * Color is applied internally by log_write per INFO level (blue).
 *
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 */
#define airy_print_ok(fmt, ...) \
    log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "[OK] " fmt, ##__VA_ARGS__)

/**
 * @brief NO status print - failure/rejection.
 *
 * Maps to LOG_LEVEL_ERROR; `[NO] ` is prefixed automatically.
 * Color is applied internally by log_write per ERROR level (red).
 *
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 */
#define airy_print_no(fmt, ...) \
    log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, "[NO] " fmt, ##__VA_ARGS__)

/* ================================================================
 * Level printing: names aligned with CMake airy_print.cmake
 * ================================================================ */

/**
 * @brief INFO level print - informational output.
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 */
#define airy_print_info(fmt, ...) log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief WARN level print - warning.
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 */
#define airy_print_warn(fmt, ...) log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief ERROR level print - error (does not terminate).
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 */
#define airy_print_error(fmt, ...) \
    log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief FATAL level print - fatal error (usually exits the process).
 *
 * Note: this macro delegates to log_write(LOG_LEVEL_FATAL, ...); whether
 * log_write aborts internally is decided by the logging system config.
 * To terminate immediately, callers should explicitly call abort() or
 * exit(EXIT_FAILURE) after this macro.
 *
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 */
#define airy_print_fatal(fmt, ...) \
    log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief DEBUG level print - debug info.
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 */
#define airy_print_debug(fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ================================================================
 * Section title printing
 * ================================================================ */

/**
 * @brief SECTION section title - informational boundary marker.
 *
 * Wraps the message with `=== ` / ` ===` boundaries and maps to
 * LOG_LEVEL_INFO. Used for daemon startup banners, module init stage
 * separators, etc.
 *
 * @param fmt  printf-style format string (usually a plain string)
 * @param ...  Format arguments
 */
#define airy_print_section(fmt, ...) \
    log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "=== " fmt " ===", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PRINT_H */
