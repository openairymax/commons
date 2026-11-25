// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file hall_event.c
 * @brief 任务大厅事件磁盘格式唯一真相源（SSoT）写侧实现。
 *
 * 原三份复刻（daemons/common/src/util/hall_writer.c 已消解、
 * gateway/src/gateway/hall/gateway_hall_store.c 写侧、
 * atoms/coreloopthree/src/work_hall/hall_store.c）的磁盘格式语义收敛到此：
 * seq 续接、prev_file 决策链、write_roles 策略、目录创建、原子写与写后回读
 * 断言在此单点定义；gseq 编号机制见 hall_evt_gseq.c。磁盘契约见 hall_event.h。
 */

#include "hall_event.h"

#include "airy_dirent.h"
#include "airy_memory.h"
#include "atomic_compat.h"
#include "hall_evt_gseq.h"
#include "io.h"
#include "logging.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define EVT_TENANT     "default"
#define EVT_SCHEMA     "task-file-v1"
#define EVT_OWNER      "cognition"
#define EVT_ROLES_COG  "[\"cognition\"]"
#define EVT_ROLES_EXEC "[\"cognition\",\"executor\"]"
#define EVT_GSEQ_TAG   "\"gseq\":"
#define EVT_GSEQ_SKIP  7
#define EVT_READBACK   1024

/* 惰性一次性初始化三态：0=未初始化，2=初始化中，1=就绪。 */
static atomic_int g_evt_ready = 0;
static airy_mtx_t g_evt_lock;
static char g_evt_root[HALL_EVT_PATH_MAX];

int hall_comp_valid(const char *s)
{
    if (!s || !s[0])
        return 0;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '.') {
            if (p[1] == '\0')
                continue;
            if (p == s || p[1] == '.')
                return 0;
            continue;
        }
        if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == ' ')
            return 0;
    }
    return 1;
}

void hall_clock_utc(char *buf, size_t sz)
{
    if (!buf || sz == 0)
        return;
    uint64_t wall_ms = airy_time_wall_ms();
    time_t secs = (time_t)(wall_ms / 1000);
    struct tm tmv;
#if defined(_WIN32)
    gmtime_s(&tmv, &secs);
#else
    gmtime_r(&secs, &tmv);
#endif
    snprintf(buf, sz, "%04d%02d%02dT%02d%02d%02d%03ld", tmv.tm_year + 1900, tmv.tm_mon + 1,
             tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (long)(wall_ms % 1000));
}

const char *hall_roles_of(const char *category)
{
    if (category && (strcmp(category, "blueprint") == 0 || strcmp(category, "command") == 0 ||
                     strcmp(category, "chain") == 0))
        return EVT_ROLES_COG;
    return EVT_ROLES_EXEC;
}

int hall_evt_name(char *out, size_t sz, const hall_evt_t *evt)
{
    if (!out || sz == 0 || !evt || !evt->tenant || !evt->task || !evt->category || !evt->ts_utc)
        return -1;
    int n = snprintf(out, sz, "%s.%s.%s.%s.%04u.json", evt->tenant, evt->task, evt->category,
                     evt->ts_utc, evt->seq);
    return (n < 0 || (size_t)n >= sz) ? -1 : 0;
}

int hall_evt_parse(const char *name, hall_evt_parts_t *out)
{
    if (!name || !out)
        return -1;
    const char *dot[5];
    int n = 0;
    for (const char *p = name; *p && n < 5; p++) {
        if (*p == '.')
            dot[n++] = p;
    }
    if (n != 5 || strcmp(dot[4], ".json") != 0)
        return -1;
    out->tenant = name;
    out->tenant_len = (size_t)(dot[0] - name);
    out->task = dot[0] + 1;
    out->task_len = (size_t)(dot[1] - dot[0] - 1);
    out->category = dot[1] + 1;
    out->category_len = (size_t)(dot[2] - dot[1] - 1);
    out->ts_utc = dot[2] + 1;
    out->ts_utc_len = (size_t)(dot[3] - dot[2] - 1);
    out->seq = dot[3] + 1;
    out->seq_len = (size_t)(dot[4] - dot[3] - 1);
    if (!out->tenant_len || !out->task_len || !out->category_len || !out->ts_utc_len ||
        !out->seq_len)
        return -1;
    return 0;
}

unsigned long hall_evt_seq(const char *name)
{
    hall_evt_parts_t parts;
    if (hall_evt_parse(name, &parts) != 0)
        return 0;
    unsigned long v = 0;
    for (size_t i = 0; i < parts.seq_len; i++) {
        char c = parts.seq[i];
        if (c < '0' || c > '9')
            return 0;
        v = v * 10 + (unsigned long)(c - '0');
    }
    return v;
}

int hall_evt_build(const hall_evt_t *evt, char *out, size_t out_sz)
{
    if (!out || out_sz == 0 || !evt || !evt->content)
        return -1;
    char id[HALL_EVT_ID_MAX];
    if (hall_evt_name(id, sizeof(id), evt) != 0)
        return -1;
    const char *node = evt->node ? evt->node : "";
    const char *owner = (evt->owner && evt->owner[0]) ? evt->owner : EVT_OWNER;
    int n = snprintf(out, out_sz,
                     "{\"file\":{\"id\":\"%s\",\"category\":\"%s\",\"schema\":\"%s\","
                     "\"tenant_id\":\"%s\",\"task_id\":\"%s\",\"node_id\":\"%s\","
                     "\"ts_utc\":\"%s\",\"seq\":%u,\"gseq\":%llu,\"prev_file\":\"%s\"},"
                     "\"access\":{\"owner_role\":\"%s\",\"write_roles\":%s,"
                     "\"read_roles\":[\"cognition\"]},\"content\":%s}",
                     id, evt->category, EVT_SCHEMA, evt->tenant, evt->task, node, evt->ts_utc,
                     evt->seq, evt->gseq, evt->prev ? evt->prev : "", owner,
                     hall_roles_of(evt->category), evt->content);
    return (n < 0 || (size_t)n >= out_sz) ? -1 : 0;
}

/* 手工解析 header 中 "gseq":N（读侧唯一实现，见 hall_event.h）。 */
uint64_t hall_evt_gseq_of(const char *json)
{
    if (!json)
        return 0;
    const char *p = strstr(json, EVT_GSEQ_TAG);
    if (!p)
        return 0;
    p += EVT_GSEQ_SKIP;
    while (*p == ' ' || *p == '\t')
        p++;
    uint64_t v = 0;
    return hall_atou(p, &v) == 0 ? v : 0;
}

/* 惰性初始化：缓存默认根目录；gseq 水位线在首次写入时按需预留（见
 * hall_gseq_next），初始化阶段不做全树扫描，也不创建目录（默认根与
 * 自定义根都由提交路径按需 mkdir）。失败则根目录置空、写入 fail-closed。 */
static int evt_ensure(void)
{
    while (atomic_load_explicit(&g_evt_ready, memory_order_acquire) != 1) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&g_evt_ready, &expected, 2,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            airy_mtx_init(&g_evt_lock);
            int n = snprintf(g_evt_root, sizeof(g_evt_root), "%s/%s", airy_data_dir(),
                             HALL_EVT_ROOT_REL);
            if (n <= 0 || (size_t)n >= sizeof(g_evt_root))
                g_evt_root[0] = '\0';
            hall_gseq_init(g_evt_root);
            atomic_store_explicit(&g_evt_ready, 1, memory_order_release);
            break;
        }
    }
    return atomic_load_explicit(&g_evt_ready, memory_order_acquire) == 1 ? 0 : -1;
}

/* 扫描单个 (task, category) 目录，返回下一个可用 seq（磁盘 max(seq)+1），
 * 并回填当前 max(seq) 事件文件名作为决策链前驱。 */
static unsigned evt_dir_scan(const char *dir, char *prev, size_t prev_sz)
{
    unsigned max_seq = 0;
    if (prev && prev_sz > 0)
        prev[0] = '\0';
    DIR *d = opendir(dir);
    if (!d)
        return 1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        unsigned long seq = hall_evt_seq(ent->d_name);
        if (seq == 0)
            continue;
        if (seq > max_seq) {
            max_seq = (unsigned)seq;
            if (prev && prev_sz > 0)
                AIRY_STRNCPY_TERM(prev, ent->d_name, prev_sz);
        }
    }
    closedir(d);
    return max_seq + 1;
}

static int evt_args_ok(const char *task_id, const char *category, const char *content_json)
{
    if (!task_id || !task_id[0] || !category || !category[0] || !content_json || !content_json[0])
        return 0;
    if (!hall_comp_valid(task_id) || !hall_comp_valid(category)) {
        AIRY_LOG_WARN("hall_event: invalid task_id/category (path traversal blocked)");
        return 0;
    }
    return 1;
}

#ifndef NDEBUG
static int evt_readback(const char *path, const char *id)
{
    FILE *rf = fopen(path, "r");
    if (!rf) {
        AIRY_LOG_ERROR("hall_event: invariant violated - event file missing after write (%s)", path);
        return -1;
    }
    char buf[EVT_READBACK];
    size_t got = fread(buf, 1, sizeof(buf) - 1, rf);
    buf[got] = '\0';
    fclose(rf);
    if (strstr(buf, id) == NULL) {
        AIRY_LOG_ERROR("hall_event: invariant violated - write-then-read mismatch for %s", id);
        return -1;
    }
    return 0;
}
#endif

/* 根目录解析：显式根优先，否则用全局根；两者皆空即丢弃事件。 */
static const char *evt_root_of(const hall_evt_req_t *req)
{
    if (evt_ensure() != 0)
        return NULL;
    const char *root = (req->root && req->root[0]) ? req->root : g_evt_root;
    if (!root[0]) {
        AIRY_LOG_WARN("hall_event: hall root unavailable, event dropped");
        return NULL;
    }
    return root;
}

/* 组装事件头并创建目标目录：seq/prev/gseq 都在目录就绪后才确定。 */
static int evt_prepare(const hall_evt_req_t *req, const char *root, hall_evt_t *evt, char *ts,
                       size_t ts_sz, char *dir, size_t dir_sz, char *prev, size_t prev_sz)
{
    AIRY_MEMSET(evt, 0, sizeof(*evt));
    evt->tenant = (req->tenant && req->tenant[0]) ? req->tenant : EVT_TENANT;
    evt->task = req->task;
    evt->category = req->category;
    evt->node = req->node;
    evt->owner = (req->owner && req->owner[0]) ? req->owner : EVT_OWNER;
    evt->content = req->content;

    int dn = snprintf(dir, dir_sz, "%s/%s/%s/%s", root, evt->tenant, req->task, req->category);
    if (dn < 0 || (size_t)dn >= dir_sz)
        return -1;
    if (airy_io_mkdir_p(dir, 0755) != 0) {
        AIRY_LOG_WARN("hall_event: mkdir failed (path=%s)", dir);
        return -1;
    }

    evt->seq = evt_dir_scan(dir, prev, prev_sz);
    evt->prev = prev;
    hall_clock_utc(ts, ts_sz);
    evt->ts_utc = ts;
    evt->gseq = (unsigned long long)hall_gseq_next(root);
    return 0;
}

/* 序列化落盘；成功后回填 out_id，并在调试构建下做写后可读断言。 */
static int evt_persist(const hall_evt_req_t *req, const hall_evt_t *evt, const char *dir,
                       const char *id)
{
    size_t cap = strlen(req->content) + HALL_EVT_HDR_MAX;
    char *json = (char *)AIRY_MALLOC(cap);
    if (!json)
        return -1;
    if (hall_evt_build(evt, json, cap) != 0) {
        AIRY_FREE(json);
        return -1;
    }

    char path[HALL_EVT_PATH_MAX];
    int pn = snprintf(path, sizeof(path), "%s/%s", dir, id);
    int rc = (pn < 0 || (size_t)pn >= sizeof(path))
                 ? -1
                 : airy_io_write_file(path, json, strlen(json));
    AIRY_FREE(json);
    if (rc != 0) {
        AIRY_LOG_WARN("hall_event: write failed (path=%s)", path);
        return -1;
    }
    if (req->out_id && req->out_id_sz > 0)
        AIRY_STRNCPY_TERM(req->out_id, id, req->out_id_sz);

#ifndef NDEBUG
    /* 单一真相源事件流断言：写后必可读，file.id 位于 header 开头。 */
    return evt_readback(path, id);
#else
    return 0;
#endif
}

int hall_evt_commit(const hall_evt_req_t *req)
{
    if (!req || !evt_args_ok(req->task, req->category, req->content))
        return -1;
    const char *tenant = (req->tenant && req->tenant[0]) ? req->tenant : EVT_TENANT;
    if (!hall_comp_valid(tenant)) {
        AIRY_LOG_WARN("hall_event: invalid tenant (path traversal blocked)");
        return -1;
    }
    const char *root = evt_root_of(req);
    if (!root)
        return -1;

    airy_mtx_lock(&g_evt_lock);

    hall_evt_t evt;
    char ts[HALL_EVT_TS_LEN];
    char dir[HALL_EVT_PATH_MAX];
    char prev[HALL_EVT_ID_MAX];
    char id[HALL_EVT_ID_MAX];
    int rc = evt_prepare(req, root, &evt, ts, sizeof(ts), dir, sizeof(dir), prev, sizeof(prev));
    if (rc == 0 && hall_evt_name(id, sizeof(id), &evt) != 0)
        rc = -1;
    if (rc == 0)
        rc = evt_persist(req, &evt, dir, id);

    airy_mtx_unlock(&g_evt_lock);
    return rc;
}

int hall_evt_write(const char *task_id, const char *category, const char *node_id,
                   const char *content_json)
{
    hall_evt_req_t req;
    AIRY_MEMSET(&req, 0, sizeof(req));
    req.task = task_id;
    req.category = category;
    req.node = node_id;
    req.content = content_json;
    return hall_evt_commit(&req);
}
