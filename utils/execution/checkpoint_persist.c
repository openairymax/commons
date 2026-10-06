// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file checkpoint_persist.c
 * @brief AgentRT task checkpoint - persistence and restore domain.
 *
 * Handles checkpoint save/load: file path and sequence number parsing,
 * directory scanning, JSON serialization write and read-back restore.
 */

#include "checkpoint_internal.h"

#include "json_key.h"

/* Build the checkpoint file path including the sequence number.
 *
 * v0.1.1 change: file name went from checkpoint_{task_id}.json to
 * checkpoint_{task_id}_{seq}.json so each sequence number is persisted
 * independently instead of overwriting the same file. This is the
 * correctness basis for list/restore_seq - the old format overwrote the
 * file on every save, keeping only 1 checkpoint per task. */
int build_filepath_with_seq(const char *task_id, uint64_t seq, char *buf, size_t size)
{
    if (!task_id || !buf || size == 0)
        return AIRY_ERR_INVALID_PARAM;
    int n = snprintf(buf, size, "%s/%s%s_%llu%s", g_checkpoint_storage_path, CHECKPOINT_FILE_PREFIX,
                     task_id, (unsigned long long)seq, CHECKPOINT_FILE_EXTENSION);
    return (n > 0 && (size_t)n < size) ? 0 : AIRY_ERR_OVERFLOW;
}

/* File name format checkpoint_{task_id}_{seq}.json. task_id may itself
 * contain '_', so the sequence number is the trailing segment after the
 * last '_' and must be all digits; task_id is everything before it. This
 * is the single SSoT parser shared by every directory scan below. */
static bool parse_cp_name(const char *name, char *tid, size_t tid_sz, uint64_t *out_seq)
{
    if (!name || !tid || tid_sz == 0 || !out_seq)
        return false;

    const size_t pfx = sizeof(CHECKPOINT_FILE_PREFIX) - 1;
    const size_t sfx = sizeof(CHECKPOINT_FILE_EXTENSION) - 1;
    size_t nlen = strlen(name);
    if (nlen < pfx + sfx + 2)
        return false;
    if (strncmp(name, CHECKPOINT_FILE_PREFIX, pfx) != 0)
        return false;
    if (strcmp(name + nlen - sfx, CHECKPOINT_FILE_EXTENSION) != 0)
        return false;

    const char *mid = name + pfx;
    size_t mid_len = nlen - pfx - sfx;

    size_t sep = mid_len;
    while (sep > 0 && mid[sep - 1] != '_')
        sep--;
    if (sep == 0 || sep == mid_len)
        return false;

    size_t tlen = sep - 1;
    if (tlen == 0 || tlen >= tid_sz)
        return false;

    uint64_t seq = 0;
    for (size_t i = sep; i < mid_len; i++) {
        char c = mid[i];
        if (c < '0' || c > '9')
            return false;
        seq = seq * 10 + (uint64_t)(c - '0');
    }

    AIRY_MEMCPY(tid, mid, tlen);
    tid[tlen] = '\0';
    *out_seq = seq;
    return true;
}

typedef bool (*cp_entry_fn)(const char *task_id, uint64_t seq, void *ctx);

/* Single directory-scan底座 shared by all collectors: enumerate the
 * checkpoint directory and hand each valid {task_id, seq} to fn. fn
 * returns false to stop early (e.g. on allocation failure). */
static void scan_cp_entries(cp_entry_fn fn, void *ctx)
{
#ifdef _WIN32
    char pattern[MAX_CHECKPOINT_PATH];
    snprintf(pattern, sizeof(pattern), "%s/%s*.json", g_checkpoint_storage_path,
             CHECKPOINT_FILE_PREFIX);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        char tid[128];
        uint64_t seq;
        if (parse_cp_name(fd.cFileName, tid, sizeof(tid), &seq) && !fn(tid, seq, ctx))
            break;
    } while (FindNextFile(h, &fd));
    FindClose(h);
#else
    DIR *dir = opendir(g_checkpoint_storage_path);
    if (!dir)
        return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char tid[128];
        uint64_t seq;
        if (parse_cp_name(entry->d_name, tid, sizeof(tid), &seq) && !fn(tid, seq, ctx))
            break;
    }
    closedir(dir);
#endif
}

/* 增长型收集器：SSoT 扩容机制件，seq/id 两个 collector 共用。 */
typedef struct {
    void *items;
    size_t count;
    size_t cap;
    size_t item_size;
} grow_t;

static void *grow_push(grow_t *g)
{
    if (g->count == g->cap) {
        size_t ncap = g->cap ? g->cap * 2 : 16;
        void *ni = AIRY_REALLOC(g->items, ncap * g->item_size);
        if (!ni)
            return NULL;
        g->items = ni;
        g->cap = ncap;
    }
    return (char *)g->items + (g->count++) * g->item_size;
}

typedef struct {
    const char *want;
    grow_t buf;
    bool failed;
} seq_sink_t;

static bool seq_visitor(const char *task_id, uint64_t seq, void *ctx)
{
    seq_sink_t *s = (seq_sink_t *)ctx;
    if (s->failed)
        return false;
    if (strcmp(task_id, s->want) != 0)
        return true;
    uint64_t *slot = (uint64_t *)grow_push(&s->buf);
    if (!slot) {
        s->failed = true;
        return false;
    }
    *slot = seq;
    return true;
}

typedef struct {
    grow_t buf;
    bool failed;
} id_sink_t;

static bool id_visitor(const char *task_id, uint64_t seq, void *ctx)
{
    id_sink_t *s = (id_sink_t *)ctx;
    (void)seq;
    if (s->failed)
        return false;
    char **base = (char **)s->buf.items;
    for (size_t i = 0; i < s->buf.count; i++) {
        if (strcmp(base[i], task_id) == 0)
            return true;
    }
    char *dup = AIRY_STRDUP(task_id);
    if (!dup) {
        s->failed = true;
        return false;
    }
    char **slot = (char **)grow_push(&s->buf);
    if (!slot) {
        AIRY_FREE(dup);
        s->failed = true;
        return false;
    }
    *slot = dup;
    return true;
}

/* Collect all sequence numbers for a task. Returns an AIRY_MALLOC-allocated
 * array (caller must AIRY_FREE) with *out_count set; returns NULL and
 * *out_count=0 when nothing matches. Not locked: only reads the directory;
 * stats are updated by the caller under lock. */
uint64_t *collect_task_seqs(const char *task_id, size_t *out_count)
{
    *out_count = 0;
    if (!task_id)
        return NULL;

    seq_sink_t s = {task_id, {NULL, 0, 0, sizeof(uint64_t)}, false};
    scan_cp_entries(seq_visitor, &s);
    if (s.failed || s.buf.count == 0) {
        AIRY_FREE(s.buf.items);
        return NULL;
    }
    *out_count = s.buf.count;
    return (uint64_t *)s.buf.items;
}

/* Collect the distinct task_ids that own at least one checkpoint. Returns an
 * AIRY_MALLOC-allocated string array (caller frees each entry plus the
 * array) with *out_count set; returns NULL and *out_count=0 when empty. */
char **collect_task_ids(size_t *out_count)
{
    *out_count = 0;

    id_sink_t s = {{NULL, 0, 0, sizeof(char *)}, false};
    scan_cp_entries(id_visitor, &s);
    if (s.failed) {
        char **base = (char **)s.buf.items;
        for (size_t i = 0; i < s.buf.count; i++)
            AIRY_FREE(base[i]);
        AIRY_FREE(s.buf.items);
        return NULL;
    }
    *out_count = s.buf.count;
    return (char **)s.buf.items;
}

/* Find the highest sequence number for a task; 0 means no checkpoint.
 * Note: production code (adapter/loop/engine) always saves with
 * sequence_num > 0, so 0 is a safe "not found" sentinel. */
static uint64_t find_latest_seq(const char *task_id)
{
    size_t cnt = 0;
    uint64_t *seqs = collect_task_seqs(task_id, &cnt);
    if (!seqs)
        return 0;

    uint64_t max_seq = 0;
    for (size_t i = 0; i < cnt; i++) {
        if (seqs[i] > max_seq)
            max_seq = seqs[i];
    }
    AIRY_FREE(seqs);
    return max_seq;
}

static void write_json_escaped_str(FILE *fp, const char *str)
{
    if (!str)
        return;
    for (const char *p = str; *p; p++) {
        switch (*p) {
        case '"':
            fputs("\\\"", fp);
            break;
        case '\\':
            fputs("\\\\", fp);
            break;
        case '\n':
            fputs("\\n", fp);
            break;
        case '\r':
            fputs("\\r", fp);
            break;
        case '\t':
            fputs("\\t", fp);
            break;
        default:
            fputc(*p, fp);
            break;
        }
    }
}

/* JSON 字符串值写出机制件：转义串外包引号构成完整 JSON 值，NULL
 * 写出 null 字面量。转义本体复用 write_json_escaped_str。 */
static void write_json_value(FILE *fp, const char *str)
{
    if (!str) {
        fputs("null", fp);
        return;
    }
    fputc('"', fp);
    write_json_escaped_str(fp, str);
    fputc('"', fp);
}

static char *json_extract_string(const char *json, const char *key)
{
    const char *p = airy_json_key(json, key);

    if (!p || *p != '"')
        return NULL;
    p++;

    size_t cap = 512;
    char *val = (char *)AIRY_MALLOC(cap);
    if (!val) {
        AIRY_LOG_ERROR("C-L07: Checkpoint: JSON-EXTRACT-FAIL — OOM (malloc) for key=%s", key);
        return NULL;
    }
    size_t len = 0;

    while (*p && *p != '"' && *p != '\n') {
        if (*p == '\\' && *(p + 1)) {
            p++;
            char esc = '\\';
            switch (*p) {
            case 'n':
                esc = '\n';
                break;
            case 't':
                esc = '\t';
                break;
            case 'r':
                esc = '\r';
                break;
            case '"':
                esc = '"';
                break;
            default:
                break;
            }
            if (len + 2 >= cap) {
                cap *= 2;
                val = (char *)AIRY_REALLOC(val, cap);
                if (!val) {
                    AIRY_LOG_ERROR(
                        "C-L07: Checkpoint: JSON-EXTRACT-FAIL — OOM (realloc escape) for key=%s",
                        key);
                    return NULL;
                }
            }
            val[len++] = esc;
            p++;
        } else {
            if (len + 2 >= cap) {
                cap *= 2;
                val = (char *)AIRY_REALLOC(val, cap);
                if (!val) {
                    AIRY_LOG_ERROR(
                        "C-L07: Checkpoint: JSON-EXTRACT-FAIL — OOM (realloc char) for key=%s",
                        key);
                    return NULL;
                }
            }
            val[len++] = *p;
            p++;
        }
    }
    val[len] = '\0';
    return val;
}

static uint64_t json_extract_uint64(const char *json, const char *key)
{
    const char *p = airy_json_key(json, key);
    uint64_t v = 0;

    if (!p)
        return 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (uint64_t)(*p - '0');
        p++;
    }
    return v;
}

airy_err_t airy_checkpoint_save(airy_task_checkpoint_t *cp)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!cp || !cp->task_id[0])
        return AIRY_EINVAL;

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_filepath_with_seq(cp->task_id, cp->sequence_num, filepath, sizeof(filepath)) != 0)
        return AIRY_EINVAL;

    char tmppath[MAX_CHECKPOINT_PATH];
    snprintf(tmppath, sizeof(tmppath), "%s.tmp", filepath);

    FILE *fp = fopen(tmppath, "w");
    if (!fp) {
        airy_mtx_lock(&g_checkpoint_mutex);
        g_checkpoint_stats.failed_checkpoints++;
        airy_mtx_unlock(&g_checkpoint_mutex);
        AIRY_LOG_ERROR("C-L07: Checkpoint: SAVE-FAIL — cannot open file "
                  "path=%s task_id=%s errno=%d",
                  tmppath, cp->task_id, errno);
        return AIRY_EIO;
    }

    char _cp_buf[2048];
    fputs("{\n", fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"version\": %d,\n", CHECKPOINT_VERSION);
    fputs(_cp_buf, fp);
    fputs("  \"task_id\": \"", fp);
    write_json_escaped_str(fp, cp->task_id);
    fputs("\",\n", fp);
    fputs("  \"session_id\": \"", fp);
    write_json_escaped_str(fp, cp->session_id);
    fputs("\",\n", fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"sequence_num\": %lu,\n",
             (unsigned long)cp->sequence_num);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"timestamp\": %lu,\n", (unsigned long)cp->timestamp);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"state\": \"%s\",\n", state_to_string(cp->state));
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"checksum\": %u,\n", cp->checksum);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"state_size\": %zu,\n", cp->state_size);
    fputs(_cp_buf, fp);

    fputs("  \"state_json\": ", fp);
    write_json_value(fp, cp->state_json);
    fputs(",\n", fp);

    snprintf(_cp_buf, sizeof(_cp_buf), "  \"completed_count\": %zu,\n", cp->completed_count);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"pending_count\": %zu,\n", cp->pending_count);
    fputs(_cp_buf, fp);
    fputs("  \"metadata\": \"", fp);
    write_json_escaped_str(fp, cp->metadata);
    fputs("\"\n", fp);
    fputs("}\n", fp);
    fclose(fp);

    if (rename(tmppath, filepath) != 0) {
        unlink(tmppath);
        airy_mtx_lock(&g_checkpoint_mutex);
        g_checkpoint_stats.failed_checkpoints++;
        airy_mtx_unlock(&g_checkpoint_mutex);
        AIRY_LOG_ERROR("C-L07: Checkpoint: SAVE-FAIL — rename failed "
                  "tmp=%s dst=%s task_id=%s errno=%d",
                  tmppath, filepath, cp->task_id, errno);
        return AIRY_EIO;
    }

    airy_mtx_lock(&g_checkpoint_mutex);
    cp->state = CHECKPOINT_STATE_COMPLETED;
    g_checkpoint_stats.successful_checkpoints++;
    g_checkpoint_stats.total_checkpoints++;
    g_checkpoint_stats.last_checkpoint_time = cp->timestamp;

    if (g_checkpoint_stats.total_checkpoints > 0) {
        g_checkpoint_stats.avg_checkpoint_size =
            (g_checkpoint_stats.avg_checkpoint_size * (g_checkpoint_stats.total_checkpoints - 1) +
             cp->state_size) /
            g_checkpoint_stats.total_checkpoints;
    } else {
        g_checkpoint_stats.avg_checkpoint_size = cp->state_size;
    }
    airy_mtx_unlock(&g_checkpoint_mutex);

    AIRY_LOG_DEBUG("C-L07: Checkpoint: SAVE-OK task_id=%s seq=%llu size=%zu", cp->task_id,
              (unsigned long long)cp->sequence_num, cp->state_size);
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_restore(const char *task_id, uint64_t sequence_num,
                                   airy_task_checkpoint_t **out_cp)
{

    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!task_id || !out_cp)
        return AIRY_EINVAL;

    /* sequence_num == 0 means restore the latest checkpoint: scan the
     * directory for the highest seq. sequence_num > 0 restores the
     * checkpoint with that specific sequence number. */
    uint64_t actual_seq = sequence_num;
    if (sequence_num == 0) {
        actual_seq = find_latest_seq(task_id);
        if (actual_seq == 0) {
            AIRY_LOG_WARN("C-L07: Checkpoint: RESTORE-FAIL — no checkpoint found "
                     "task_id=%s",
                     task_id);
            return AIRY_ENOENT;
        }
    }

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_filepath_with_seq(task_id, actual_seq, filepath, sizeof(filepath)) != 0)
        return AIRY_EINVAL;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        AIRY_LOG_WARN("C-L07: Checkpoint: RESTORE-FAIL — file not found "
                 "path=%s task_id=%s seq=%llu errno=%d",
                 filepath, task_id, (unsigned long long)actual_seq, errno);
        return AIRY_ENOENT;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        fclose(fp);
        AIRY_LOG_ERROR("C-L07: Checkpoint: RESTORE-FAIL — invalid file size "
                  "path=%s size=%ld task_id=%s",
                  filepath, file_size, task_id);
        return AIRY_EIO;
    }

    char *json_buf = (char *)AIRY_MALLOC((size_t)(file_size + 1));
    if (!json_buf) {
        fclose(fp);
        AIRY_LOG_ERROR("C-L07: Checkpoint: RESTORE-FAIL — OOM "
                  "path=%s size=%ld task_id=%s",
                  filepath, file_size, task_id);
        return AIRY_ENOMEM;
    }

    size_t read_len = fread(json_buf, 1, (size_t)file_size, fp);
    if (read_len != (size_t)file_size) {
        AIRY_FREE(json_buf);
        fclose(fp);
        AIRY_LOG_ERROR("C-L07: Checkpoint: RESTORE-FAIL — read error "
                  "path=%s expected=%ld actual=%zu task_id=%s",
                  filepath, file_size, read_len, task_id);
        return AIRY_EIO;
    }
    json_buf[read_len] = '\0';
    fclose(fp);

    airy_task_checkpoint_t *cp =
        (airy_task_checkpoint_t *)AIRY_CALLOC(1, sizeof(airy_task_checkpoint_t));
    if (!cp) {
        AIRY_FREE(json_buf);
        return AIRY_ENOMEM;
    }

    char *state_str = json_extract_string(json_buf, "state");
    char *sj = json_extract_string(json_buf, "state_json");

    init_fields(cp, task_id, "", 0);

    if (state_str) {
        cp->state = string_to_state(state_str);
        AIRY_FREE(state_str);
    }
    if (sj) {
        cp->state_json = sj;
        cp->state_size = strlen(sj);
        cp->checksum = calculate_checksum(sj, strlen(sj));
    }

    char *sid = json_extract_string(json_buf, "session_id");
    if (sid) {
        AIRY_STRNCPY_TERM(cp->session_id, sid, sizeof(cp->session_id));
        AIRY_FREE(sid);
        sid = NULL;
    }
    cp->sequence_num = json_extract_uint64(json_buf, "sequence_num");
    cp->timestamp = json_extract_uint64(json_buf, "timestamp");

    AIRY_FREE(json_buf);
    json_buf = NULL;
    airy_mtx_lock(&g_checkpoint_mutex);
    g_checkpoint_stats.total_restore_ops++;
    airy_mtx_unlock(&g_checkpoint_mutex);
    *out_cp = cp;
    AIRY_LOG_DEBUG("C-L07: Checkpoint: RESTORE-OK task_id=%s seq=%llu state=%s", task_id,
              (unsigned long long)cp->sequence_num, state_to_string(cp->state));
    return AIRY_SUCCESS;
}
