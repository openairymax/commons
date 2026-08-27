// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logging_backend_file.c
 * @brief Unified layered logging system - file output backend & rotation.
 *
 * 2026-08-27 域拆分（原 logging.c）：文件后端打开/写入与按大小
 * 轮转管理（备份数保留 + 轮转前 fsync 防崩溃丢日志）。
 */

#include "logging_internal.h"

static log_file_state_t g_log_file_state = {
    .file = NULL,
    .current_size = 0,
    .mutex_init = false,
};

log_file_state_t *log_internal_file_state(void)
{
    return &g_log_file_state;
}

int log_internal_file_open(const char *path)
{
    if (!path || !g_log_file_state.mutex_init)
        return AIRY_EINVAL;

    airy_mtx_lock(&g_log_file_state.mutex);
    if (g_log_file_state.file) {
        fclose(g_log_file_state.file);
        g_log_file_state.file = NULL;
    }
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    g_log_file_state.file = fopen(path, "a");
    if (!g_log_file_state.file) {
        airy_mtx_unlock(&g_log_file_state.mutex);
        return AIRY_EIO;
    }

    fseek(g_log_file_state.file, 0, SEEK_END);
    g_log_file_state.current_size = (size_t)ftell(g_log_file_state.file);
    airy_mtx_unlock(&g_log_file_state.mutex);
    return 0;
}

static void log_file_rotate_if_needed(void)
{
    if (!g_log_file_state.file || !g_log_file_state.mutex_init)
        return;

    logging_state_t *st = log_internal_state();
    size_t max_size = st->manager.max_file_size;
    int max_backup = st->manager.max_backup_count;
    if (max_size == 0)
        max_size = 10 * 1024 * 1024;
    if (max_backup <= 0)
        max_backup = 5;

    if (g_log_file_state.current_size < max_size)
        return;

    const char *path = st->manager.file_path;
    if (!path)
        return;

    /* P2-1：轮转前 fsync 旧文件，避免轮转窗口崩溃丢日志 */
    if (g_log_file_state.file) {
        fflush(g_log_file_state.file);
#ifdef _WIN32
        _commit(_fileno(g_log_file_state.file));
#else
        fsync(fileno(g_log_file_state.file));
#endif
    }
    fclose(g_log_file_state.file);
    g_log_file_state.file = NULL;

    char old_path[512];
    char new_path[512];
    int rename_failed = 0;
    for (int i = max_backup - 1; i >= 0; i--) {
        if (i == 0) {
            snprintf(old_path, sizeof(old_path), "%s", path);
        } else {
            snprintf(old_path, sizeof(old_path), "%s.%d", path, i);
        }
        snprintf(new_path, sizeof(new_path), "%s.%d", path, i + 1);
        /* P2-1：rename 失败（目标为目录/跨设备）不再静默吞掉 */
        if (rename(old_path, new_path) != 0 && errno != ENOENT)
            rename_failed = 1;
    }

    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    g_log_file_state.file = fopen(path, "a");
    if (g_log_file_state.file) {
        fseek(g_log_file_state.file, 0, SEEK_END);
        g_log_file_state.current_size = (size_t)ftell(g_log_file_state.file);
    } else {
        g_log_file_state.current_size = 0;
    }
    if (rename_failed)
        AIRY_LOG_WARN("log rotation: some backup renames failed (path=%s)", path);
}

void log_internal_file_write(const log_record_t *record, const char *formatted_message,
                             size_t formatted_len)
{
    (void)formatted_message;
    (void)formatted_len;

    if (!g_log_file_state.file || !g_log_file_state.mutex_init || !record)
        return;

    airy_mtx_lock(&g_log_file_state.mutex);
    if (!g_log_file_state.file) {
        airy_mtx_unlock(&g_log_file_state.mutex);
        return;
    }

    char file_buffer[MAX_MESSAGE_LEN * 2];
    time_t sec = record->timestamp / 1000;
    int ms = (int)(record->timestamp % 1000);
    struct tm tm_storage;
    localtime_r(&sec, &tm_storage);
    const char *level_name = log_level_to_string(record->level);

    int len = snprintf(file_buffer, sizeof(file_buffer),
                       "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s:%d]",
                       tm_storage.tm_year + 1900, tm_storage.tm_mon + 1, tm_storage.tm_mday,
                       tm_storage.tm_hour, tm_storage.tm_min, tm_storage.tm_sec, ms, level_name,
                       record->module ? record->module : "?", record->line);
    if (len < 0) {
        airy_mtx_unlock(&g_log_file_state.mutex);
        return;
    }
    if ((size_t)len >= sizeof(file_buffer))
        len = (int)sizeof(file_buffer) - 1;

    if (record->trace_id && record->trace_id[0]) {
        int tlen = snprintf(file_buffer + len, sizeof(file_buffer) - (size_t)len, " [trace:%s]",
                            record->trace_id);
        if (tlen > 0)
            len += tlen;
        if ((size_t)len >= sizeof(file_buffer))
            len = (int)sizeof(file_buffer) - 1;
    }

    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fwrite(file_buffer, 1, (size_t)len, g_log_file_state.file);
    fwrite(" ", 1, 1, g_log_file_state.file);
    fwrite(record->message ? record->message : "", 1, record->message ? strlen(record->message) : 0,
           g_log_file_state.file);
    fwrite("\n", 1, 1, g_log_file_state.file);
    fflush(g_log_file_state.file);

    g_log_file_state.current_size +=
        (size_t)len + 1 + (record->message ? strlen(record->message) : 0) + 1;

    log_file_rotate_if_needed();
    airy_mtx_unlock(&g_log_file_state.mutex);
}

void log_internal_file_cleanup(void)
{
    if (g_log_file_state.mutex_init) {
        airy_mtx_lock(&g_log_file_state.mutex);
        if (g_log_file_state.file) {
            fflush(g_log_file_state.file);
            fclose(g_log_file_state.file);
            g_log_file_state.file = NULL;
        }
        g_log_file_state.current_size = 0;
        airy_mtx_unlock(&g_log_file_state.mutex);
        airy_mtx_destroy(&g_log_file_state.mutex);
        g_log_file_state.mutex_init = false;
    }
}
