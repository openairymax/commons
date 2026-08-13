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
#include "sync.h"
#include "sync_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sync_mutex {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    platform_mutex_t mutex;
};

struct sync_recursive_mutex {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    size_t recursive_count;
    uint64_t owner_thread;
    platform_recursive_mutex_t mutex;
};

struct sync_rwlock {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    size_t read_count;
    bool is_writer;
    platform_rwlock_t rwlock;
};

struct sync_spinlock {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    platform_spinlock_t lock;
};

struct sync_semaphore {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    unsigned int max_value;
    platform_semaphore_t semaphore;
};

struct sync_condition {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    platform_condition_t cond;
};

struct sync_barrier {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    unsigned int count;
    unsigned int current;
    unsigned int generation;
    platform_barrier_t barrier;
};

struct sync_event {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    bool manual_reset;
    bool signaled;
    platform_event_t event;
};

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SYNC_TYPES_H */
