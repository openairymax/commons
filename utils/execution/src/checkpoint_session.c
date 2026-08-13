// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file checkpoint_session.c
 * @brief AgentRT task checkpoint - session management domain.
 *
 * Handles checkpoint session maintenance: delete/list/expiry cleanup and
 * the auto-checkpoint hook mechanism (CoreLoopThree integration).
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

airy_checkpoint_hook_fn g_auto_hook = NULL;
void *g_auto_hook_user_data = NULL;
uint64_t g_auto_interval_ms = 0;

airy_err_t airy_checkpoint_delete(const char *task_id, uint64_t seq_num)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!task_id)
        return AIRY_EINVAL;

    if (seq_num > 0) {
        char filepath[MAX_CHECKPOINT_PATH];
        if (build_filepath_with_seq(task_id, seq_num, filepath, sizeof(filepath)) != 0)
            return AIRY_EINVAL;

        airy_mtx_lock(&g_checkpoint_mutex);
        int result = unlink(filepath);
        if (result == 0) {
            if (g_checkpoint_stats.total_checkpoints > 0)
                g_checkpoint_stats.total_checkpoints--;
            LOG_INFO("C-L07: Checkpoint: DELETE-OK task_id=%s seq=%llu", task_id,
                     (unsigned long long)seq_num);
        } else {
            LOG_WARN("C-L07: Checkpoint: DELETE-FAIL — unlink failed "
                     "task_id=%s seq=%llu errno=%d",
                     task_id, (unsigned long long)seq_num, errno);
        }
        airy_mtx_unlock(&g_checkpoint_mutex);

        return (result == 0) ? AIRY_SUCCESS : AIRY_ENOENT;
    }

    size_t cnt = 0;
    uint64_t *seqs = collect_task_seqs(task_id, &cnt);
    if (!seqs) {
        LOG_INFO("C-L07: Checkpoint: DELETE-ALL — no checkpoints for task_id=%s", task_id);
        return AIRY_ENOENT;
    }

    size_t deleted = 0;
    airy_mtx_lock(&g_checkpoint_mutex);
    for (size_t i = 0; i < cnt; i++) {
        char filepath[MAX_CHECKPOINT_PATH];
        if (build_filepath_with_seq(task_id, seqs[i], filepath, sizeof(filepath)) == 0) {
            if (unlink(filepath) == 0) {
                deleted++;
                if (g_checkpoint_stats.total_checkpoints > 0)
                    g_checkpoint_stats.total_checkpoints--;
            }
        }
    }
    airy_mtx_unlock(&g_checkpoint_mutex);
    AIRY_FREE(seqs);

    LOG_INFO("C-L07: Checkpoint: DELETE-ALL task_id=%s deleted=%zu/%zu", task_id, deleted, cnt);
    return (deleted > 0) ? AIRY_SUCCESS : AIRY_ENOENT;
}

airy_err_t airy_checkpoint_list(const char *task_id, airy_task_checkpoint_t ***out_cps,
                                size_t *out_count)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!task_id || !out_cps || !out_count)
        return AIRY_EINVAL;

    *out_cps = NULL;
    *out_count = 0;

    size_t cnt = 0;
    uint64_t *seqs = collect_task_seqs(task_id, &cnt);
    if (!seqs || cnt == 0) {
        if (seqs)
            AIRY_FREE(seqs);
        return AIRY_ENOENT;
    }

    for (size_t i = 1; i < cnt; i++) {
        uint64_t key = seqs[i];
        size_t j = i;
        while (j > 0 && seqs[j - 1] > key) {
            seqs[j] = seqs[j - 1];
            j--;
        }
        seqs[j] = key;
    }

    /* Restore checkpoints one by one. collect_task_seqs gives a snapshot
     * taken while scanning the directory; files may be removed concurrently.
     * A failed restore skips that entry instead of failing the whole
     * operation, so the result is the set that was actually restorable. */
    airy_task_checkpoint_t **arr =
        (airy_task_checkpoint_t **)AIRY_CALLOC(cnt, sizeof(airy_task_checkpoint_t *));
    if (!arr) {
        AIRY_FREE(seqs);
        return AIRY_ENOMEM;
    }

    size_t restored_count = 0;
    for (size_t i = 0; i < cnt; i++) {
        airy_task_checkpoint_t *cp = NULL;
        airy_err_t err = airy_checkpoint_restore(task_id, seqs[i], &cp);
        if (err == AIRY_SUCCESS && cp) {
            arr[restored_count++] = cp;
        }
    }
    AIRY_FREE(seqs);

    if (restored_count == 0) {
        AIRY_FREE(arr);
        return AIRY_ENOENT;
    }

    *out_cps = arr;
    *out_count = restored_count;
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_cleanup(uint64_t max_age_sec, size_t max_cnt)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;

    airy_mtx_lock(&g_checkpoint_mutex);
    /* Must use the CLOCK_REALTIME baseline (time(NULL)) to compare against
     * file st_mtime. Earlier code wrongly used airy_time_ms()
     * (CLOCK_MONOTONIC, milliseconds since boot), whose baseline differs
     * from st_mtime (CLOCK_REALTIME, seconds since 1970); the uint64_t
     * subtraction underflowed and every file was judged stale and deleted. */
    uint64_t now_sec = (uint64_t)time(NULL);

    if (max_age_sec > 0) {
        char pattern[MAX_CHECKPOINT_PATH];
        snprintf(pattern, sizeof(pattern), "%s/*.json", g_checkpoint_storage_path);

#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(pattern, &find_data);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                char filepath[MAX_CHECKPOINT_PATH];
                snprintf(filepath, sizeof(filepath), "%s/%s", g_checkpoint_storage_path,
                         find_data.cFileName);
                ULARGE_INTEGER ft;
                ft.LowPart = find_data.ftLastWriteTime.dwLowDateTime;
                ft.HighPart = find_data.ftLastWriteTime.dwHighDateTime;
                uint64_t mod_sec = (ft.QuadPart / 10000000ULL) - 11644473600ULL;
                if ((now_sec - mod_sec) > max_age_sec)
                    DeleteFileA(filepath);
            } while (FindNextFile(hFind, &find_data));
            FindClose(hFind);
        }
#else
        DIR *dir = opendir(g_checkpoint_storage_path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                size_t nlen = strlen(entry->d_name);
                if (nlen < 5 || strcmp(entry->d_name + nlen - 5, ".json") != 0)
                    continue;
                char filepath[MAX_CHECKPOINT_PATH];
                snprintf(filepath, sizeof(filepath), "%s/%s", g_checkpoint_storage_path,
                         entry->d_name);
                struct stat st;
                if (stat(filepath, &st) == 0) {
                    if ((now_sec - (uint64_t)st.st_mtime) > max_age_sec)
                        remove(filepath);
                }
            }
            closedir(dir);
        }
#endif
    }

    if (max_cnt > 0 && g_checkpoint_stats.total_checkpoints > max_cnt)
        g_checkpoint_stats.total_checkpoints = max_cnt;

    airy_mtx_unlock(&g_checkpoint_mutex);
    return AIRY_SUCCESS;
}

/* ========== Auto-checkpoint hooks (CoreLoopThree integration) ========== */
airy_err_t airy_checkpoint_set_auto_hook(airy_checkpoint_hook_fn hook, void *user_data,
                                         uint64_t interval_ms)
{

    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    airy_mtx_lock(&g_checkpoint_mutex);
    g_auto_hook = hook;
    g_auto_hook_user_data = user_data;
    g_auto_interval_ms = interval_ms;
    airy_mtx_unlock(&g_checkpoint_mutex);
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_trigger_auto(const char *task_id)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;

    airy_mtx_lock(&g_checkpoint_mutex);
    airy_checkpoint_hook_fn hook = g_auto_hook;
    void *hook_udata = g_auto_hook_user_data;
    airy_mtx_unlock(&g_checkpoint_mutex);

    if (!hook || !task_id)
        return AIRY_EINVAL;

    hook(task_id, NULL, hook_udata);
    return AIRY_SUCCESS;
}
