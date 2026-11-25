// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file checkpoint_session.c
 * @brief AgentRT task checkpoint - session management domain.
 *
 * Handles checkpoint session maintenance: task id listing, expiry cleanup
 * and the auto-checkpoint hook mechanism (CoreLoopThree integration).
 */

#include "checkpoint_internal.h"

airy_checkpoint_hook_fn g_auto_hook = NULL;
void *g_auto_hook_user_data = NULL;
uint64_t g_auto_interval_ms = 0;

airy_err_t airy_checkpoint_ids(char ***out_task_ids, size_t *out_count)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!out_task_ids || !out_count)
        return AIRY_EINVAL;

    *out_task_ids = NULL;
    *out_count = 0;

    size_t cnt = 0;
    char **ids = collect_task_ids(&cnt);
    if (!ids || cnt == 0) {
        AIRY_FREE(ids);
        return AIRY_SUCCESS;
    }

    *out_task_ids = ids;
    *out_count = cnt;
    return AIRY_SUCCESS;
}

/* 按 mtime 清理过期 checkpoint 文件；now_sec 为 CLOCK_REALTIME 基线。 */
static void cp_sweep_old(uint64_t max_age_sec, uint64_t now_sec)
{
#ifdef _WIN32
    char pattern[MAX_CHECKPOINT_PATH];
    snprintf(pattern, sizeof(pattern), "%s/*.json", g_checkpoint_storage_path);

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
            if (mod_sec <= now_sec && (now_sec - mod_sec) > max_age_sec)
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
            if (stat(filepath, &st) != 0)
                continue;
            uint64_t mtime = (uint64_t)st.st_mtime;
            if (mtime <= now_sec && (now_sec - mtime) > max_age_sec)
                remove(filepath);
        }
        closedir(dir);
    }
#endif
}

airy_err_t airy_checkpoint_cleanup(uint64_t max_age_sec, size_t max_cnt)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;

    airy_mtx_lock(&g_checkpoint_mutex);
    /* Age is measured on the CLOCK_REALTIME baseline (time(NULL)) so the
     * comparison is meaningful against st_mtime (also CLOCK_REALTIME).
     * The subtraction must never be allowed to underflow: coarse
     * filesystem timestamps can report an mtime up to one tick ahead of
     * time(NULL), and an unsigned wrap would make a freshly written
     * checkpoint look infinitely old and get deleted. Guard the ordering
     * before subtracting instead of relying on wraparound. */
    uint64_t now_sec = (uint64_t)time(NULL);

    if (max_age_sec > 0) {
        cp_sweep_old(max_age_sec, now_sec);
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
