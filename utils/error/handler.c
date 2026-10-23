// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file handler.c
 * @brief Unified error handling module implementation.
 *
 * Provides unified error handling:
 * - Error code description and severity management
 * - Error chain tracking and context management
 * - Multi-language error description support
 * - Error statistics and reporting
 */

#include "atomic_compat.h"
#include "error.h"
#include "error_catalog_internal.h"
#include "logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <time.h>

#ifdef _WIN32
#else
#include <sys/time.h>
#endif
#include "platform.h"

static airy_language_t g_current_language = AIRY_LANG_EN_US;

static airy_err_i18n_entry_t *g_i18n_entries = NULL;
static size_t g_i18n_entry_count = 0;

static struct {
    uint64_t total_errors;
    uint64_t errors_by_severity[4];
    uint64_t last_error_time;
    airy_err_t last_error;
} g_error_stats;

#ifdef _WIN32
static airy_mtx_t g_error_stats_mutex;
static atomic_int g_error_stats_initialized = 0;

static void ensure_stats_init(void)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_error_stats_initialized, &expected, 1,
                                                memory_order_acq_rel, memory_order_acquire) == 0) {
        airy_mtx_init(&g_error_stats_mutex);
    }
}

#define STATS_LOCK()                         \
    do {                                     \
        ensure_stats_init();                 \
        airy_mtx_lock(&g_error_stats_mutex); \
    } while (0)

#define STATS_UNLOCK()                         \
    do {                                       \
        airy_mtx_unlock(&g_error_stats_mutex); \
    } while (0)

#else
static airy_mtx_t g_error_stats_mutex = {0};

#define STATS_LOCK()                         \
    do {                                     \
        airy_mtx_lock(&g_error_stats_mutex); \
    } while (0)

#define STATS_UNLOCK()                         \
    do {                                       \
        airy_mtx_unlock(&g_error_stats_mutex); \
    } while (0)
#endif

typedef struct {
    airy_err_chain_t chain;
    int initialized;
} thread_error_state_t;

#ifdef _WIN32
static DWORD g_tls_index = TLS_OUT_OF_INDEXES;

static thread_error_state_t *get_thread_error_state(void)
{
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
        g_tls_index = TlsAlloc();
        if (g_tls_index == TLS_OUT_OF_INDEXES) {
            AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
        }
    }

    thread_error_state_t *state = (thread_error_state_t *)TlsGetValue(g_tls_index);
    if (state == NULL) {
        state = (thread_error_state_t *)AIRY_CALLOC(1, sizeof(thread_error_state_t));
        if (state != NULL) {
            state->initialized = 1;
            TlsSetValue(g_tls_index, state);
        }
    }
    return state;
}
#else
#include <pthread.h>

/* POSIX：pthread_key 析构器在线程退出时自动回收错误状态（含链上 message
 * 副本），库内全部线程入口无需逐一调用清理接口。析构器内不分配、不压
 * 错误，避免 pthread 反复重跑析构。 */
static pthread_key_t g_tls_error_key;
static pthread_once_t g_tls_error_once = PTHREAD_ONCE_INIT;

static void tls_state_destroy(void *p)
{
    thread_error_state_t *state = (thread_error_state_t *)p;
    if (state == NULL)
        return;
    for (int i = 0; i < state->chain.depth; i++) {
        AIRY_FREE((void *)state->chain.contexts[i].message);
        state->chain.contexts[i].message = NULL;
    }
    AIRY_FREE(state);
}

static void tls_key_create(void)
{
    (void)pthread_key_create(&g_tls_error_key, tls_state_destroy);
}

static thread_error_state_t *get_thread_error_state(void)
{
    (void)pthread_once(&g_tls_error_once, tls_key_create);
    thread_error_state_t *state =
        (thread_error_state_t *)pthread_getspecific(g_tls_error_key);
    if (state == NULL) {
        state = (thread_error_state_t *)AIRY_CALLOC(1, sizeof(thread_error_state_t));
        if (state != NULL) {
            state->initialized = 1;
            (void)pthread_setspecific(g_tls_error_key, state);
        }
    }
    return state;
}
#endif


const char *airy_err_str(airy_err_t code)
{
    /* S-1 收敛（2026-08-14）：输入码经 AIRY_ERR_NEG 归一（正幅值宏取负、
     * 负值原样），使 airy_err_str(AIRY_ENOMEM)（=9）与 airy_err_str(-9)
     * 均解析为同一描述（A-UEF 宏/返回值双视角一致）。 */
    airy_err_t norm = AIRY_ERR_NEG(code);
    const error_info_t *info = airy_err_find(norm);
    return info ? info->description_en : "Unknown error";
}

const char *airy_err_code_name(airy_err_t code)
{
    /* 与 airy_err_str 同一 AIRY_ERR_NEG 归一语义（S-1 收敛） */
    airy_err_t norm = AIRY_ERR_NEG(code);
    const error_info_t *info = airy_err_find(norm);
    return info ? info->name : "ERR_UNKNOWN";
}

airy_err_severity_t airy_err_get_severity(airy_err_t code)
{
    airy_err_t norm = AIRY_ERR_NEG(code);
    const error_info_t *info = airy_err_find(norm);
    return info ? info->severity : AIRY_ERR_SEVERITY_ERROR;
}

airy_err_chain_t *airy_err_get_chain(void)
{
    thread_error_state_t *state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }
    return &state->chain;
}

void airy_err_clear(void)
{
    thread_error_state_t *state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        return;
    }

    for (int i = 0; i < state->chain.depth; i++) {
        AIRY_FREE((void *)state->chain.contexts[i].message);
        state->chain.contexts[i].message = NULL;
    }
    state->chain.code = AIRY_OK;
    state->chain.depth = 0;
}

void airy_err_thread_cleanup(void)
{
#ifdef _WIN32
    if (g_tls_index != TLS_OUT_OF_INDEXES) {
        thread_error_state_t *state = (thread_error_state_t *)TlsGetValue(g_tls_index);
        if (state != NULL) {

            for (int i = 0; i < state->chain.depth; i++) {
                AIRY_FREE((void *)state->chain.contexts[i].message);
                state->chain.contexts[i].message = NULL;
            }
            AIRY_FREE(state);
            TlsSetValue(g_tls_index, NULL);
        }
    }
#else
    (void)pthread_once(&g_tls_error_once, tls_key_create);
    thread_error_state_t *state =
        (thread_error_state_t *)pthread_getspecific(g_tls_error_key);
    if (state != NULL) {
        tls_state_destroy(state);
        (void)pthread_setspecific(g_tls_error_key, NULL);
    }
#endif
}

void airy_err_push_ex(airy_err_t code, const char *file, int line, const char *func,
                      const char *fmt, ...)
{
    thread_error_state_t *state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        return;
    }

    airy_err_chain_t *chain = &state->chain;

    STATS_LOCK();
    g_error_stats.total_errors++;
    airy_err_severity_t severity = airy_err_get_severity(code);
    if (severity >= 0 && severity < 4) {
        g_error_stats.errors_by_severity[severity]++;
    }
    g_error_stats.last_error_time = airy_time_ns();
    g_error_stats.last_error = code;
    STATS_UNLOCK();

    char message_buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message_buffer, sizeof(message_buffer), fmt,
              args); /* flawfinder: ignore - variadic error handler with bounded buffer */
    va_end(args);

    if (chain->depth < AIRY_ERROR_CONTEXT_MAX_DEPTH) {
        airy_err_context_entry_t *entry = &chain->contexts[chain->depth];
        entry->file = file;
        entry->line = line;
        entry->function = func;
        entry->message = AIRY_STRDUP(message_buffer);
        entry->error_code = code;
        entry->timestamp_ns = airy_time_ns();
        chain->depth++;
    } else {

        AIRY_FREE((void *)chain->contexts[0].message);
        for (int i = 0; i < AIRY_ERROR_CONTEXT_MAX_DEPTH - 1; i++) {
            chain->contexts[i] = chain->contexts[i + 1];
        }
        airy_err_context_entry_t *entry = &chain->contexts[AIRY_ERROR_CONTEXT_MAX_DEPTH - 1];
        entry->file = file;
        entry->line = line;
        entry->function = func;
        entry->message = AIRY_STRDUP(message_buffer);
        entry->error_code = code;
        entry->timestamp_ns = airy_time_ns();
    }

    chain->code = code;
}

void airy_err_print_chain(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        AIRY_LOG_DEBUG("Error chain is NULL");
        return;
    }

    AIRY_LOG_DEBUG("Error chain (depth: %d, latest error: %d)", chain->depth, chain->code);
    for (int i = 0; i < chain->depth; i++) {
        const airy_err_context_entry_t *ctx = &chain->contexts[i];
        (void)ctx;
        AIRY_LOG_DEBUG("  [%d] %s:%d in %s() - %d: %s", i + 1, ctx->file ? ctx->file : "(unknown)",
                  ctx->line, ctx->function ? ctx->function : "(unknown)", ctx->error_code,
                  ctx->message ? ctx->message : "");
    }
}

char *airy_err_chain_to_json(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        return AIRY_STRDUP("{\"error\": \"null chain\"}");
    }

    return airy_err_chain_to_json_i18n(chain, -1);
}

void airy_err_get_stats(airy_err_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }

    STATS_LOCK();
    stats->total_errors = g_error_stats.total_errors;
    for (int i = 0; i < 4; i++) {
        stats->errors_by_code[i] = g_error_stats.errors_by_severity[i];
    }
    stats->last_error_time = g_error_stats.last_error_time;
    stats->last_error = g_error_stats.last_error;
    STATS_UNLOCK();
}

void airy_err_reset_stats(void)
{
    STATS_LOCK();
    g_error_stats.total_errors = 0;
    for (int i = 0; i < 4; i++) {
        g_error_stats.errors_by_severity[i] = 0;
    }
    g_error_stats.last_error_time = 0;
    g_error_stats.last_error = AIRY_OK;
    STATS_UNLOCK();
}

airy_err_t airy_err_set_language(airy_language_t lang)
{
    if ((int)lang < 0 || (int)lang > 7) {
        return AIRY_ERR_INVALID_PARAM;
    }

    g_current_language = lang;
    return AIRY_OK;
}

airy_language_t airy_err_get_language(void)
{
    return g_current_language;
}

const char *airy_err_str_i18n(airy_err_t code, airy_language_t lang)
{
    airy_language_t use_lang = lang;
    if ((int)lang < 0) {
        use_lang = g_current_language;
    }

    if ((int)use_lang < 0 || (int)use_lang > 7) {
        use_lang = AIRY_LANG_EN_US;
    }

    /* S-1 收敛（2026-08-14）：与 airy_err_str 一致，输入码经 AIRY_ERR_NEG
     * 归一（正幅值宏取负），保证宏/返回值双视角均命中同一描述。 */
    airy_err_t norm = AIRY_ERR_NEG(code);

    for (size_t i = 0; i < g_i18n_entry_count; i++) {
        if (g_i18n_entries[i].error_code == norm) {
            const char *desc = g_i18n_entries[i].descriptions[use_lang];
            if (desc != NULL) {
                return desc;
            }
        }
    }

    const error_info_t *info = airy_err_find(norm);
    if (info != NULL) {
        switch (use_lang) {
        case AIRY_LANG_ZH_CN:
            return info->description_zh_cn ? info->description_zh_cn : info->description_en;
        default:
            return info->description_en;
        }
    }

    return airy_err_str(code);
}

airy_err_t airy_err_register_i18n(const airy_err_i18n_entry_t *entries, size_t count)
{

    if (entries == NULL || count == 0) {
        return AIRY_ERR_INVALID_PARAM;
    }

    if (g_i18n_entries != NULL) {
        AIRY_FREE(g_i18n_entries);
        g_i18n_entries = NULL;
        g_i18n_entry_count = 0;
    }

    g_i18n_entries = airy_malloc_array(count, sizeof(airy_err_i18n_entry_t));
    if (g_i18n_entries == NULL) {
        return AIRY_ERR_OUT_OF_MEMORY;
    }

    __builtin_memcpy(g_i18n_entries, entries, count * sizeof(airy_err_i18n_entry_t));
    g_i18n_entry_count = count;

    return AIRY_OK;
}

char *airy_err_chain_to_json_i18n(const airy_err_chain_t *chain, airy_language_t lang)
{

    if (!chain) {
        return AIRY_STRDUP("{\"error\": \"null\"}");
    }

    airy_language_t use_lang = lang;
    if ((int)lang < 0) {
        use_lang = g_current_language;
    }

    size_t buf_size = 4096;
    char *buf = (char *)AIRY_MALLOC(buf_size);
    if (!buf) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    size_t offset = 0;
    int n = snprintf(buf, buf_size,
                     "{\"code\": %d, \"message\": \"%s\", \"depth\": %d, \"contexts\": [",
                     chain->code, airy_err_str_i18n(chain->code, use_lang), chain->depth);
    /* snprintf returns the number of chars "that would have been written",
     * possibly >= remaining space; offset must always stay <= buf_size,
     * otherwise buf_size - offset underflows and writes out of bounds
     * (same protection pattern as AIRY_REPORT_APPEND in resource_guard.c). */
    if (n < 0) {
        offset = buf_size;
    } else if ((size_t)n >= buf_size) {
        offset = buf_size;
    } else {
        offset = (size_t)n;
    }

    for (int i = 0; i < chain->depth && offset < buf_size; i++) {
        const airy_err_context_entry_t *ctx = &chain->contexts[i];

        char escaped_msg[2048] = {0};
        const char *msg = ctx->message ? ctx->message : "";
        for (size_t j = 0, k = 0; j < strlen(msg) && k < sizeof(escaped_msg) - 1; j++) {
            if (msg[j] == '"' || msg[j] == '\\') {
                escaped_msg[k++] = '\\';
            }
            escaped_msg[k++] = msg[j];
        }
        escaped_msg[sizeof(escaped_msg) - 1] = '\0';

        n = snprintf(buf + offset, buf_size - offset,
                     "%s{\"file\": \"%s\", \"line\": %d, \"function\": \"%s\", \"code\": %d, "
                     "\"message\": \"%s\"}",
                     i > 0 ? ", " : "", ctx->file ? ctx->file : "", ctx->line,
                     ctx->function ? ctx->function : "", ctx->error_code, escaped_msg);
        if (n < 0) {
            offset = buf_size;
        } else if ((size_t)n >= buf_size - offset) {
            offset = buf_size;
        } else {
            offset += (size_t)n;
        }
    }

    if (offset < buf_size) {
        n = snprintf(buf + offset, buf_size - offset, "]}");
        if (n < 0 || (size_t)n >= buf_size - offset) {
            offset = buf_size;
        } else {
            offset += (size_t)n;
        }
    }

    buf[buf_size - 1] = '\0';

    return buf;
}

void airy_err_chain_iter_init(const airy_err_chain_t *chain, airy_err_chain_iterator_t *iter)
{

    if (!iter)
        return;

    iter->chain = chain;
    iter->current_index = 0;
}

const airy_err_context_entry_t *airy_err_chain_iter_next(airy_err_chain_iterator_t *iter)
{

    if (!iter || !iter->chain) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if ((size_t)iter->current_index >= (size_t)iter->chain->depth) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    const airy_err_context_entry_t *ctx = &iter->chain->contexts[iter->current_index];
    iter->current_index++;

    return ctx;
}

void airy_err_chain_iter_reset(airy_err_chain_iterator_t *iter)
{
    if (!iter)
        return;

    iter->current_index = 0;
}

int airy_err_chain_get_depth(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        return 0;
    }
    return chain->depth;
}

airy_err_t airy_err_chain_get_root_error(const airy_err_chain_t *chain)
{
    if (chain == NULL || chain->depth <= 0) {
        return AIRY_OK;
    }
    return chain->contexts[0].error_code;
}

airy_err_t airy_err_latest(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        return AIRY_OK;
    }
    return chain->code;
}

char *airy_err_chain_format(const airy_err_chain_t *chain, airy_language_t lang)
{

    if (chain == NULL) {
        return AIRY_STRDUP("(null chain)");
    }

    size_t buf_size = 4096;
    char *buf = (char *)AIRY_MALLOC(buf_size);
    if (buf == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "Error chain (depth=%d, code=%d):\n",
                       chain->depth, chain->code);

    for (int i = 0; i < chain->depth && offset < buf_size - 1; i++) {
        const airy_err_context_entry_t *ctx = &chain->contexts[i];
        offset += snprintf(buf + offset, buf_size - offset, "  [%d] %s:%d in %s(): %s\n", i + 1,
                           ctx->file ? ctx->file : "?", ctx->line,
                           ctx->function ? ctx->function : "?", ctx->message ? ctx->message : "");
    }

    (void)lang;
    return buf;
}


