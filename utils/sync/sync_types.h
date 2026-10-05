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

#include "airy_memory.h"
#include "atomic_compat.h"
#include "sync.h"
#include "sync_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 原子统计计数器镜像。
 *
 * 读者可在持有读锁期间并发上报统计（读锁本身即是共享的），因此计数器
 * 必须是原子的，否则任何多读者路径天生构成 data race。对外快照类型
 * sync_stats_t 保持纯值语义（便于打印与跨编译器传递），本镜像仅存在于
 * 内部头；sync_get_stats() 负责在此镜像上取时点快照并转换为 sync_stats_t。
 */
typedef struct {
    atomic_size_t lock_count;
    atomic_size_t unlock_count;
    atomic_size_t wait_count;
    atomic_size_t timeout_count;
    atomic_size_t deadlock_count;
    atomic_uint64_t total_wait_time_ms;
    atomic_uint64_t max_wait_time_ms;
} sync_stats_ctr_t;

/*
 * 家族公共头：各具锁结构体的首个成员，承载类型标签、初始化态、命名与
 * 统计。以下各 struct 以本类型为首成员（C11 6.7.2.1p15），故
 * sync_internal 的生命周期单源可经首成员指针安全访问，不依赖公共前驱
 * 序列的别名灰区。
 */
typedef struct {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_ctr_t stats;
} sync_lock_hdr_t;

struct sync_mutex {
    sync_lock_hdr_t hdr;
    platform_mutex_t mutex;
};

struct sync_recursive_mutex {
    sync_lock_hdr_t hdr;
    size_t recursive_count;
    uint64_t owner_thread;
    platform_mutex_t mutex;
};

struct sync_rwlock {
    sync_lock_hdr_t hdr;
    platform_rwlock_t rwlock;
#ifdef _WIN32
    /* SRWLock 的释放必须与获取方向一致，且只有以写模式获取的线程才以写模式
     * 释放。记账写者线程 ID（0 表示当前无写者）；读者不触碰该域。 */
    atomic_uint writer_owner;
#endif
};

struct sync_spinlock {
    sync_lock_hdr_t hdr;
    platform_spinlock_t lock;
};

struct sync_semaphore {
    sync_lock_hdr_t hdr;
    unsigned int max_value;
    platform_semaphore_t semaphore;
};

struct sync_condition {
    sync_lock_hdr_t hdr;
    platform_condition_t cond;
};

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SYNC_TYPES_H */
