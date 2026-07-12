// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file resource_guard.c
 * @brief 资源作用域守卫实?- RAII 模式
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "resource_guard.h"

#include "../memory/include/airy_memory.h"
#include "../string/include/airy_string.h"
#include "../sync/include/sync.h"
#include "atomic_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 核心接口实现 ==================== */

void airy_resource_guard_init(airy_resource_guard_t *guard, void *resource,
                                 airy_resource_cleanup_t cleanup, const char *file, int line,
                                 const char *name)
{
    if (!guard) {
        return;
    }

    guard->resource = resource;
    guard->cleanup = cleanup;
    guard->file = file;
    guard->line = line;
    guard->name = name;
    guard->active = 1;
}

void airy_resource_guard_cleanup(airy_resource_guard_t *guard)
{
    if (!guard || !guard->active) {
        return;
    }

    if (guard->cleanup && guard->resource) {
        guard->cleanup(guard->resource);
    }

    guard->active = 0;
    guard->resource = NULL;
    guard->cleanup = NULL;
}

void airy_resource_guard_dismiss(airy_resource_guard_t *guard)
{
    if (!guard) {
        return;
    }

    guard->active = 0;
}

/* ==================== 资源追踪实现 ==================== */

#ifdef AIRY_RESOURCE_TRACKING

#include "platform.h"

#include <stdint.h>

typedef struct airy_resource_record {
    void *resource;
    const char *type;
    const char *file;
    int line;
    uint64_t timestamp_ns;
    struct airy_resource_record *next;
} airy_resource_record_t;

static airy_resource_record_t *g_resource_head = NULL;
static airy_mtx_t g_resource_mutex;
static atomic_int g_resource_mutex_initialized = 0;

static void ensure_mutex_initialized(void)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_resource_mutex_initialized, &expected, 1,
                                                memory_order_seq_cst, memory_order_seq_cst)) {
        airy_mtx_init(&g_resource_mutex);
    }
}

static uint64_t get_monotonic_ns(void)
{
    return airy_time_ns();
}

void airy_resource_track_alloc(void *resource, const char *type, const char *file, int line)
{
    if (!resource) {
        return;
    }

    airy_resource_record_t *record = (airy_resource_record_t *)memory_alloc(
        sizeof(airy_resource_record_t), "resource_record");
    if (!record) {
        return;
    }

    record->resource = resource;
    record->type = type;
    record->file = file;
    record->line = line;
    record->timestamp_ns = get_monotonic_ns();

    ensure_mutex_initialized();
    airy_mtx_lock(&g_resource_mutex);
    record->next = g_resource_head;
    g_resource_head = record;
    airy_mtx_unlock(&g_resource_mutex);
}

void airy_resource_track_free(void *resource)
{
    if (!resource) {
        return;
    }

    ensure_mutex_initialized();
    airy_mtx_lock(&g_resource_mutex);

    airy_resource_record_t *prev = NULL;
    airy_resource_record_t *curr = g_resource_head;

    while (curr) {
        if (curr->resource == resource) {
            if (prev) {
                prev->next = curr->next;
            } else {
                g_resource_head = curr->next;
            }
            memory_free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    airy_mtx_unlock(&g_resource_mutex);
}

int airy_resource_track_report(char **out_report)
{
    ensure_mutex_initialized();
    airy_mtx_lock(&g_resource_mutex);

    int count = 0;
    airy_resource_record_t *curr = g_resource_head;
    while (curr) {
        count++;
        curr = curr->next;
    }

    if (out_report) {
        size_t buf_size = 4096;
        char *buf = (char *)memory_alloc(buf_size, "resource_report_buffer");
        if (buf) {
            size_t offset = 0;
            int n;

            /*
             * snprintf 返回"将要写入"的字符数（不含 NUL），可能 >= 剩余空间。
             * size_t 是无符号类型：offset >= buf_size 时 buf_size - offset 会下溢为
             * 巨大值，使 snprintf 写入越界。每次写入前必须检查剩余空间，并正确处理
             * 截断情况。
             */
#define AIRY_REPORT_APPEND(fmt, ...)                                            \
    do {                                                                        \
        if (offset >= buf_size)                                                 \
            break;                                                              \
        n = snprintf(buf + offset, buf_size - offset, fmt, ##__VA_ARGS__);      \
        if (n < 0)                                                              \
            break;                                                              \
        if ((size_t)n >= buf_size - offset)                                     \
            offset = buf_size;  /* 已截断，标记缓冲区已满 */                       \
        else                                                                    \
            offset += (size_t)n;                                                 \
    } while (0)

            AIRY_REPORT_APPEND("Resource leak report:\n");
            AIRY_REPORT_APPEND("===================\n");
            AIRY_REPORT_APPEND("Total leaks: %d\n\n", count);

            curr = g_resource_head;
            int i = 0;
            while (curr && i < 100) {
                AIRY_REPORT_APPEND("[%d] Type: %s, Ptr: %p, File: %s:%d, Time: %lu ns\n",
                                   i + 1, curr->type, curr->resource, curr->file, curr->line,
                                   curr->timestamp_ns);
                curr = curr->next;
                i++;
            }

            if (count > 100) {
                AIRY_REPORT_APPEND("... and %d more\n", count - 100);
            }
#undef AIRY_REPORT_APPEND

            *out_report = buf;
        }
    }

    airy_mtx_unlock(&g_resource_mutex);
    return count;
}

void airy_resource_track_clear(void)
{
    ensure_mutex_initialized();
    airy_mtx_lock(&g_resource_mutex);

    airy_resource_record_t *curr = g_resource_head;
    while (curr) {
        airy_resource_record_t *next = curr->next;
        AIRY_FREE(curr);
        curr = next;
    }
    g_resource_head = NULL;

    airy_mtx_unlock(&g_resource_mutex);
}

#endif /* AIRY_RESOURCE_TRACKING */
