// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file checkpoint_snapshot.c
 * @brief AgentRT task checkpoint - snapshot domain.
 *
 * Handles checkpoint snapshot export/import: serializes the latest
 * checkpoint into the SNAPSHOT_V1 text format, or restores task identity
 * from a snapshot file.
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

static void fprintf_sanitized(FILE *fp, const char *label, const char *str)
{
    char _cp_buf[2048];
    snprintf(_cp_buf, sizeof(_cp_buf), "%s: ", label);
    fputs(_cp_buf, fp);
    if (!str) {
        fputc('\n', fp);
        return;
    }
    for (const char *p = str; *p; p++) {
        fputc((*p == '\n' || *p == '\r') ? ' ' : *p, fp);
    }
    fputc('\n', fp);
}

airy_err_t airy_snapshot_create(const char *task_id, const char *snap_path)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!task_id || !snap_path)
        return AIRY_EINVAL;

    airy_task_checkpoint_t *cp = NULL;
    airy_err_t err = airy_checkpoint_restore(task_id, 0, &cp);
    if (err != AIRY_SUCCESS)
        return err;

    FILE *fp = fopen(snap_path, "wb");
    if (!fp) {
        airy_checkpoint_destroy(cp);
        LOG_ERROR("C-L07: Checkpoint: SNAPSHOT-CREATE-FAIL — cannot open file "
                  "path=%s task_id=%s errno=%d",
                  snap_path, task_id, errno);
        return AIRY_EIO;
    }

    char _cp_buf[2048];
    fputs("SNAPSHOT_V1\n", fp);
    fprintf_sanitized(fp, "TaskID", cp->task_id); /* BAN-70 EXEMPT: checkpoint snapshot output */
    fprintf_sanitized(fp, "SessionID",
                      cp->session_id); /* BAN-70 EXEMPT: checkpoint snapshot output */
    snprintf(_cp_buf, sizeof(_cp_buf), "SequenceNum: %lu\n", (unsigned long)cp->sequence_num);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "Timestamp: %lu\n", (unsigned long)cp->timestamp);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "StateSize: %zu\n", cp->state_size);
    fputs(_cp_buf, fp);
    fputs("---DATA---\n", fp);
    if (cp->state_json && cp->state_size > 0)
        fwrite(cp->state_json, 1, cp->state_size, fp);
    fputs("\n---END---\n", fp);
    fclose(fp);
    airy_checkpoint_destroy(cp);
    return AIRY_SUCCESS;
}

airy_err_t airy_snapshot_restore(const char *snap_path, char **tid)
{
    if (!g_checkpoint_initialized)
        return AIRY_ENOTINIT;
    if (!snap_path || !tid)
        return AIRY_EINVAL;

    FILE *fp = fopen(snap_path, "rb");
    if (!fp) {
        LOG_WARN("C-L07: Checkpoint: SNAPSHOT-RESTORE-FAIL — file not found "
                 "path=%s errno=%d",
                 snap_path, errno);
        return AIRY_ENOENT;
    }

    char header[64];
    if (!fgets(header, sizeof(header), fp) || strncmp(header, "SNAPSHOT_V1", 11) != 0) {
        fclose(fp);
        LOG_ERROR("C-L07: Checkpoint: SNAPSHOT-RESTORE-FAIL — bad header "
                  "path=%s header=%.20s",
                  snap_path, header);
        return AIRY_EIO;
    }

    char line[256];
    *tid = NULL;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "TaskID: ", 8) == 0) {
            *tid = safe_strdup(line + 8);
            if (*tid) {
                size_t tlen = strlen(*tid);
                if (tlen > 0 && (*tid)[tlen - 1] == '\n')
                    (*tid)[tlen - 1] = '\0';
            }
        } else if (strncmp(line, "---DATA---", 10) == 0) {
            break;
        }
    }
    fclose(fp);
    if (!*tid)
        return AIRY_EIO;
    return AIRY_SUCCESS;
}
