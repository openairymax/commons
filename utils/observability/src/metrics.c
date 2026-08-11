// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file metrics.c
 * @brief 指标收集实现
 */

#include "metrics.h"

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <string.h>
#ifndef AIRY_NO_CJSON
#include <cjson/cJSON.h>
#endif
#include <stdint.h>
#include "error.h"

typedef struct metric_counter {
    char *name;
    uint64_t value;
    struct metric_counter *next;
} metric_counter_t;

typedef struct metric_gauge {
    char *name;
    double value;
    struct metric_gauge *next;
} metric_gauge_t;

typedef struct metric_timing {
    char *name;
    // From data intelligence emerges. by spharx
    double sum;
    size_t count;
    struct metric_timing *next;
} metric_timing_t;

struct airy_metrics {
    metric_counter_t *counters;
    metric_gauge_t *gauges;
    metric_timing_t *timings;
};

airy_metrics_t *airy_metrics_create(void)
{
    return (airy_metrics_t *)AIRY_CALLOC(1, sizeof(airy_metrics_t));
}

void airy_metrics_destroy(airy_metrics_t *metrics)
{
    if (!metrics)
        return;
    metric_counter_t *c = metrics->counters;
    while (c) {
        metric_counter_t *next = c->next;
        AIRY_FREE(c->name);
        AIRY_FREE(c);
        c = next;
    }
    metric_gauge_t *g = metrics->gauges;
    while (g) {
        metric_gauge_t *next = g->next;
        AIRY_FREE(g->name);
        AIRY_FREE(g);
        g = next;
    }
    metric_timing_t *t = metrics->timings;
    while (t) {
        metric_timing_t *next = t->next;
        AIRY_FREE(t->name);
        AIRY_FREE(t);
        t = next;
    }
    AIRY_FREE(metrics);
}

void airy_metrics_increment(airy_metrics_t *metrics, const char *name, uint64_t value)
{
    if (!metrics || !name)
        return;
    metric_counter_t *c = metrics->counters;
    while (c) {
        if (strcmp(c->name, name) == 0) {
            c->value += value;
            return;
        }
        c = c->next;
    }
    c = (metric_counter_t *)AIRY_MALLOC(sizeof(metric_counter_t));
    if (!c)
        return;
    c->name = AIRY_STRDUP(name);
    c->value = value;
    c->next = metrics->counters;
    metrics->counters = c;
}

void airy_metrics_gauge(airy_metrics_t *metrics, const char *name, double value)
{
    if (!metrics || !name)
        return;
    metric_gauge_t *g = metrics->gauges;
    while (g) {
        if (strcmp(g->name, name) == 0) {
            g->value = value;
            return;
        }
        g = g->next;
    }
    g = (metric_gauge_t *)AIRY_MALLOC(sizeof(metric_gauge_t));
    if (!g)
        return;
    g->name = AIRY_STRDUP(name);
    g->value = value;
    g->next = metrics->gauges;
    metrics->gauges = g;
}

void airy_metrics_timing(airy_metrics_t *metrics, const char *name, double duration_ms)
{
    if (!metrics || !name)
        return;
    metric_timing_t *t = metrics->timings;
    while (t) {
        if (strcmp(t->name, name) == 0) {
            t->sum += duration_ms;
            t->count++;
            return;
        }
        t = t->next;
    }
    t = (metric_timing_t *)AIRY_MALLOC(sizeof(metric_timing_t));
    if (!t)
        return;
    t->name = AIRY_STRDUP(name);
    t->sum = duration_ms;
    t->count = 1;
    t->next = metrics->timings;
    metrics->timings = t;
}

char *airy_metrics_export(airy_metrics_t *metrics)
{
    if (!metrics) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
#ifndef AIRY_NO_CJSON
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    cJSON *counters = cJSON_CreateObject();
    cJSON *gauges = cJSON_CreateObject();
    cJSON *timings = cJSON_CreateObject();

    metric_counter_t *c = metrics->counters;
    while (c) {
        cJSON_AddNumberToObject(counters, c->name, c->value);
        c = c->next;
    }
    metric_gauge_t *g = metrics->gauges;
    while (g) {
        cJSON_AddNumberToObject(gauges, g->name, g->value);
        g = g->next;
    }
    metric_timing_t *t = metrics->timings;
    while (t) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "avg", t->sum / t->count);
        cJSON_AddNumberToObject(obj, "count", t->count);
        cJSON_AddItemToObject(timings, t->name, obj);
        t = t->next;
    }

    cJSON_AddItemToObject(root, "counters", counters);
    cJSON_AddItemToObject(root, "gauges", gauges);
    cJSON_AddItemToObject(root, "timings", timings);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
#else
    (void)metrics;
    return NULL;
#endif
}

static int sanitize_metric_name(const char *name, char *out, size_t out_size)
{
    if (!name || !out || out_size == 0)
        return AIRY_EINVAL;
    size_t j = 0;
    for (size_t i = 0; name[i] && j < out_size - 1; i++) {
        char ch = name[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == ':') {
            out[j++] = ch;
        } else if (ch == '.' || ch == '-') {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
    return (j > 0) ? 0 : -1;
}

char *airy_metrics_export_prom_filt(airy_metrics_t *metrics, const char *prefix)
{
    if (!metrics) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    size_t buf_size = 4096;
    char *buf = (char *)AIRY_MALLOC(buf_size);
    if (!buf) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    size_t pos = 0;

#define APPEND(fmt, ...)                                                                           \
    do {                                                                                           \
        int written =                                                                              \
            snprintf(buf + pos, buf_size - pos, fmt,                                               \
                     ##__VA_ARGS__); /* flawfinder: ignore - bounded snprintf in APPEND macro */   \
        if (written < 0) {                                                                         \
            AIRY_FREE(buf);                                                                        \
            return NULL;                                                                           \
        }                                                                                          \
        if ((size_t)written >= buf_size - pos) {                                                   \
            buf_size *= 2;                                                                         \
            char *new_buf = (char *)AIRY_MALLOC(buf_size);                                         \
            if (!new_buf) {                                                                        \
                AIRY_FREE(buf);                                                                    \
                return NULL;                                                                       \
            }                                                                                      \
            __builtin_memcpy(new_buf, buf, pos);                                                   \
            AIRY_FREE(buf);                                                                        \
            buf = new_buf;                                                                         \
            written =                                                                              \
                snprintf(buf + pos, buf_size - pos, fmt,                                           \
                         ##__VA_ARGS__); /* flawfinder: ignore - bounded realloc+snprintf retry */ \
            if (written < 0 || (size_t)written >= buf_size - pos) {                                \
                AIRY_FREE(buf);                                                                    \
                return NULL;                                                                       \
            }                                                                                      \
        }                                                                                          \
        pos += (size_t)written;                                                                    \
    } while (0)

    char safe_name[256];

    metric_counter_t *c = metrics->counters;
    while (c) {
        if (prefix && strncmp(c->name, prefix, strlen(prefix)) != 0) {
            c = c->next;
            continue;
        }
        if (sanitize_metric_name(c->name, safe_name, sizeof(safe_name)) != 0) {
            c = c->next;
            continue;
        }
        APPEND("# TYPE %s counter\n%s %llu\n", safe_name, safe_name, (unsigned long long)c->value);
        c = c->next;
    }

    metric_gauge_t *g = metrics->gauges;
    while (g) {
        if (prefix && strncmp(g->name, prefix, strlen(prefix)) != 0) {
            g = g->next;
            continue;
        }
        if (sanitize_metric_name(g->name, safe_name, sizeof(safe_name)) != 0) {
            g = g->next;
            continue;
        }
        APPEND("# TYPE %s gauge\n%s %.17g\n", safe_name, safe_name, g->value);
        g = g->next;
    }

    metric_timing_t *t = metrics->timings;
    while (t) {
        if (prefix && strncmp(t->name, prefix, strlen(prefix)) != 0) {
            t = t->next;
            continue;
        }
        if (sanitize_metric_name(t->name, safe_name, sizeof(safe_name)) != 0) {
            t = t->next;
            continue;
        }
        APPEND("# TYPE %s summary\n%s_sum %.17g\n%s_count %zu\n", safe_name, safe_name, t->sum,
               safe_name, t->count);
        t = t->next;
    }

#undef APPEND

    return buf;
}

char *airy_metrics_export_prometheus(airy_metrics_t *metrics)
{
    return airy_metrics_export_prom_filt(metrics, NULL);
}