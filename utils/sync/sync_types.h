/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file sync_types.h
 * @brief Sync primitive internal type definitions.
 *
 * Defines the internal structs of all sync primitives for use by each
 * platform implementation file. Not exposed externally; sync-module
 * internal only.
 */

#ifndef AIRY_RT_SYNC_TYPES_H
#define AIRY_RT_SYNC_TYPES_H

#include "platform_sync.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 家族公共头：各具锁结构体的首个成员，承载初始化态。以下各 struct 以本
 * 类型为首成员（C11 6.7.2.1p15），故 sync_internal 的生命周期单源可经
 * 首成员指针安全访问，不依赖公共前驱序列的别名灰区。
 */
typedef struct {
    bool initialized;
} sync_lock_hdr_t;

struct sync_mutex {
    sync_lock_hdr_t hdr;
    airy_mtx_t mutex;
};

struct sync_rwlock {
    sync_lock_hdr_t hdr;
    airy_rwlock_t rwlock;
};

struct sync_condition {
    sync_lock_hdr_t hdr;
    airy_cond_t cond;
};

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SYNC_TYPES_H */
