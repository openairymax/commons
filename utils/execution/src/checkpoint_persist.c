// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file checkpoint_persist.c
 * @brief AgentRT task checkpoint - persistence and restore domain.
 *
 * Handles checkpoint save/load: file path and sequence number parsing,
 * directory scanning, JSON serialization write and read-back restore.
 */

#include "checkpoint.h"
#include "checkpoint_internal.h"

#include <logging.h> /* LOG_ERROR/LOG_INFO/LOG_WARN/LOG_DEBUG → log_write() */
#include <types.h> /* AIRY_SUCCESS */
#include "platform.h" /* airy_time_ns/airy_mtx_* */
#include "error.h" /* AIRY_ERROR/airy_err_t/AIRY_ERR_STATE_ERROR */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include "airy_dirent.h"

#include <sys/stat.h>
#endif

#include "atomic_compat.h"
#include "airy_memory.h"

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

/* File name format checkpoint_{task_id}_{seq}.json: the middle part must
 * be all digits, avoiding prefix overlap when task_id contains '_'. */
static bool parse_seq_from_filename(const char *filename, const char *task_id, uint64_t *out_seq)
{
    if (!filename || !task_id || !out_seq)
        return false;

    char prefix[MAX_CHECKPOINT_PATH];
    int pn = snprintf(prefix, sizeof(prefix), "%s%s%s", CHECKPOINT_FILE_PREFIX, task_id, "_");
    if (pn <= 0 || (size_t)pn >= sizeof(prefix))
        return false;

    size_t prefix_len = (size_t)pn;
    if (strncmp(filename, prefix, prefix_len) != 0)
        return false;

    const char *suffix = CHECKPOINT_FILE_EXTENSION;
    size_t suffix_len = strlen(suffix);
    size_t flen = strlen(filename);
    if (flen < prefix_len + suffix_len + 1)
        return false;
    if (strcmp(filename + flen - suffix_len, suffix) != 0)
        return false;

    const char *mid = filename + prefix_len;
    size_t mid_len = flen - prefix_len - suffix_len;
    if (mid_len == 0)
        return false;
    for (size_t i = 0; i < mid_len; i++) {
        if (mid[i] < '0' || mid[i] > '9')
            return false;
    }

    uint64_t seq = 0;
    for (size_t i = 0; i < mid_len; i++) {
        seq = seq * 10 + (uint64_t)(mid[i] - '0');
    }
    *out_seq = seq;
    return true;
}

/* Scan the storage directory, collecting all sequence numbers for a task.
 * Returns an AIRY_MALLOC-allocated array (caller must AIRY_FREE) with
 * *out_count set; returns NULL and *out_count=0 when nothing matches.
 * Not locked: only reads the directory; stats are updated by the caller
 * under lock. */
uint64_t *collect_task_seqs(const char *task_id, size_t *out_count)
{
    *out_count = 0;
    if (!task_id)
        return NULL;

    size_t cap = 16;
    size_t cnt = 0;
    uint64_t *seqs = (uint64_t *)AIRY_MALLOC(cap * sizeof(uint64_t));
    if (!seqs)
        return NULL;

#ifdef _WIN32
    char pattern[MAX_CHECKPOINT_PATH];
    snprintf(pattern, sizeof(pattern), "%s/%s*.json", g_checkpoint_storage_path,
             CHECKPOINT_FILE_PREFIX);
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(pattern, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            uint64_t seq;
            if (parse_seq_from_filename(find_data.cFileName, task_id, &seq)) {
                if (cnt >= cap) {
                    cap *= 2;
                    uint64_t *ns = (uint64_t *)AIRY_REALLOC(seqs, cap * sizeof(uint64_t));
                    if (!ns) {
                        AIRY_FREE(seqs);
                        FindClose(hFind);
                        return NULL;
                    }
                    seqs = ns;
                }
                seqs[cnt++] = seq;
            }
        } while (FindNextFile(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir(g_checkpoint_storage_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            uint64_t seq;
            if (parse_seq_from_filename(entry->d_name, task_id, &seq)) {
                if (cnt >= cap) {
                    cap *= 2;
                    uint64_t *ns = (uint64_t *)AIRY_REALLOC(seqs, cap * sizeof(uint64_t));
                    if (!ns) {
                        AIRY_FREE(seqs);
                        closedir(dir);
                        return NULL;
                    }
                    seqs = ns;
                }
                seqs[cnt++] = seq;
            }
        }
        closedir(dir);
    }
#endif

    if (cnt == 0) {
        AIRY_FREE(seqs);
        return NULL;
    }
    *out_count = cnt;
    return seqs;
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

static char *json_extract_string(const char *json, const char *key)
{

    if (!json || !key) {
        return NULL;
    }
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);

    if (!p) {
        return NULL;
    }
    p += strlen(search);
    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p != ':') {
        return NULL;
    }
    p++;
    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p != '"') {
        return NULL;
    }
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
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p)
        return 0;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r'))
        p++;
    uint64_t v = 0;
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
    if (cp->state_json) {
        fputc('"', fp);
        for (const char *p = cp->state_json; *p; p++) {
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
        fputc('"', fp);
    } else {
        fputs("null", fp);
    }
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
