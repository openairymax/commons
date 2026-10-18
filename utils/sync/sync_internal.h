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

#include "airy_memory.h"
#include "platform_sync.h"
#include "sync.h"
#include "sync_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

sync_result_t sync_internal_posix_error_to_result(int error_code);

sync_result_t sync_mtx_lock(airy_mtx_t *mtx, const sync_timeout_t *timeout);
sync_result_t sync_rw_lock(airy_rwlock_t *rwlock, bool write, const sync_timeout_t *timeout);

/* 生命周期单源：sync_types.h 定义家族公共头 sync_lock_hdr_t（各具锁结构
 * 体首成员），本组函数以之为视图承载对象分配与释放/解锁的公共逻辑，使各
 * 实现文件不再各自复制同一段生命周期代码。prim 指向各结构体自身持有的
 * 平台互斥量字段。 */
sync_lock_hdr_t *sync_lock_new(size_t size);
sync_result_t sync_lock_free(sync_lock_hdr_t *hdr, airy_mtx_t *prim);
sync_result_t sync_lock_unlock(sync_lock_hdr_t *hdr, airy_mtx_t *prim);

#endif
