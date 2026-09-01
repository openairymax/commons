// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file logging_core.c
 * @brief Unified layered logging system - core layer implementation.
 *
 * 2026-08-27 域拆分（原 logging.c）：日志核心——级别管理、系统初始化、
 * 基础写入路径（控制台 + 文件后端）、trace/span ID（TLS）、模块级过滤、
 * 生命周期清理。格式化见 logging_format.c，文件后端见
 * logging_backend_file.c，节流/采样/热加载见 logging_control.c。
 */

#include "logging_internal.h"

static AIRY_THREAD_LOCAL char g_tls_trace_id[128] = {0};
static AIRY_THREAD_LOCAL char g_tls_span_id[64] = {0};

static const char *LEVEL_NAMES[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const size_t LEVEL_NAMES_COUNT = sizeof(LEVEL_NAMES) / sizeof(LEVEL_NAMES[0]);

static bool g_log_use_color = true;

static const log_level_t DEFAULT_LOG_LEVEL = LOG_LEVEL_INFO;

static const log_format_t DEFAULT_LOG_FORMAT = LOG_FORMAT_TEXT;

static logging_state_t g_logging_state = {.initialized = false, .module_level_count = 0};

/* ==================== Internal accessors ==================== */

logging_state_t *log_internal_state(void)
{
    return &g_logging_state;
}

const char *const *log_internal_level_names(size_t *count)
{
    if (count)
        *count = LEVEL_NAMES_COUNT;
    return LEVEL_NAMES;
}

bool log_internal_color_enabled(void)
{
    return g_log_use_color;
}

/* ==================== Context helpers ==================== */

static uint64_t get_current_timestamp(void)
{
#if defined(_WIN32)
    /* Windows 无 clock_gettime：GetSystemTimeAsFileTime 返回 1601-01-01
     * 起的 100ns 间隔，换算到 Unix 毫秒（偏移 11644473600000 ms）。 */
    FILETIME ft;
    ULARGE_INTEGER u;
    GetSystemTimeAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (uint64_t)((u.QuadPart / 10000ULL) - 11644473600000ULL);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

static uint64_t get_current_thread_id(void)
{
    return airy_thread_id();
}

static uint32_t get_current_process_id(void)
{
#if defined(_WIN32)
    return (uint32_t)GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

/* ==================== Level & module filtering ==================== */

static bool should_log(log_level_t level, const char *module)
{
    if (level < g_logging_state.manager.level) {
        return false;
    }

    if (!module)
        return true;

    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        const char *pattern = g_logging_state.module_levels[i].pattern;
        if (pattern[0] == '*') {
            size_t plen = strlen(pattern);
            if (plen == 1)
                return level >= g_logging_state.module_levels[i].level;
            const char *suffix = pattern + 1;
            size_t slen = strlen(suffix);
            size_t mlen = strlen(module);
            if (mlen >= slen && strcmp(module + mlen - slen, suffix) == 0)
                return level >= g_logging_state.module_levels[i].level;
        } else if (strcmp(pattern, module) == 0) {
            return level >= g_logging_state.module_levels[i].level;
        }
    }

    return true;
}

const char *log_level_to_string(log_level_t level)
{
    if (level >= 0 && level < LEVEL_NAMES_COUNT) {
        return LEVEL_NAMES[level];
    }
    return "UNKNOWN";
}

log_level_t log_level_from_string(const char *str)
{
    if (!str) {
        return DEFAULT_LOG_LEVEL;
    }

    for (size_t i = 0; i < LEVEL_NAMES_COUNT; i++) {
        if (strcasecmp(str, LEVEL_NAMES[i]) == 0) {
            return (log_level_t)i;
        }
    }

    char *endptr;
    long value = strtol(str, &endptr, 10);
    if (endptr != str && *endptr == '\0' && value >= 0 && (size_t)value < LEVEL_NAMES_COUNT) {
        return (log_level_t)value;
    }

    return DEFAULT_LOG_LEVEL;
}

/* ==================== Lifecycle ==================== */

int log_init(const log_config_t *manager)
{
    if (g_logging_state.initialized) {
        return 0;
    }

    if (airy_mtx_init(&g_logging_state.mutex) != 0) {
        return AIRY_EINVAL;
    }

    log_internal_throttle_init_mutex();
    log_file_state_t *fs = log_internal_file_state();
    if (!fs->mutex_init) {
        if (airy_mtx_init(&fs->mutex) == 0) {
            fs->mutex_init = true;
        }
    }

    g_tls_trace_id[0] = '\0';
    g_tls_span_id[0] = '\0';

    {
        const char *env_color = getenv("AIRY_LOG_COLOR");
        if (env_color) {

            if (strcmp(env_color, "0") == 0 || strcmp(env_color, "no") == 0 ||
                strcmp(env_color, "false") == 0 || strcmp(env_color, "off") == 0 ||
                strcmp(env_color, "never") == 0) {
                g_log_use_color = false;
            } else {
                g_log_use_color = true;
            }
        } else {

            g_log_use_color =
                log_internal_is_terminal(STDOUT_FILENO) || log_internal_is_terminal(STDERR_FILENO);
        }
    }

    if (manager) {
        __builtin_memcpy(&g_logging_state.manager, manager, sizeof(log_config_t));
    } else {
        g_logging_state.manager.level = DEFAULT_LOG_LEVEL;
        g_logging_state.manager.outputs = 1 << LOG_OUTPUT_CONSOLE;
        g_logging_state.manager.format = DEFAULT_LOG_FORMAT;
        g_logging_state.manager.async_mode = false;
        g_logging_state.manager.enable_statistics = false;
    }

    {
        const char *env_level = getenv("AIRY_LOG_LEVEL");
        if (env_level && env_level[0]) {
            g_logging_state.manager.level = log_level_from_string(env_level);
        }
    }

    if ((g_logging_state.manager.outputs & (1 << LOG_OUTPUT_FILE)) &&
        g_logging_state.manager.file_path && fs->mutex_init) {
        if (log_internal_file_open(g_logging_state.manager.file_path) != 0) {

            g_logging_state.manager.outputs &= ~(1 << LOG_OUTPUT_FILE);
        }
    }

    g_logging_state.initialized = true;

    return 0;
}

int log_set_default_config(const log_config_t *manager)
{
    if (!manager) {
        return AIRY_EINVAL;
    }

    airy_mtx_lock(&g_logging_state.mutex);
    __builtin_memcpy(&g_logging_state.default_config, manager, sizeof(log_config_t));
    airy_mtx_unlock(&g_logging_state.mutex);

    return 0;
}

/* ==================== Write path ==================== */

void log_write(log_level_t level, const char *module, int line, const char *fmt, ...)
{
    if (!g_logging_state.initialized) {
        log_init(NULL);
    }

    if (!should_log(level, module)) {
        return;
    }

    const char *trace_id = log_get_trace_id();
    const char *span_id = log_get_span_id();

    char message_buffer[MAX_MESSAGE_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message_buffer, sizeof(message_buffer), fmt,
              args); /* flawfinder: ignore - variadic logging wrapper */
    va_end(args);

    uint64_t now_sec = (uint64_t)(get_current_timestamp() / 1000);
    if (log_internal_throttle_suppress(module, line, message_buffer, now_sec)) {
        return;
    }

    log_record_t record = {.timestamp = get_current_timestamp(),
                           .level = level,
                           .module = module,
                           .line = line,
                           .trace_id = trace_id,
                           .span_id = span_id,
                           .message = message_buffer,
                           .thread_id = get_current_thread_id(),
                           .process_id = get_current_process_id()};

    char formatted_buffer[MAX_MESSAGE_LEN * 2];
    size_t formatted_len =
        log_internal_format_message(&record, formatted_buffer, sizeof(formatted_buffer));

    if (formatted_len > 0) {
        FILE *stream = stderr;
        (void)level;
        fwrite(formatted_buffer, 1, formatted_len, stream);
        fflush(stream);
    }

    if (g_logging_state.manager.outputs & (1 << LOG_OUTPUT_FILE)) {
        log_internal_file_write(&record, formatted_buffer, formatted_len);
    }
}

void log_write_va(log_level_t level, const char *module, int line, const char *fmt, va_list args)
{
    if (!g_logging_state.initialized) {
        log_init(NULL);
    }

    if (!should_log(level, module)) {
        return;
    }

    const char *trace_id = log_get_trace_id();
    const char *span_id = log_get_span_id();

    char message_buffer[MAX_MESSAGE_LEN];
    vsnprintf(message_buffer, sizeof(message_buffer), fmt,
              args); /* flawfinder: ignore - variadic logging wrapper */
    log_record_t record = {.timestamp = get_current_timestamp(),
                           .level = level,
                           .module = module,
                           .line = line,
                           .trace_id = trace_id,
                           .span_id = span_id,
                           .message = message_buffer,
                           .thread_id = get_current_thread_id(),
                           .process_id = get_current_process_id()};

    char formatted_buffer[MAX_MESSAGE_LEN * 2];
    size_t formatted_len =
        log_internal_format_message(&record, formatted_buffer, sizeof(formatted_buffer));

    if (formatted_len > 0) {
        FILE *stream = stderr;
        (void)level;
        fwrite(formatted_buffer, 1, formatted_len, stream);
        fflush(stream);
    }

    if (g_logging_state.manager.outputs & (1 << LOG_OUTPUT_FILE)) {
        log_internal_file_write(&record, formatted_buffer, formatted_len);
    }
}

/* ==================== Trace / span ID (TLS) ==================== */

const char *log_set_trace_id(const char *trace_id)
{
    if (!g_logging_state.initialized)
        return NULL;

    if (trace_id) {
        AIRY_STRNCPY_TERM(g_tls_trace_id, trace_id, sizeof(g_tls_trace_id));
        g_tls_trace_id[sizeof(g_tls_trace_id) - 1] = '\0';
    } else {
        g_tls_trace_id[0] = '\0';
    }

    return g_tls_trace_id;
}

const char *log_get_trace_id(void)
{
    if (!g_logging_state.initialized)
        return NULL;
    return g_tls_trace_id[0] ? g_tls_trace_id : NULL;
}

const char *log_set_span_id(const char *span_id)
{
    if (!g_logging_state.initialized)
        return NULL;

    if (span_id) {
        AIRY_STRNCPY_TERM(g_tls_span_id, span_id, sizeof(g_tls_span_id));
        g_tls_span_id[sizeof(g_tls_span_id) - 1] = '\0';
    } else {
        g_tls_span_id[0] = '\0';
    }

    return g_tls_span_id;
}

const char *log_get_span_id(void)
{
    if (!g_logging_state.initialized)
        return NULL;
    return g_tls_span_id[0] ? g_tls_span_id : NULL;
}

/* ==================== Module level management ==================== */

int log_set_module_level(const char *module_pattern, log_level_t level)
{
    if (!g_logging_state.initialized || !module_pattern) {
        return AIRY_EINVAL;
    }

    airy_mtx_lock(&g_logging_state.mutex);

    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        if (strcmp(g_logging_state.module_levels[i].pattern, module_pattern) == 0) {
            g_logging_state.module_levels[i].level = level;
            airy_mtx_unlock(&g_logging_state.mutex);
            return 0;
        }
    }

    if (g_logging_state.module_level_count <
        sizeof(g_logging_state.module_levels) / sizeof(g_logging_state.module_levels[0])) {
        AIRY_STRNCPY_TERM(g_logging_state.module_levels[g_logging_state.module_level_count].pattern,
                          module_pattern, sizeof(g_logging_state.module_levels[0].pattern));
        g_logging_state.module_levels[g_logging_state.module_level_count]
            .pattern[sizeof(g_logging_state.module_levels[0].pattern) - 1] = '\0';
        g_logging_state.module_levels[g_logging_state.module_level_count].level = level;
        g_logging_state.module_level_count++;
        airy_mtx_unlock(&g_logging_state.mutex);
        return 0;
    }

    airy_mtx_unlock(&g_logging_state.mutex);
    return AIRY_ERR_NOT_FOUND;
}

size_t log_get_module_count(void)
{
    if (!g_logging_state.initialized) {
        return 0;
    }

    airy_mtx_lock(&g_logging_state.mutex);
    size_t count = g_logging_state.module_level_count;
    airy_mtx_unlock(&g_logging_state.mutex);
    return count;
}

size_t log_get_module_info(log_module_info_t *out_info, size_t max_count)
{
    if (!g_logging_state.initialized || out_info == NULL || max_count == 0) {
        return 0;
    }

    airy_mtx_lock(&g_logging_state.mutex);
    size_t copy_count = g_logging_state.module_level_count;
    if (copy_count > max_count) {
        copy_count = max_count;
    }
    for (size_t i = 0; i < copy_count; i++) {
        AIRY_STRNCPY_TERM(out_info[i].pattern, g_logging_state.module_levels[i].pattern,
                          sizeof(out_info[i].pattern));
        out_info[i].pattern[sizeof(out_info[i].pattern) - 1] = '\0';
        out_info[i].level = g_logging_state.module_levels[i].level;
    }
    airy_mtx_unlock(&g_logging_state.mutex);
    return copy_count;
}

/* ==================== Flush & cleanup ==================== */

void log_flush(void)
{
    fflush(stdout);
    fflush(stderr);

    log_file_state_t *fs = log_internal_file_state();
    if (fs->file) {
        fflush(fs->file);
    }
}

void log_cleanup(void)
{
    if (!g_logging_state.initialized) {
        return;
    }

    airy_mtx_lock(&g_logging_state.mutex);

    g_tls_trace_id[0] = '\0';
    g_tls_span_id[0] = '\0';

    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        AIRY_MEMSET(&g_logging_state.module_levels[i], 0, sizeof(g_logging_state.module_levels[i]));
    }
    g_logging_state.module_level_count = 0;

    g_logging_state.initialized = false;

    airy_mtx_unlock(&g_logging_state.mutex);
    airy_mtx_destroy(&g_logging_state.mutex);

    log_internal_throttle_cleanup();
    log_internal_file_cleanup();

    AIRY_MEMSET(&g_logging_state, 0, sizeof(g_logging_state));
}
