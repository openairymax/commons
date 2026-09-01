// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logging_format.c
 * @brief Unified layered logging system - console line formatting.
 *
 * 2026-08-27 域拆分（原 logging.c）：ANSI 颜色、终端探测与控制台
 * 日志行格式化（时间戳/级别/位置/trace/span/线程/进程）。
 */

#include "logging_internal.h"

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN "\033[36m"
#define ANSI_GRAY "\033[90m"
#define ANSI_BG_RED "\033[41m"

static const char *LEVEL_COLORS[] = {
    ANSI_GRAY, ANSI_BLUE, ANSI_YELLOW, ANSI_RED, ANSI_MAGENTA,
};

bool log_internal_is_terminal(int fd)
{
#ifdef _WIN32
    (void)fd;
    return false;
#else
    static int cached_stdout_tty = -1;
    static int cached_stderr_tty = -1;
    if (fd == STDOUT_FILENO) {
        if (cached_stdout_tty < 0)
            cached_stdout_tty = isatty(STDOUT_FILENO) ? 1 : 0;
        return cached_stdout_tty == 1;
    }
    if (fd == STDERR_FILENO) {
        if (cached_stderr_tty < 0)
            cached_stderr_tty = isatty(STDERR_FILENO) ? 1 : 0;
        return cached_stderr_tty == 1;
    }
    return isatty(fd) == 1;
#endif
}

size_t log_internal_format_message(const log_record_t *record, char *buffer, size_t buffer_size)
{
    if (!record || !buffer || buffer_size == 0) {
        return 0;
    }

    time_t sec = record->timestamp / 1000;
    int ms = record->timestamp % 1000;
    struct tm tm_storage;
    localtime_r(&sec, &tm_storage);
    struct tm *tm_info = &tm_storage;

    const char *level_name = log_level_to_string(record->level);

    size_t level_names_count = 0;
    (void)log_internal_level_names(&level_names_count);

    const char *color = "";
    const char *reset = "";
    if (log_internal_color_enabled() && record->level < (log_level_t)level_names_count) {
        color = LEVEL_COLORS[record->level];
        reset = ANSI_RESET;
    }

    int len = snprintf(buffer, buffer_size, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s%s%s] [%s:%d]",
                       tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                       tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, ms, color, level_name,
                       reset, record->module, record->line);
    if (len < 0)
        return 0;
    if ((size_t)len >= buffer_size)
        len = (int)buffer_size - 1;

    if (record->trace_id && record->trace_id[0] != '\0') {
        len += snprintf(buffer + len, buffer_size - (size_t)len, " [trace:%s]", record->trace_id);
        if (len < 0)
            return 0;
        if ((size_t)len >= buffer_size)
            len = (int)buffer_size - 1;
    }

    if (record->span_id && record->span_id[0] != '\0') {
        len += snprintf(buffer + len, buffer_size - (size_t)len, " [span:%s]", record->span_id);
        if (len < 0)
            return 0;
        if ((size_t)len >= buffer_size)
            len = (int)buffer_size - 1;
    }

    len += snprintf(buffer + len, buffer_size - (size_t)len, " [thread:%llu] [process:%u]",
                    (unsigned long long)record->thread_id, (unsigned)record->process_id);
    if (len < 0)
        return 0;
    if ((size_t)len >= buffer_size)
        len = (int)buffer_size - 1;

    len += snprintf(buffer + len, buffer_size - (size_t)len, " %s\n", record->message);
    if (len < 0)
        return 0;
    if ((size_t)len >= buffer_size)
        len = (int)buffer_size - 1;

    return (size_t)len;
}
