// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file trace.c
 * @brief 链路追踪实现（跨平台）
 *
 * @details
 * 本模块实现分布式链路追踪功能。
 * - 支持 Span 的创建和生命周期管理
 * - 提供事件注解和属性添加
 * - 支持 JSON 格式的追踪导出
 * - 符合 OpenTelemetry 追踪规范
 */

#include "trace.h"

#include "observability.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <ctype.h>
#include <string.h>
#include <time.h>

#include "atomic_compat.h"
#include "platform.h"
#include "error.h"

#ifndef _WIN32
#include <stdint.h>
#include <unistd.h>
#endif

#define MAX_SPANS 1024
#define MAX_EVENTS_PER_SPAN 64
#define MAX_TRACE_ID_LEN 64
#define MAX_SPAN_ID_LEN 32

typedef struct trace_event {
    char name[128];
    int64_t timestamp;
    char attributes[512];
    struct trace_event *next;
} trace_event_t;

typedef airy_mtx_t trace_mutex_t;

static int trace_mutex_init(trace_mutex_t *mutex)
{
    return airy_mtx_init(mutex);
}

static void trace_mutex_destroy(trace_mutex_t *mutex)
{
    airy_mtx_destroy(mutex);
}

static void trace_mutex_lock(trace_mutex_t *mutex)
{
    airy_mtx_lock(mutex);
}

static void trace_mutex_unlock(trace_mutex_t *mutex)
{
    airy_mtx_unlock(mutex);
}

struct airy_trace_span {
    char trace_id[MAX_TRACE_ID_LEN];
    char span_id[MAX_SPAN_ID_LEN]; /**< Span ID */
    char parent_id[MAX_SPAN_ID_LEN];
    char name[128];
    int64_t start_time;
    int64_t end_time;
    atomic_int status;
    trace_event_t *events;
    trace_event_t *events_tail;
    int event_count;
    trace_mutex_t mutex;
    struct airy_trace_span *next;
};

static struct {
    atomic_uint64_t span_counter;
    atomic_uint64_t trace_counter;
    airy_trace_span_t *head;
    airy_trace_span_t *tail;
    trace_mutex_t mutex;
    int initialized;
} g_trace_state;

static int64_t get_current_time_us(void)
{
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (int64_t)((uli.QuadPart - 116444736000000000LL) / 10);
#else
    return (int64_t)(airy_time_ns() / 1000);
#endif
}

static int init_trace_system(void)
{
    if (g_trace_state.initialized) {
        return 0;
    }

    if (trace_mutex_init(&g_trace_state.mutex) != 0) {
        return AIRY_EINVAL;
    }

    atomic_init(&g_trace_state.span_counter, 0);
    atomic_init(&g_trace_state.trace_counter, 0);
    g_trace_state.head = NULL;
    g_trace_state.tail = NULL;
    g_trace_state.initialized = 1;
    return 0;
}

static void generate_id(char *buffer, size_t size, uint64_t counter, const char *prefix)
{
    snprintf(buffer, size, "%s-%016llx-%08llx", prefix, (unsigned long long)time(NULL),
             (unsigned long long)counter);
}

static trace_event_t *create_event(const char *name, const char *attributes)
{
    trace_event_t *event = (trace_event_t *)AIRY_MALLOC(sizeof(trace_event_t));
    if (!event) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    AIRY_MEMSET(event, 0, sizeof(trace_event_t));

    if (name) {
        AIRY_STRNCPY_TERM(event->name, name, sizeof(event->name));
        event->name[sizeof(event->name) - 1] = '\0';
    }

    event->timestamp = get_current_time_us();

    if (attributes) {
        AIRY_STRNCPY_TERM(event->attributes, attributes, sizeof(event->attributes));
        event->attributes[sizeof(event->attributes) - 1] = '\0';
    }

    return event;
}

static void free_events(trace_event_t *head)
{
    while (head) {
        trace_event_t *next = head->next;
        AIRY_FREE(head);
        head = next;
    }
}

airy_trace_span_t *airy_trace_begin(const char *name, const char *parent_id)
{
    if (!name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (init_trace_system() != 0) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    airy_trace_span_t *span = (airy_trace_span_t *)AIRY_MALLOC(sizeof(airy_trace_span_t));
    if (!span) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    AIRY_MEMSET(span, 0, sizeof(airy_trace_span_t));

    uint64_t trace_id = atomic_fetch_add(&g_trace_state.trace_counter, 1);
    uint64_t span_id = atomic_fetch_add(&g_trace_state.span_counter, 1);

    generate_id(span->trace_id, sizeof(span->trace_id), trace_id, "tr");
    generate_id(span->span_id, sizeof(span->span_id), span_id, "sp");

    if (parent_id) {
        AIRY_STRNCPY_TERM(span->parent_id, parent_id, sizeof(span->parent_id));
        span->parent_id[sizeof(span->parent_id) - 1] = '\0';
    } else {
        span->parent_id[0] = '\0';
    }

    AIRY_STRNCPY_TERM(span->name, name, sizeof(span->name));
    span->name[sizeof(span->name) - 1] = '\0';

    span->start_time = get_current_time_us();
    span->end_time = 0;
    atomic_init(&span->status, 0);
    span->events = NULL;
    span->events_tail = NULL;
    span->event_count = 0;

    if (trace_mutex_init(&span->mutex) != 0) {
        AIRY_FREE(span);
        AIRY_ERROR_NULL(AIRY_ERR_OVERFLOW, "limit exceeded");
    }

    trace_mutex_lock(&g_trace_state.mutex);

    span->next = NULL;
    if (g_trace_state.tail) {
        g_trace_state.tail->next = span;
        g_trace_state.tail = span;
    } else {
        g_trace_state.head = span;
        g_trace_state.tail = span;
    }

    trace_mutex_unlock(&g_trace_state.mutex);

    return span;
}

void airy_trace_end(airy_trace_span_t *span)
{
    if (!span) {
        return;
    }

    trace_mutex_lock(&span->mutex);

    if (atomic_load(&span->status) != 0) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    span->end_time = get_current_time_us();
    atomic_store(&span->status, 1);

    trace_mutex_unlock(&span->mutex);
}

void airy_trace_add_event(airy_trace_span_t *span, const char *name, const char *attributes)
{
    if (!span || !name) {
        return;
    }

    trace_mutex_lock(&span->mutex);

    if (atomic_load(&span->status) == 2) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    if (span->event_count >= MAX_EVENTS_PER_SPAN) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    trace_event_t *event = create_event(name, attributes);
    if (!event) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    if (span->events_tail) {
        span->events_tail->next = event;
        span->events_tail = event;
    } else {
        span->events = event;
        span->events_tail = event;
    }

    span->event_count++;

    trace_mutex_unlock(&span->mutex);
}

char *airy_trace_export(void)
{
    if (init_trace_system() != 0) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    trace_mutex_lock(&g_trace_state.mutex);

    size_t buffer_size = 4096;
    char *buffer = (char *)AIRY_MALLOC(buffer_size);
    if (!buffer) {
        trace_mutex_unlock(&g_trace_state.mutex);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, "[\n");

    airy_trace_span_t *span = g_trace_state.head;
    int first = 1;

    while (span) {
        trace_mutex_lock(&span->mutex);

        if (!first) {
            offset += snprintf(buffer + offset, buffer_size - offset, ",\n");
        }
        first = 0;

        offset += snprintf(buffer + offset, buffer_size - offset, "  {\n");
        offset += snprintf(buffer + offset, buffer_size - offset, "    \"trace_id\": \"%s\",\n",
                           span->trace_id);
        offset += snprintf(buffer + offset, buffer_size - offset, "    \"span_id\": \"%s\",\n",
                           span->span_id);

        if (span->parent_id[0]) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                               "    \"parent_id\": \"%s\",\n", span->parent_id);
        }

        offset +=
            snprintf(buffer + offset, buffer_size - offset, "    \"name\": \"%s\",\n", span->name);
        offset += snprintf(buffer + offset, buffer_size - offset, "    \"start_time\": %lld,\n",
                           (long long)span->start_time);

        if (span->end_time > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, "    \"end_time\": %lld,\n",
                               (long long)span->end_time);
            offset +=
                snprintf(buffer + offset, buffer_size - offset, "    \"duration_us\": %lld,\n",
                         (long long)(span->end_time - span->start_time));
        }

        offset += snprintf(buffer + offset, buffer_size - offset, "    \"status\": \"%s\",\n",
                           atomic_load(&span->status) == 1 ? "ok" :
                           atomic_load(&span->status) == 2 ? "error" :
                                                             "running");

        if (span->event_count > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, "    \"events\": [\n");

            trace_event_t *event = span->events;
            int first_event = 1;
            while (event) {
                if (!first_event) {
                    offset += snprintf(buffer + offset, buffer_size - offset, ",\n");
                }
                first_event = 0;

                offset += snprintf(buffer + offset, buffer_size - offset,
                                   "      {\"name\": \"%s\", \"timestamp\": %lld", event->name,
                                   (long long)event->timestamp);

                if (event->attributes[0]) {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                       ", \"attributes\": %s", event->attributes);
                }

                offset += snprintf(buffer + offset, buffer_size - offset, "}");
                event = event->next;
            }

            offset += snprintf(buffer + offset, buffer_size - offset, "\n    ]");
        }

        offset += snprintf(buffer + offset, buffer_size - offset, "\n  }");

        trace_mutex_unlock(&span->mutex);

        span = span->next;

        if (buffer_size > 512 && offset >= buffer_size - 512) {
            buffer_size *= 2;
            char *new_buffer = (char *)AIRY_REALLOC(buffer, buffer_size);
            if (!new_buffer) {
                trace_mutex_unlock(&g_trace_state.mutex);
                AIRY_FREE(buffer);
                AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
            }
            buffer = new_buffer;
        }
    }

    offset += snprintf(buffer + offset, buffer_size - offset, "\n]");

    trace_mutex_unlock(&g_trace_state.mutex);

    return buffer;
}

void airy_trace_cleanup(void)
{
    if (!g_trace_state.initialized) {
        return;
    }

    trace_mutex_lock(&g_trace_state.mutex);

    airy_trace_span_t *span = g_trace_state.head;
    while (span) {
        airy_trace_span_t *next = span->next;

        trace_mutex_lock(&span->mutex);
        span->end_time = get_current_time_us();
        atomic_store(&span->status, 1);
        free_events(span->events);
        trace_mutex_unlock(&span->mutex);

        trace_mutex_destroy(&span->mutex);
        AIRY_FREE(span);

        span = next;
    }

    g_trace_state.head = NULL;
    g_trace_state.tail = NULL;

    trace_mutex_unlock(&g_trace_state.mutex);
}

int airy_trace_get_span_count(void)
{
    if (!g_trace_state.initialized) {
        return 0;
    }

    trace_mutex_lock(&g_trace_state.mutex);

    int count = 0;
    airy_trace_span_t *span = g_trace_state.head;
    while (span) {
        count++;
        span = span->next;
    }

    trace_mutex_unlock(&g_trace_state.mutex);

    return count;
}

const char *airy_trace_span_get_trace_id(const airy_trace_span_t *span)
{
    return span ? span->trace_id : NULL;
}

const char *airy_trace_span_get_span_id(const airy_trace_span_t *span)
{
    return span ? span->span_id : NULL;
}

const char *airy_trace_span_get_parent_id(const airy_trace_span_t *span)
{
    return span ? span->parent_id : NULL;
}

const char *airy_trace_span_get_name(const airy_trace_span_t *span)
{
    return span ? span->name : NULL;
}

int64_t airy_tspan_get_start_time_us(const airy_trace_span_t *span)
{
    return span ? span->start_time : 0;
}

int64_t airy_tspan_get_end_time_us(const airy_trace_span_t *span)
{
    return span ? span->end_time : 0;
}

/* 历史命名差异：头文件以 airy_trace_span_get_* 声明（telemetry.c 等按此调用），
 * 实现长期以 airy_tspan_get_* 定义。此处补全长名定义，保证链接与 API 一致。 */
int64_t airy_trace_span_get_start_time_us(const airy_trace_span_t *span)
{
    return airy_tspan_get_start_time_us(span);
}

int64_t airy_trace_span_get_end_time_us(const airy_trace_span_t *span)
{
    return airy_tspan_get_end_time_us(span);
}

int airy_trace_span_get_status(const airy_trace_span_t *span)
{
    return span ? atomic_load(&span->status) : 0;
}
