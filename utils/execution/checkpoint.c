// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file checkpoint.c
 * @brief AgentRT task checkpoint implementation (production v0.1.0) -
 * lifecycle and core state machine domain.
 *
 * Keeps the checkpoint module entry and core state machine: init/shutdown/
 * create/stats/destroy, plus cross-file shared internal helpers and
 * global state definitions.
 *
 * v0.1.0 changes:
 * - CROSS-01: airy_mtx_t -> airy_mtx_t
 * - CROSS-03: time(NULL) -> airy_time_ns()
 * - New auto-checkpoint hook mechanism (CoreLoopThree integration)
 * - Stronger JSON restore parsing robustness
 *
 * SP02 decoupling: migrated from daemons/common/src/ to
 * commons/utils/execution/, removing atoms/coreloopthree's physical
 * dependency on the daemons layer (ACC-SP02 decoupling point #1).
 * During the move SVC_LOG_* macros were replaced with the commons-layer
 * LOG_* macros (both call log_write()), and the daemons-layer
 * svc_logger.h and daemon_errors.h dependencies were removed.
 */

#include "checkpoint.h"
#include "checkpoint_internal.h"

#include <logging.h> /* LOG_ERROR/LOG_INFO/LOG_WARN/LOG_DEBUG → log_write() */
#include <types.h> /* AIRY_SUCCESS */
#include <platform.h> /* AIRY_HOME 权威路径：airy_data_dir() 收敛 checkpoint 落盘；airy_time_ns/airy_mtx_* */
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

char g_checkpoint_storage_path[MAX_CHECKPOINT_PATH] = {0};
atomic_int g_checkpoint_initialized = 0;
airy_mtx_t g_checkpoint_mutex;
atomic_int g_checkpoint_mutex_initialized = 0;
airy_checkpoint_stats_t g_checkpoint_stats = {0};

uint32_t calculate_checksum(const char *data, size_t len)
{
    if (!data || len == 0)
        return 0;
    uint32_t checksum = 0;
    for (size_t i = 0; i < len && data[i] != '\0'; i++) {
        checksum = (checksum << 1) ^ (uint32_t)(unsigned char)data[i];
    }
    return checksum;
}

const char *state_to_string(airy_checkpoint_state_t state)
{
    static const char *state_strings[] = {[CHECKPOINT_STATE_PENDING] = "pending",
                                          [CHECKPOINT_STATE_COMPLETED] = "completed",
                                          [CHECKPOINT_STATE_FAILED] = "failed",
                                          [CHECKPOINT_STATE_INVALID] = "invalid"};
    if (state >= 0 && state <= CHECKPOINT_STATE_INVALID)
        return state_strings[state];
    return "unknown";
}

airy_checkpoint_state_t string_to_state(const char *s)
{
    if (!s)
        return CHECKPOINT_STATE_INVALID;
    if (strcmp(s, "pending") == 0)
        return CHECKPOINT_STATE_PENDING;
    if (strcmp(s, "completed") == 0)
        return CHECKPOINT_STATE_COMPLETED;
    if (strcmp(s, "failed") == 0)
        return CHECKPOINT_STATE_FAILED;
    return CHECKPOINT_STATE_INVALID;
}

char *safe_strdup(const char *src)
{

    if (!src) {
        return NULL;
    }
    size_t len = strlen(src);
    char *d = (char *)AIRY_MALLOC(len + 1);
    if (d) {
        __builtin_memcpy(d, src, len);
        d[len] = '\0';
    }
    return d;
}

static char **safe_str_array_dup(char **src, size_t count)
{

    if (!src || count == 0) {
        return NULL;
    }
    char **dst = (char **)AIRY_CALLOC(count, sizeof(char *));
    if (!dst) {
        AIRY_LOG_ERROR("C-L07: Checkpoint: ARRAY-DUP-FAIL — OOM for count=%zu", count);
        return NULL;
    }
    __builtin_memset(dst, 0, sizeof(char *) * count);
    for (size_t i = 0; i < count; i++) {
        dst[i] = safe_strdup(src[i]);
        if (!dst[i] && src[i]) {
            AIRY_LOG_ERROR("C-L07: Checkpoint: ARRAY-DUP-FAIL — OOM at index=%zu", i);
            for (size_t j = 0; j < i; j++)
                AIRY_FREE(dst[j]);
            AIRY_FREE(dst);
            return NULL;
        }
    }
    return dst;
}

void init_fields(airy_task_checkpoint_t *cp, const char *task_id, const char *session_id,
                 uint64_t seq)
{
    if (!cp)
        return;
    __builtin_memset(cp, 0, sizeof(*cp));
    if (task_id) {
        AIRY_STRNCPY_TERM(cp->task_id, task_id, sizeof(cp->task_id));
    }
    if (session_id) {
        AIRY_STRNCPY_TERM(cp->session_id, session_id, sizeof(cp->session_id));
    }
    cp->sequence_num = seq;
    cp->timestamp = airy_time_ns();
    cp->state = CHECKPOINT_STATE_PENDING;
}

static airy_err_t copy_nodes(airy_task_checkpoint_t *cp, char **completed, size_t ccount,
                             char **pending, size_t pcount)
{
    if (!cp)
        return AIRY_EINVAL;
    cp->completed_count = ccount;
    if (ccount > 0) {
        cp->completed_nodes = safe_str_array_dup(completed, ccount);
        if (!cp->completed_nodes)
            return AIRY_ENOMEM;
    }
    cp->pending_count = pcount;
    if (pcount > 0) {
        cp->pending_nodes = safe_str_array_dup(pending, pcount);
        if (!cp->pending_nodes) {
            if (cp->completed_nodes) {
                for (size_t i = 0; i < ccount; i++)
                    AIRY_FREE(cp->completed_nodes[i]);
                AIRY_FREE(cp->completed_nodes);
            }
            return AIRY_ENOMEM;
        }
    }
    return AIRY_SUCCESS;
}

/* ==================== Public API ==================== */
const char *airy_checkpoint_default_dir(void)
{
    static char g_cp_dir[MAX_CHECKPOINT_PATH];
    snprintf(g_cp_dir, sizeof(g_cp_dir), "%s/" CHECKPOINT_DIRECTORY, airy_data_dir());
    return g_cp_dir;
}

airy_err_t airy_checkpoint_init(const char *storage_path)
{
    if (g_checkpoint_initialized)
        return AIRY_SUCCESS;
    const char *path = storage_path ? storage_path : airy_checkpoint_default_dir();
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(g_checkpoint_storage_path)) {
        AIRY_LOG_ERROR("C-L07: Checkpoint: INIT-FAIL — invalid storage path "
                  "len=%zu max=%zu",
                  len, sizeof(g_checkpoint_storage_path));
        return AIRY_EINVAL;
    }
    __builtin_memcpy(g_checkpoint_storage_path, path, len);
    g_checkpoint_storage_path[len] = '\0';

    {
        int expected = 0;
        /* once-init（0.1.6f 强化）：CAS 成功 acq_rel 发布，失败 relaxed 即可 */
        if (atomic_compare_exchange_strong_explicit(&g_checkpoint_mutex_initialized, &expected, 1,
                                                    memory_order_acq_rel,
                                                    memory_order_relaxed)) {
            if (airy_mtx_init(&g_checkpoint_mutex) != 0) {
                atomic_store_explicit(&g_checkpoint_mutex_initialized, 0, memory_order_release);
                AIRY_LOG_ERROR("C-L07: Checkpoint: INIT-FAIL — mutex init failed "
                          "path=%s",
                          g_checkpoint_storage_path);
                return AIRY_ERR_STATE_ERROR;
            }
        }
    }

    __builtin_memset(&g_checkpoint_stats, 0, sizeof(g_checkpoint_stats));
    atomic_store_explicit(&g_checkpoint_initialized, 1, memory_order_release);
    AIRY_LOG_INFO("C-L07: Checkpoint: INIT-OK path=%s", g_checkpoint_storage_path);
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_shutdown(void)
{
    if (!atomic_load_explicit(&g_checkpoint_initialized, memory_order_acquire))
        return AIRY_SUCCESS;
    if (atomic_load_explicit(&g_checkpoint_mutex_initialized, memory_order_acquire)) {
        airy_mtx_destroy(&g_checkpoint_mutex);
        atomic_store_explicit(&g_checkpoint_mutex_initialized, 0, memory_order_release);
    }
    g_auto_hook = NULL;
    g_auto_hook_user_data = NULL;
    atomic_store_explicit(&g_checkpoint_initialized, 0, memory_order_release);
    AIRY_LOG_INFO("C-L07: Checkpoint: SHUTDOWN-OK "
             "total=%llu success=%llu failed=%llu",
             (unsigned long long)g_checkpoint_stats.total_checkpoints,
             (unsigned long long)g_checkpoint_stats.successful_checkpoints,
             (unsigned long long)g_checkpoint_stats.failed_checkpoints);
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_create(const char *task_id, const char *session_id,
                                  uint64_t sequence_num, const char *state_json,
                                  char **completed_nodes, size_t completed_count,
                                  char **pending_nodes, size_t pending_count,
                                  airy_task_checkpoint_t **out_cp)
{

    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!task_id || !state_json || !out_cp)
        return AIRY_EINVAL;

    airy_task_checkpoint_t *cp =
        (airy_task_checkpoint_t *)AIRY_CALLOC(1, sizeof(airy_task_checkpoint_t));
    if (!cp) {
        AIRY_LOG_ERROR("C-L07: Checkpoint: CREATE-FAIL — OOM for task_id=%s", task_id);
        return AIRY_ENOMEM;
    }

    init_fields(cp, task_id, session_id, sequence_num);

    cp->state_json = safe_strdup(state_json);
    if (!cp->state_json) {
        AIRY_FREE(cp);
        return AIRY_ENOMEM;
    }
    cp->state_size = strlen(state_json);

    airy_err_t err = copy_nodes(cp, completed_nodes, completed_count, pending_nodes, pending_count);
    if (err != AIRY_SUCCESS) {
        AIRY_FREE(cp->state_json);
        AIRY_FREE(cp);
        return err;
    }

    cp->checksum = calculate_checksum(state_json, strlen(state_json));
    *out_cp = cp;
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_get_stats(airy_checkpoint_stats_t *stats)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!stats)
        return AIRY_EINVAL;
    airy_mtx_lock(&g_checkpoint_mutex);
    __builtin_memcpy(stats, &g_checkpoint_stats, sizeof(airy_checkpoint_stats_t));
    airy_mtx_unlock(&g_checkpoint_mutex);
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_verify(const airy_task_checkpoint_t *cp, bool *is_valid)
{

    if (!cp || !is_valid)
        return AIRY_EINVAL;
    *is_valid = false;
    if (cp->state == CHECKPOINT_STATE_INVALID)
        return AIRY_SUCCESS;
    if (!cp->state_json || cp->state_size == 0)
        return AIRY_SUCCESS;
    uint32_t calc = calculate_checksum(cp->state_json, cp->state_size);
    *is_valid = (calc == cp->checksum);
    return AIRY_SUCCESS;
}

airy_err_t airy_checkpoint_destroy(airy_task_checkpoint_t *cp)
{
    if (!cp)
        return AIRY_SUCCESS;
    if (cp->state_json) {
        AIRY_FREE(cp->state_json);
        cp->state_json = NULL;
    }
    if (cp->completed_nodes) {
        for (size_t i = 0; i < cp->completed_count; i++)
            AIRY_FREE(cp->completed_nodes[i]);
        AIRY_FREE(cp->completed_nodes);
        cp->completed_nodes = NULL;
    }
    if (cp->pending_nodes) {
        for (size_t i = 0; i < cp->pending_count; i++)
            AIRY_FREE(cp->pending_nodes[i]);
        AIRY_FREE(cp->pending_nodes);
        cp->pending_nodes = NULL;
    }
    AIRY_FREE(cp);
    return AIRY_SUCCESS;
}
