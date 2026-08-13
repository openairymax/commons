// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file checkpoint_internal.h
 * @brief Checkpoint module internal shared definitions: global state
 * variables, path macros and cross-file helper declarations.
 */

#ifndef AIRY_CHECKPOINT_INTERNAL_H
#define AIRY_CHECKPOINT_INTERNAL_H

#include "checkpoint.h"

#include <logging.h> /* LOG_ERROR/LOG_INFO/LOG_WARN/LOG_DEBUG → log_write() */
#include <types.h> /* AIRY_SUCCESS */
#include "platform.h" /* airy_time_ns/airy_mtx_* */
#include "error.h" /* AIRY_ERROR/airy_err_t/AIRY_ERR_STATE_ERROR */
#include "atomic_compat.h"
#include "airy_memory.h"

#define CHECKPOINT_DIRECTORY "checkpoints"
#define CHECKPOINT_FILE_PREFIX "checkpoint_"
#define CHECKPOINT_FILE_EXTENSION ".json"
#define MAX_CHECKPOINT_PATH 1024
#define CHECKPOINT_VERSION 1
#define MAX_LINE_LENGTH 8192
#define MAX_VALUE_LENGTH 4096

extern char g_checkpoint_storage_path[MAX_CHECKPOINT_PATH];
extern atomic_int g_checkpoint_initialized;
extern airy_mtx_t g_checkpoint_mutex;
extern atomic_int g_checkpoint_mutex_initialized;
extern airy_checkpoint_stats_t g_checkpoint_stats;

extern airy_checkpoint_hook_fn g_auto_hook;
extern void *g_auto_hook_user_data;
extern uint64_t g_auto_interval_ms;

uint32_t calculate_checksum(const char *data, size_t len);

const char *state_to_string(airy_checkpoint_state_t state);

airy_checkpoint_state_t string_to_state(const char *s);

char *safe_strdup(const char *src);

void init_fields(airy_task_checkpoint_t *cp, const char *task_id, const char *session_id,
                 uint64_t seq);

int build_filepath_with_seq(const char *task_id, uint64_t seq, char *buf, size_t size);

uint64_t *collect_task_seqs(const char *task_id, size_t *out_count);

#endif /* AIRY_CHECKPOINT_INTERNAL_H */
