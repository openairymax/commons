/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file sync_internal.h
 * @brief Sync 模块内部共享前置声明（家族 prelude）。
 *
 * 聚合家族内各实现文件共同依赖的平台抽象与标准头；sync 家族的实现文件
 * 只需包含本头，即可获得平台类型与内部 helper 声明。
 */

#ifndef AIRY_RT_SYNC_INTERNAL_H
#define AIRY_RT_SYNC_INTERNAL_H

#include "sync.h"
#include "sync_platform.h"
#include "sync_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

char *sync_internal_strdup(const char *str);
sync_result_t sync_internal_posix_error_to_result(int error_code);
void sync_stats_reset(sync_stats_ctr_t *stats);
void sync_stats_snapshot(const sync_stats_ctr_t *stats, sync_stats_t *out);
void sync_internal_update_stats_lock(sync_stats_ctr_t *stats, int64_t elapsed_ms);
void sync_internal_update_stats_timeout(sync_stats_ctr_t *stats);
void sync_internal_update_stats_wait(sync_stats_ctr_t *stats, int64_t elapsed_ms);

sync_result_t sync_mtx_lock(platform_mutex_t *mtx, const sync_timeout_t *timeout,
                            sync_stats_ctr_t *stats);
sync_result_t sync_rw_lock(platform_rwlock_t *rwlock, bool write, const sync_timeout_t *timeout,
                           sync_stats_ctr_t *stats);

#endif
