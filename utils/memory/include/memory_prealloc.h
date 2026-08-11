/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef AIRY_RT_MEMORY_PREALLOC_H
#define AIRY_RT_MEMORY_PREALLOC_H

#include <stddef.h>
#include <stdint.h>

/**
 * Pre-allocated emergency buffers for critical paths.
 * These buffers are allocated at startup and reserved for
 * OOM/signal/audit critical operations that must never fail.
 */

/* Pre-allocation categories */
#define AIRY_PREALLOC_SIGNAL_BUF_SIZE 4096 /* Signal handler buffer */
#define AIRY_PREALLOC_OOM_BUF_SIZE 8192 /* OOM error reporting buffer */
#define AIRY_PREALLOC_AUDIT_BUF_SIZE 16384 /* Audit log emergency buffer */
#define AIRY_PREALLOC_SHUTDOWN_BUF_SIZE 8192 /* Emergency shutdown buffer */
typedef struct airy_prealloc_pool {
    void *signal_buf;
    size_t signal_buf_size;
    int signal_buf_in_use;

    void *oom_buf;
    size_t oom_buf_size;
    int oom_buf_in_use;

    void *audit_buf;
    size_t audit_buf_size;
    int audit_buf_in_use;

    void *shutdown_buf;
    size_t shutdown_buf_size;
    int shutdown_buf_in_use;

    int initialized;
} airy_prealloc_pool_t;

/* Initialize pre-allocation pool (call at startup, before any daemon work) */
int airy_prealloc_init(void);

/* Shutdown pre-allocation pool (call at process exit) */
void airy_prealloc_shutdown(void);

/* Acquire a pre-allocated buffer for critical path use.
 * Returns NULL if buffer is already in use (only one user at a time per category).
 * Caller must release with airy_prealloc_release(). */
void *airy_prealloc_acquire(int category);

/* Release a pre-allocated buffer back to the pool */
void airy_prealloc_release(int category);

/* Check if pre-allocation is initialized */
int airy_prealloc_is_initialized(void);

/* Category constants */
#define AIRY_PREALLOC_SIGNAL 0
#define AIRY_PREALLOC_OOM 1
#define AIRY_PREALLOC_AUDIT 2
#define AIRY_PREALLOC_SHUTDOWN 3

#endif /* AIRY_RT_MEMORY_PREALLOC_H */
