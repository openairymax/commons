// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file hall_event.c
 * @brief 任务大厅事件磁盘格式唯一真相源（SSoT）写侧实现。
 *
 * 原三份复刻（daemons/common/src/util/hall_writer.c 已消解、
 * gateway/src/gateway/gateway_hall_store.c 写侧、
 * atoms/coreloopthree/src/work_hall/hall_store.c）的磁盘格式语义收敛到此：
 * gseq 水位线预留与续接、seq 续接、prev_file 决策链、write_roles 策略、
 * 目录创建、原子写与写后回读断言均在此单点定义。磁盘契约见 hall_event.h。
 */

#include "hall_event.h"

#include "airy_dirent.h"
#include "airy_memory.h"
#include "atomic_compat.h"
#include "io.h"
#include "logging.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <sys/stat.h>
#define EVT_STAT_STRUCT struct _stat
#define EVT_STAT(p, st) _stat((p), (st))
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#else
#include <sys/stat.h>
#define EVT_STAT_STRUCT struct stat
#define EVT_STAT(p, st) stat((p), (st))
#endif

#define EVT_TENANT     "default"
#define EVT_SCHEMA     "task-file-v1"
#define EVT_OWNER      "cognition"
#define EVT_ROLES_COG  "[\"cognition\"]"
#define EVT_ROLES_EXEC "[\"cognition\",\"executor\"]"
#define EVT_GSEQ_TAG   "\"gseq\":"
#define EVT_GSEQ_SKIP  7
#define EVT_READBACK   1024

/* gseq 水位线：<root>/.gseq 记录「已预留上界」，锁文件 <root>/.gseq.lck
 * 串行化跨进程预留。点前缀使全部读侧（evt_max_gseq 跳过 '.' 条目、
 * hall_scan_rebuild 与 gw_hall_watch_walk 对普通文件 opendir 失败即跳过）
 * 自动忽略这两个文件，无需各自加白名单。 */
#define EVT_WM_FILE  ".gseq"
#define EVT_WM_LOCK  ".gseq.lck"
#define EVT_WM_BLOCK 1024u
#define EVT_WM_BUF   32

/* 惰性一次性初始化三态：0=未初始化，2=初始化中，1=就绪。 */
static atomic_int g_evt_ready = 0;
static airy_mtx_t g_evt_lock;
/* gseq 仅在 g_evt_lock 内访问：g_evt_gseq=最近已分配，g_evt_limit=已预留上界。 */
static uint64_t g_evt_gseq;
static uint64_t g_evt_limit;
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
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
#if defined(_WIN32)
    gmtime_s(&tmv, &ts.tv_sec);
#else
    gmtime_r(&ts.tv_sec, &tmv);
#endif
    snprintf(buf, sz, "%04d%02d%02dT%02d%02d%02d%03ld", tmv.tm_year + 1900, tmv.tm_mon + 1,
             tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ts.tv_nsec / 1000000);
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

/* 取 p 处前导十进制数（sscanf 禁用）；首字符非数字返回 -1。 */
static int evt_digits(const char *p, uint64_t *out)
{
    if (*p < '0' || *p > '9')
        return -1;
    uint64_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (uint64_t)(*p - '0');
        p++;
    }
    *out = v;
    return 0;
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
    return evt_digits(p, &v) == 0 ? v : 0;
}

/* 递归扫描 hall 根目录，返回全部事件文件中的最大 gseq（无事件返回 0）。 */
static uint64_t evt_max_gseq(const char *dir)
{
    uint64_t max_g = 0;
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        char sub[HALL_EVT_PATH_MAX];
        int n = snprintf(sub, sizeof(sub), "%s/%s", dir, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(sub))
            continue;
        EVT_STAT_STRUCT st;
        if (EVT_STAT(sub, &st) == 0 && S_ISDIR(st.st_mode)) {
            uint64_t g = evt_max_gseq(sub);
            if (g > max_g)
                max_g = g;
            continue;
        }
        hall_evt_parts_t parts;
        if (hall_evt_parse(ent->d_name, &parts) != 0)
            continue;
        FILE *fp = fopen(sub, "r");
        if (!fp)
            continue;
        char hdr[EVT_READBACK] = {0};
        size_t rd = fread(hdr, 1, sizeof(hdr) - 1, fp);
        fclose(fp);
        hdr[rd] = '\0';
        uint64_t g = hall_evt_gseq_of(hdr);
        if (g > max_g)
            max_g = g;
    }
    closedir(d);
    return max_g;
}

/* 拼接 <root>/<leaf>；截断返回 -1。 */
static int evt_root_path(char *out, size_t sz, const char *leaf)
{
    int n = snprintf(out, sz, "%s/%s", g_evt_root, leaf);
    return (n < 0 || (size_t)n >= sz) ? -1 : 0;
}

/* 读取水位线文本的十进制值；文件缺失或内容损坏一律返回 -1，调用方据此
 * 回退全树扫描——损坏的水位线绝不能静默当 0，否则会与磁盘既有事件撞号。 */
static int evt_wm_load(uint64_t *out)
{
    char path[HALL_EVT_PATH_MAX];
    if (evt_root_path(path, sizeof(path), EVT_WM_FILE) != 0)
        return -1;
    char *buf = NULL;
    if (airy_io_read_file(path, &buf, NULL) != 0)
        return -1;
    const char *p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    uint64_t v = 0;
    int rc = evt_digits(p, &v);
    AIRY_FREE(buf);
    if (rc == 0)
        *out = v;
    return rc;
}

/* 落盘新的预留上界（原子写：tmp+fsync+rename）。 */
static void evt_wm_store(uint64_t v)
{
    char path[HALL_EVT_PATH_MAX];
    if (evt_root_path(path, sizeof(path), EVT_WM_FILE) != 0)
        return;
    char buf[EVT_WM_BUF];
    int n = snprintf(buf, sizeof(buf), "%llu\n", (unsigned long long)v);
    if (n > 0 && (size_t)n < sizeof(buf))
        (void)airy_io_write_file(path, buf, (size_t)n);
}

#if defined(_WIN32)
#define EVT_FILENO(fp) _fileno(fp)
#else
#define EVT_FILENO(fp) fileno(fp)
#endif

/* 跨进程串行化预留：持 root 级排他锁读水位线，缺失/损坏时回退一次全树
 * 扫描，再落盘 base+BLOCK 并解锁。取锁失败不写盘、仅按进程内 floor 续接，
 * 保证写路径永不因锁争用而阻塞。 */
static uint64_t evt_reserve(uint64_t floor)
{
    char lpath[HALL_EVT_PATH_MAX];
    if (evt_root_path(lpath, sizeof(lpath), EVT_WM_LOCK) != 0)
        return floor;
    FILE *lf = fopen(lpath, "a");
    if (!lf)
        return floor;
    int locked = (airy_file_lock(EVT_FILENO(lf), 1, 1) == 0);
    uint64_t base = 0;
    if (!locked || evt_wm_load(&base) != 0)
        base = evt_max_gseq(g_evt_root);
    if (base < floor)
        base = floor;
    if (locked) {
        evt_wm_store(base + EVT_WM_BLOCK);
        (void)airy_file_unlock(EVT_FILENO(lf));
    }
    fclose(lf);
    return base;
}

/* 惰性初始化：缓存根目录并确保其存在；gseq 水位线在首次写入时按需预留
 * （见 evt_reserve），初始化阶段不做全树扫描。失败则根目录置空、写入
 * fail-closed。 */
static int evt_ensure(void)
{
    while (atomic_load_explicit(&g_evt_ready, memory_order_acquire) != 1) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&g_evt_ready, &expected, 2,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            airy_mtx_init(&g_evt_lock);
            g_evt_gseq = 0;
            g_evt_limit = 0;
            int n = snprintf(g_evt_root, sizeof(g_evt_root), "%s/%s", airy_data_dir(),
                             HALL_EVT_ROOT_REL);
            if (n <= 0 || (size_t)n >= sizeof(g_evt_root) ||
                airy_io_mkdir_p(g_evt_root, 0755) != 0)
                g_evt_root[0] = '\0';
            atomic_store_explicit(&g_evt_ready, 1, memory_order_release);
            break;
        }
    }
    return atomic_load_explicit(&g_evt_ready, memory_order_acquire) == 1 ? 0 : -1;
}

/* 分配下一个 gseq；仅在 g_evt_lock 内调用。预留区间耗尽时向磁盘续领一块。 */
static uint64_t evt_next_gseq(void)
{
    if (g_evt_gseq >= g_evt_limit) {
        uint64_t base = evt_reserve(g_evt_gseq);
        g_evt_limit = base + EVT_WM_BLOCK;
        g_evt_gseq = base;
    }
    return ++g_evt_gseq;
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

int hall_evt_write(const char *task_id, const char *category, const char *node_id,
                   const char *content_json)
{
    if (!evt_args_ok(task_id, category, content_json))
        return -1;
    if (evt_ensure() != 0 || !g_evt_root[0]) {
        AIRY_LOG_WARN("hall_event: hall root unavailable, event dropped");
        return -1;
    }

    airy_mtx_lock(&g_evt_lock);

    hall_evt_t evt;
    AIRY_MEMSET(&evt, 0, sizeof(evt));
    evt.tenant = EVT_TENANT;
    evt.task = task_id;
    evt.category = category;
    evt.node = node_id;
    evt.owner = EVT_OWNER;
    evt.content = content_json;
    evt.gseq = (unsigned long long)evt_next_gseq();

    char ts[HALL_EVT_TS_LEN];
    hall_clock_utc(ts, sizeof(ts));
    evt.ts_utc = ts;

    char dir[HALL_EVT_PATH_MAX];
    int dn = snprintf(dir, sizeof(dir), "%s/%s/%s/%s", g_evt_root, EVT_TENANT, task_id, category);
    if (dn < 0 || (size_t)dn >= sizeof(dir)) {
        airy_mtx_unlock(&g_evt_lock);
        return -1;
    }
    if (airy_io_mkdir_p(dir, 0755) != 0) {
        airy_mtx_unlock(&g_evt_lock);
        AIRY_LOG_WARN("hall_event: mkdir failed (path=%s)", dir);
        return -1;
    }

    char prev[HALL_EVT_ID_MAX];
    evt.seq = evt_dir_scan(dir, prev, sizeof(prev));
    evt.prev = prev;

    char id[HALL_EVT_ID_MAX];
    if (hall_evt_name(id, sizeof(id), &evt) != 0) {
        airy_mtx_unlock(&g_evt_lock);
        return -1;
    }

    size_t cap = strlen(content_json) + HALL_EVT_HDR_MAX;
    char *json = (char *)AIRY_MALLOC(cap);
    if (!json) {
        airy_mtx_unlock(&g_evt_lock);
        return -1;
    }
    if (hall_evt_build(&evt, json, cap) != 0) {
        AIRY_FREE(json);
        airy_mtx_unlock(&g_evt_lock);
        return -1;
    }

    char path[HALL_EVT_PATH_MAX];
    int pn = snprintf(path, sizeof(path), "%s/%s", dir, id);
    if (pn < 0 || (size_t)pn >= sizeof(path)) {
        AIRY_FREE(json);
        airy_mtx_unlock(&g_evt_lock);
        return -1;
    }
    int rc = airy_io_write_file(path, json, strlen(json));
    AIRY_FREE(json);
    airy_mtx_unlock(&g_evt_lock);
    if (rc != 0) {
        AIRY_LOG_WARN("hall_event: write failed (path=%s)", path);
        return -1;
    }

#ifndef NDEBUG
    /* 单一真相源事件流断言：写后必可读，file.id 位于 header 开头。 */
    if (evt_readback(path, id) != 0)
        return -1;
#endif

    return 0;
}
