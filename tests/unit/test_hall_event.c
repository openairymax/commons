// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_hall_event.c
 * @brief 任务大厅事件磁盘格式 SSoT（hall_event）单元测试。
 *
 * 覆盖 daemons/gateway/atoms 三份复刻收敛后的写侧契约：路径分量校验、
 * UTC 时间戳、write_roles 策略、事件命名、envelope 逐字节布局，以及完整
 * 写路径（gseq 续接、seq 续接、prev_file 决策链、跨 category 链隔离、
 * 非法参数 fail-closed）。数据目录经 AIRY_HOME 隔离到临时目录。
 */

#include "hall_event.h"
#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#define HE_GETPID() ((long)_getpid())
#define HE_SETENV(k, v) _putenv_s((k), (v))
#else
#include <unistd.h>
#define HE_GETPID() ((long)getpid())
#define HE_SETENV(k, v) setenv((k), (v), 1)
#endif

#define TEST_ASSERT(condition, message)                            \
    do {                                                           \
        if (!(condition)) {                                        \
            fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
            return 1;                                              \
        }                                                          \
    } while (0)

#define TEST_RUN(test_func)                                      \
    do {                                                         \
        printf("  [TEST] %s ... ", #test_func);                  \
        if (test_func() != 0) {                                  \
            fprintf(stderr, "test failed: %s\n", #test_func);    \
            failed_tests++;                                      \
        } else {                                                 \
            printf("PASS\n");                                    \
            passed_tests++;                                      \
        }                                                        \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

static char g_home[256];
static char g_root[512];

static const char *he_tmp_base(void)
{
#if defined(_WIN32)
    const char *t = getenv("TEMP");
    return (t && t[0]) ? t : ".";
#else
    const char *t = getenv("TMPDIR");
    return (t && t[0]) ? t : "/tmp";
#endif
}

static void isolate_data_dir(void)
{
    snprintf(g_home, sizeof(g_home), "%s/airymaxrt-he-%ld", he_tmp_base(), HE_GETPID());
    HE_SETENV("AIRY_HOME", g_home);
    HE_SETENV("AIRY_DATA_DIR", "");
    snprintf(g_root, sizeof(g_root), "%s/data/agentrt/hall", g_home);
}

static int he_read(const char *path, char *out, size_t sz)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    size_t got = fread(out, 1, sz - 1, f);
    out[got] = '\0';
    fclose(f);
    return (int)got;
}

/* 事件文件名倒数第二段（"...ts.seq.json" 的 seq）。 */
static unsigned he_name_seq(const char *name)
{
    const char *last = strrchr(name, '.');
    if (!last || last == name)
        return 0;
    const char *sep = last - 1;
    while (sep > name && *sep != '.')
        sep--;
    if (*sep != '.')
        return 0;
    unsigned v = 0;
    for (const char *q = sep + 1; q < last; q++) {
        if (*q < '0' || *q > '9')
            return 0;
        v = v * 10 + (unsigned)(*q - '0');
    }
    return v;
}

/* 目录内 seq 最大的事件文件名（与 SSoT evt_dir_scan 同规则）。 */
static int he_max_seq_file(const char *dir, char *out, size_t sz)
{
    out[0] = '\0';
    char **files = NULL;
    size_t n = 0;
    if (airy_io_list_files(dir, &files, &n) != 0)
        return -1;
    unsigned max_seq = 0;
    for (size_t i = 0; i < n; i++) {
        size_t len = strlen(files[i]);
        if (len < 5 || strcmp(files[i] + len - 5, ".json") != 0)
            continue;
        unsigned s = he_name_seq(files[i]);
        if (s > max_seq) {
            max_seq = s;
            snprintf(out, sz, "%s", files[i]);
        }
    }
    airy_io_free_list(files, n);
    return out[0] ? 0 : -1;
}

static int he_json_str(const char *json, const char *key, char *out, size_t sz)
{
    out[0] = '\0';
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < sz)
        out[i++] = *p++;
    out[i] = '\0';
    return 0;
}

/* 首个用例：必须在任何 hall_evt_write 之前落下磁盘事件，验证 gseq 续接。 */
static int test_gseq_resumption(void)
{
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/default/he-resume/chain", g_root);
    TEST_ASSERT(airy_io_mkdir_p(dir, 0755) == 0, "pre-create event dir");

    char pre_path[768];
    snprintf(pre_path, sizeof(pre_path), "%s/default.he-resume.chain.20260829T000000000.0001.json",
             dir);
    const char pre[] = "{\"file\":{\"id\":\"default.he-resume.chain.20260829T000000000.0001.json\","
                       "\"gseq\":5},\"content\":{}}";
    TEST_ASSERT(airy_io_write_file(pre_path, pre, strlen(pre)) == 0, "pre-write event");

    TEST_ASSERT(hall_evt_write("he-resume", "chain", NULL, "{\"e\":1}") == 0, "write resumes gseq");

    char fname[256];
    TEST_ASSERT(he_max_seq_file(dir, fname, sizeof(fname)) == 0, "find latest event");
    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dir, fname);
    char buf[2048];
    TEST_ASSERT(he_read(path, buf, sizeof(buf)) > 0, "read latest event");
    TEST_ASSERT(strstr(buf, "\"gseq\":6") != NULL, "gseq must resume to disk max + 1");
    TEST_ASSERT(strstr(fname, ".0002.json") != NULL, "seq must resume to disk max + 1");
    return 0;
}

static int test_comp_valid(void)
{
    TEST_ASSERT(hall_comp_valid("task-1") == 1, "dash name valid");
    TEST_ASSERT(hall_comp_valid("task.1") == 1, "internal dot valid");
    TEST_ASSERT(hall_comp_valid("task.") == 1, "trailing dot valid");
    TEST_ASSERT(hall_comp_valid("\xE4\xBD\xA0\xE5\xA5\xBD") == 1, "UTF-8 name valid");
    TEST_ASSERT(hall_comp_valid("..") == 0, "double dot rejected");
    TEST_ASSERT(hall_comp_valid(".hidden") == 0, "leading dot rejected");
    TEST_ASSERT(hall_comp_valid("a..b") == 0, "embedded double dot rejected");
    TEST_ASSERT(hall_comp_valid("a/b") == 0, "slash rejected");
    TEST_ASSERT(hall_comp_valid("a\\b") == 0, "backslash rejected");
    TEST_ASSERT(hall_comp_valid("a:b") == 0, "colon rejected");
    TEST_ASSERT(hall_comp_valid("a b") == 0, "space rejected");
    TEST_ASSERT(hall_comp_valid("a\tb") == 0, "control char rejected");
    TEST_ASSERT(hall_comp_valid("") == 0, "empty rejected");
    TEST_ASSERT(hall_comp_valid(NULL) == 0, "NULL rejected");
    return 0;
}

static int test_clock_utc(void)
{
    char ts[HALL_EVT_TS_LEN];
    memset(ts, 'x', sizeof(ts));
    hall_clock_utc(ts, sizeof(ts));
    TEST_ASSERT(strlen(ts) == 18, "UTC stamp is 18 chars wide");
    TEST_ASSERT(ts[8] == 'T', "'T' separates date and time");
    for (int i = 0; i < 18; i++) {
        if (i == 8)
            continue;
        TEST_ASSERT(ts[i] >= '0' && ts[i] <= '9', "UTC stamp is all digits outside 'T'");
    }

    hall_clock_utc(NULL, 0);
    char small[4];
    memset(small, 'x', sizeof(small));
    hall_clock_utc(small, sizeof(small));
    TEST_ASSERT(small[3] == '\0', "small buffer still NUL-terminated");
    return 0;
}

static int test_roles_of(void)
{
    TEST_ASSERT(strcmp(hall_roles_of("blueprint"), "[\"cognition\"]") == 0, "blueprint cognition-only");
    TEST_ASSERT(strcmp(hall_roles_of("command"), "[\"cognition\"]") == 0, "command cognition-only");
    TEST_ASSERT(strcmp(hall_roles_of("chain"), "[\"cognition\"]") == 0, "chain cognition-only");
    TEST_ASSERT(strcmp(hall_roles_of("progress"), "[\"cognition\",\"executor\"]") == 0,
                "progress executor-writable");
    TEST_ASSERT(strcmp(hall_roles_of("result"), "[\"cognition\",\"executor\"]") == 0,
                "result executor-writable");
    TEST_ASSERT(strcmp(hall_roles_of("issue"), "[\"cognition\",\"executor\"]") == 0,
                "issue executor-writable");
    TEST_ASSERT(strcmp(hall_roles_of("verify"), "[\"cognition\",\"executor\"]") == 0,
                "verify executor-writable");
    TEST_ASSERT(strcmp(hall_roles_of(NULL), "[\"cognition\",\"executor\"]") == 0,
                "NULL category defaults to executor-writable");
    return 0;
}

static int test_evt_name(void)
{
    hall_evt_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.tenant = "default";
    evt.task = "t1";
    evt.category = "progress";
    evt.ts_utc = "20260829T000000000";
    evt.seq = 3;

    char out[HALL_EVT_ID_MAX];
    TEST_ASSERT(hall_evt_name(out, sizeof(out), &evt) == 0, "name build ok");
    TEST_ASSERT(strcmp(out, "default.t1.progress.20260829T000000000.0003.json") == 0,
                "name format {tenant}.{task}.{category}.{ts}.{seq:04u}.json");

    char small[16];
    TEST_ASSERT(hall_evt_name(small, sizeof(small), &evt) == -1, "truncation fail-closed");
    TEST_ASSERT(hall_evt_name(NULL, sizeof(out), &evt) == -1, "NULL out rejected");
    TEST_ASSERT(hall_evt_name(out, 0, &evt) == -1, "zero size rejected");
    TEST_ASSERT(hall_evt_name(out, sizeof(out), NULL) == -1, "NULL evt rejected");

    evt.seq = 12345;
    TEST_ASSERT(hall_evt_name(out, sizeof(out), &evt) == 0, "5-digit seq ok");
    TEST_ASSERT(strstr(out, ".12345.json") != NULL, "seq widens past 4 digits");
    return 0;
}

static int test_evt_build(void)
{
    hall_evt_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.tenant = "default";
    evt.task = "t1";
    evt.category = "progress";
    evt.node = "n1";
    evt.owner = NULL;
    evt.ts_utc = "20260829T000000000";
    evt.content = "{\"a\":1}";
    evt.prev = "default.t1.progress.20260829T000000000.0002.json";
    evt.seq = 3;
    evt.gseq = 42;

    char out[2048];
    TEST_ASSERT(hall_evt_build(&evt, out, sizeof(out)) == 0, "build ok");

    const char *expect =
        "{\"file\":{\"id\":\"default.t1.progress.20260829T000000000.0003.json\","
        "\"category\":\"progress\",\"schema\":\"task-file-v1\",\"tenant_id\":\"default\","
        "\"task_id\":\"t1\",\"node_id\":\"n1\",\"ts_utc\":\"20260829T000000000\","
        "\"seq\":3,\"gseq\":42,"
        "\"prev_file\":\"default.t1.progress.20260829T000000000.0002.json\"},"
        "\"access\":{\"owner_role\":\"cognition\","
        "\"write_roles\":[\"cognition\",\"executor\"],\"read_roles\":[\"cognition\"]},"
        "\"content\":{\"a\":1}}";
    TEST_ASSERT(strcmp(out, expect) == 0, "envelope is byte-exact");

    hall_evt_t c2;
    memset(&c2, 0, sizeof(c2));
    c2.tenant = "default";
    c2.task = "t2";
    c2.category = "chain";
    c2.owner = "cognition";
    c2.ts_utc = "20260829T000000001";
    c2.content = "{}";
    c2.seq = 1;
    c2.gseq = 1;
    TEST_ASSERT(hall_evt_build(&c2, out, sizeof(out)) == 0, "chain build ok");
    TEST_ASSERT(strstr(out, "\"write_roles\":[\"cognition\"]") != NULL, "chain cognition-only");
    TEST_ASSERT(strstr(out, "\"node_id\":\"\"") != NULL, "missing node defaults to empty");
    TEST_ASSERT(strstr(out, "\"prev_file\":\"\"") != NULL, "missing prev defaults to empty");

    char tiny[8];
    TEST_ASSERT(hall_evt_build(&evt, tiny, sizeof(tiny)) == -1, "truncation fail-closed");
    TEST_ASSERT(hall_evt_build(&evt, NULL, sizeof(out)) == -1, "NULL out rejected");
    evt.content = NULL;
    TEST_ASSERT(hall_evt_build(&evt, out, sizeof(out)) == -1, "NULL content rejected");
    return 0;
}

static int test_event_write_readback(void)
{
    const char *task = "he-t1";
    const char *cat = "progress";
    TEST_ASSERT(hall_evt_write(task, cat, NULL, "{\"event\":\"task_queued\"}") == 0, "write ok");

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/default/%s/%s", g_root, task, cat);
    char fname[256];
    TEST_ASSERT(he_max_seq_file(dir, fname, sizeof(fname)) == 0, "find event");

    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dir, fname);
    char buf[2048];
    TEST_ASSERT(he_read(path, buf, sizeof(buf)) > 0, "read event");

    char v[256];
    TEST_ASSERT(he_json_str(buf, "id", v, sizeof(v)) == 0, "id present");
    TEST_ASSERT(strcmp(v, fname) == 0, "file.id equals file name");
    TEST_ASSERT(he_json_str(buf, "category", v, sizeof(v)) == 0, "category present");
    TEST_ASSERT(strcmp(v, cat) == 0, "category round-trips");
    TEST_ASSERT(he_json_str(buf, "task_id", v, sizeof(v)) == 0, "task_id present");
    TEST_ASSERT(strcmp(v, task) == 0, "task_id round-trips");
    TEST_ASSERT(he_json_str(buf, "prev_file", v, sizeof(v)) == 0, "prev_file present");
    TEST_ASSERT(strcmp(v, "") == 0, "first event has empty prev_file");
    TEST_ASSERT(strstr(buf, "\"write_roles\":[\"cognition\",\"executor\"]") != NULL,
                "progress is executor-writable");
    TEST_ASSERT(strstr(buf, "\"event\":\"task_queued\"") != NULL, "content embedded verbatim");
    return 0;
}

static int test_prev_file_links(void)
{
    const char *task = "he-t2";
    const char *cat = "progress";
    TEST_ASSERT(hall_evt_write(task, cat, NULL, "{\"event\":\"task_queued\"}") == 0, "first write");

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/default/%s/%s", g_root, task, cat);
    char first[256];
    TEST_ASSERT(he_max_seq_file(dir, first, sizeof(first)) == 0, "find first");

    TEST_ASSERT(hall_evt_write(task, cat, NULL, "{\"event\":\"task_started\"}") == 0, "second write");

    char second[256];
    TEST_ASSERT(he_max_seq_file(dir, second, sizeof(second)) == 0, "find second");
    TEST_ASSERT(strcmp(second, first) != 0, "second event got its own file");

    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dir, second);
    char buf[2048];
    TEST_ASSERT(he_read(path, buf, sizeof(buf)) > 0, "read second");
    char v[256];
    TEST_ASSERT(he_json_str(buf, "prev_file", v, sizeof(v)) == 0, "prev_file present");
    TEST_ASSERT(strcmp(v, first) == 0, "prev_file links to the previous event");
    return 0;
}

static int test_prev_file_category_isolated(void)
{
    const char *task = "he-t3";
    TEST_ASSERT(hall_evt_write(task, "chain", NULL, "{\"event\":\"chat_start\"}") == 0,
                "chain write");

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/default/%s/chain", g_root, task);
    char fname[256];
    TEST_ASSERT(he_max_seq_file(dir, fname, sizeof(fname)) == 0, "find chain event");
    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dir, fname);
    char buf[2048];
    TEST_ASSERT(he_read(path, buf, sizeof(buf)) > 0, "read chain event");
    TEST_ASSERT(strstr(buf, "\"write_roles\":[\"cognition\"]") != NULL, "chain cognition-only");

    TEST_ASSERT(hall_evt_write(task, "result", NULL, "{\"event\":\"tool_result\"}") == 0,
                "result write");
    snprintf(dir, sizeof(dir), "%s/default/%s/result", g_root, task);
    TEST_ASSERT(he_max_seq_file(dir, fname, sizeof(fname)) == 0, "find result event");
    snprintf(path, sizeof(path), "%s/%s", dir, fname);
    TEST_ASSERT(he_read(path, buf, sizeof(buf)) > 0, "read result event");
    char v[256];
    TEST_ASSERT(he_json_str(buf, "prev_file", v, sizeof(v)) == 0, "prev_file present");
    TEST_ASSERT(strcmp(v, "") == 0, "decision chain is per-category isolated");
    return 0;
}

static int test_seq_increments(void)
{
    const char *task = "he-t4";
    const char *cat = "result";
    for (int i = 0; i < 3; i++) {
        char content[64];
        snprintf(content, sizeof(content), "{\"i\":%d}", i);
        TEST_ASSERT(hall_evt_write(task, cat, NULL, content) == 0, "write loop ok");
    }
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/default/%s/%s", g_root, task, cat);
    char fname[256];
    TEST_ASSERT(he_max_seq_file(dir, fname, sizeof(fname)) == 0, "find latest");
    TEST_ASSERT(strstr(fname, ".0003.json") != NULL, "seq increments to 3");
    return 0;
}

static int test_invalid_params(void)
{
    TEST_ASSERT(hall_evt_write(NULL, "progress", NULL, "{}") == -1, "NULL task rejected");
    TEST_ASSERT(hall_evt_write("", "progress", NULL, "{}") == -1, "empty task rejected");
    TEST_ASSERT(hall_evt_write("t", NULL, NULL, "{}") == -1, "NULL category rejected");
    TEST_ASSERT(hall_evt_write("t", "", NULL, "{}") == -1, "empty category rejected");
    TEST_ASSERT(hall_evt_write("t", "progress", NULL, NULL) == -1, "NULL content rejected");
    TEST_ASSERT(hall_evt_write("t", "progress", NULL, "") == -1, "empty content rejected");
    TEST_ASSERT(hall_evt_write("../escape", "progress", NULL, "{}") == -1,
                "path traversal rejected");
    TEST_ASSERT(hall_evt_write("t", "a/b", NULL, "{}") == -1, "slash category rejected");
    return 0;
}

int main(void)
{
    isolate_data_dir();
    printf("test_hall_event: AIRY_HOME=%s\n", g_home);

    /* gseq 续接必须首位：先于任何 hall_evt_write 落下带 gseq 的磁盘事件 */
    TEST_RUN(test_gseq_resumption);
    TEST_RUN(test_comp_valid);
    TEST_RUN(test_clock_utc);
    TEST_RUN(test_roles_of);
    TEST_RUN(test_evt_name);
    TEST_RUN(test_evt_build);
    TEST_RUN(test_event_write_readback);
    TEST_RUN(test_prev_file_links);
    TEST_RUN(test_prev_file_category_isolated);
    TEST_RUN(test_seq_increments);
    TEST_RUN(test_invalid_params);

    printf("\n=== Results: %d passed, %d failed ===\n", passed_tests, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
