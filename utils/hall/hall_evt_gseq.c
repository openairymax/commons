// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file hall_evt_gseq.c
 * @brief hall 事件全局序号（gseq）编号机制实现。
 *
 * 水位线文件 <root>/.gseq 记录「已预留上界」，锁文件 <root>/.gseq.lck
 * 串行化跨进程预留。点前缀使全部读侧（evt_max_gseq 跳过 '.' 条目、
 * hall_scan_rebuild 与 gw_hall_watch_walk 对普通文件 opendir 失败即跳过）
 * 自动忽略这两个文件，无需各自加白名单。
 */

#include "hall_evt_gseq.h"

#include "airy_dirent.h"
#include "airy_memory.h"
#include "hall_event.h"
#include "io.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

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

#if defined(_WIN32)
#define EVT_FILENO(fp) _fileno(fp)
#else
#define EVT_FILENO(fp) fileno(fp)
#endif

#define EVT_WM_FILE  ".gseq"
#define EVT_WM_LOCK  ".gseq.lck"
#define EVT_WM_BLOCK 1024u
#define EVT_WM_BUF   32
#define EVT_READBACK 1024

/* 默认根的区间缓存：g_cur=最近已分配，g_limit=已预留上界。调用方持
 * hall_event 写锁访问，故此处不加锁。 */
static uint64_t g_cur;
static uint64_t g_limit;
static char g_root[HALL_EVT_PATH_MAX];

int hall_atou(const char *p, uint64_t *out)
{
    if (!p || !out || *p < '0' || *p > '9')
        return -1;
    uint64_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (uint64_t)(*p - '0');
        p++;
    }
    *out = v;
    return 0;
}

void hall_gseq_init(const char *root)
{
    g_cur = 0;
    g_limit = 0;
    g_root[0] = '\0';
    if (!root || !root[0])
        return;
    int n = snprintf(g_root, sizeof(g_root), "%s", root);
    if (n <= 0 || (size_t)n >= sizeof(g_root))
        g_root[0] = '\0';
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
static int evt_join(char *out, size_t sz, const char *root, const char *leaf)
{
    int n = snprintf(out, sz, "%s/%s", root, leaf);
    return (n < 0 || (size_t)n >= sz) ? -1 : 0;
}

/* 读取水位线文本的十进制值；文件缺失或内容损坏一律返回 -1，调用方据此
 * 回退全树扫描——损坏的水位线绝不能静默当 0，否则会与磁盘既有事件撞号。 */
static int evt_wm_load(const char *root, uint64_t *out)
{
    char path[HALL_EVT_PATH_MAX];
    if (evt_join(path, sizeof(path), root, EVT_WM_FILE) != 0)
        return -1;
    char *buf = NULL;
    if (airy_io_read_file(path, &buf, NULL) != 0)
        return -1;
    const char *p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    uint64_t v = 0;
    int rc = hall_atou(p, &v);
    AIRY_FREE(buf);
    if (rc == 0)
        *out = v;
    return rc;
}

/* 落盘新的预留上界（原子写：tmp+fsync+rename）。 */
static void evt_wm_store(const char *root, uint64_t v)
{
    char path[HALL_EVT_PATH_MAX];
    if (evt_join(path, sizeof(path), root, EVT_WM_FILE) != 0)
        return;
    char buf[EVT_WM_BUF];
    int n = snprintf(buf, sizeof(buf), "%llu\n", (unsigned long long)v);
    if (n > 0 && (size_t)n < sizeof(buf))
        (void)airy_io_write_file(path, buf, (size_t)n);
}

/* 跨进程串行化预留：持 root 级排他锁读水位线，缺失/损坏时回退一次全树
 * 扫描，再落盘 base+BLOCK 并解锁。取锁失败不写盘、仅按进程内 floor 续接，
 * 保证写路径永不因锁争用而阻塞。 */
static uint64_t evt_reserve(const char *root, uint64_t floor)
{
    char lpath[HALL_EVT_PATH_MAX];
    if (evt_join(lpath, sizeof(lpath), root, EVT_WM_LOCK) != 0)
        return floor;
    FILE *lf = fopen(lpath, "a");
    if (!lf)
        return floor;
    int locked = (airy_file_lock(EVT_FILENO(lf), 1, 1) == 0);
    uint64_t base = 0;
    if (!locked || evt_wm_load(root, &base) != 0)
        base = evt_max_gseq(root);
    if (base < floor)
        base = floor;
    if (locked) {
        evt_wm_store(root, base + EVT_WM_BLOCK);
        (void)airy_file_unlock(EVT_FILENO(lf));
    }
    fclose(lf);
    return base;
}

uint64_t hall_gseq_next(const char *root)
{
    if (!root || !root[0])
        return 0;
    if (g_root[0] && strcmp(root, g_root) == 0) {
        if (g_cur >= g_limit) {
            uint64_t base = evt_reserve(root, g_cur);
            g_limit = base + EVT_WM_BLOCK;
            g_cur = base;
        }
        return ++g_cur;
    }
    /* 非默认根不缓存：每次按该根自己的锁文件与水位线预留，保证同一根上的
     * 多个写者（含跨进程）永不撞号。 */
    return evt_reserve(root, 0) + 1;
}
