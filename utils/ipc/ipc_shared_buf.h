/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file ipc_shared_buf.h
 * @brief P1.21.3: IPC shared buffer - refcounted_t-based lifetime management.
 *
 * Provides a thread-safe IPC shared buffer whose lifetime is managed by
 * refcounted_t, supporting zero-copy sharing (multiple consumers hold
 * references to the same buffer).
 *
 * Design:
 *   - ipc_shared_buf_t embeds refcounted_t at its head, managed by
 *     refcount_alloc/retain/release
 *   - ipc_buf_create()  allocates a refcounted buffer (@ownership alloc,
 *                       refcount = 1)
 *   - ipc_buf_dup()     increments the refcount (@ownership retain)
 *   - ipc_buf_release() decrements the refcount, frees at zero
 *
 * Thread safety:
 *   - Refcount ops are based on _Atomic uint32_t, lock-free atomic ops
 *   - Producer-consumer: producer create -> consumer dup -> each releases
 *
 * Usage example:
 *   // producer
 *   ipc_shared_buf_t *buf = ipc_buf_create(4096);
 *   memcpy(buf->data, payload, payload_len);
 *   send_to_consumer(buf);  // consumer will dup
 *   ipc_buf_release(buf);   // producer releases its own reference
 *
 *   // consumer
 *   void on_message(ipc_shared_buf_t *buf) {
 *       process(buf->data, buf->size);
 *       ipc_buf_release(buf);
 *   }
 */

#ifndef AIRY_RT_IPC_SHARED_BUF_H
#define AIRY_RT_IPC_SHARED_BUF_H

#include "refcounted.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * IPC shared buffer structure
 * ============================================================================ */

/**
 * @brief P1.21.3: IPC shared buffer structure (embeds refcounted_t header).
 *
 * A flexible array member data[] stores the payload; the whole structure
 * is allocated in one shot via refcount_alloc.
 *
 * @ownership alloc  - ipc_buf_create() returns a buffer with refcount = 1
 * @ownership retain - ipc_buf_dup() increments the refcount
 * @ownership release - ipc_buf_release() decrements the refcount, frees at 0
 */
typedef struct {
    AIRY_REFCOUNTED_HEADER;
    size_t size;
    uint64_t timestamp;
    char data[];
} ipc_shared_buf_t;

/* ============================================================================
 * Lifetime API
 * ============================================================================ */

/**
 * @brief P1.21.3: Create a refcounted IPC shared buffer.
 *
 * @ownership alloc - the returned buffer holds 1 reference
 *
 * @param size Data area size (bytes), not including the struct header
 * @return Allocated buffer pointer, NULL on failure
 */
static inline ipc_shared_buf_t *ipc_buf_create(size_t size)
{

    if (size > SIZE_MAX - sizeof(ipc_shared_buf_t)) {
        return NULL;
    }

    size_t total_size = sizeof(ipc_shared_buf_t) + size;
    ipc_shared_buf_t *buf = (ipc_shared_buf_t *)refcount_alloc(total_size, NULL);
    if (!buf)
        return NULL;

    buf->size = size;
    buf->timestamp = 0;

    return buf;
}

/**
 * @brief P1.21.3: Duplicate an IPC shared buffer (increments refcount).
 *
 * @ownership retain - the caller obtains 1 new reference
 *
 * @param buf Source buffer (may be NULL)
 * @return buf itself (for chaining), NULL if buf is NULL
 *
 * @note Data is not copied; only the refcount is incremented
 *       (zero-copy sharing)
 */
static inline ipc_shared_buf_t *ipc_buf_dup(ipc_shared_buf_t *buf)
{
    return (ipc_shared_buf_t *)refcount_retain((void *)buf);
}

/**
 * @brief P1.21.3: Release an IPC shared buffer reference.
 *
 * @ownership release - the caller releases 1 reference
 *
 * When the refcount reaches zero the memory is freed automatically.
 * buf becomes invalid after the call.
 *
 * @param buf Buffer pointer (may be NULL)
 */
static inline void ipc_buf_release(ipc_shared_buf_t *buf)
{
    refcount_release((void *)buf);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get the current refcount of a buffer (debug only).
 * @param buf Buffer pointer
 * @return Current refcount, 0 if buf is NULL
 */
static inline uint32_t ipc_buf_refcount(const ipc_shared_buf_t *buf)
{
    if (!buf)
        return 0;
    return refcount_get(&buf->_rc);
}

/**
 * @brief Get the buffer data area size.
 * @param buf Buffer pointer
 * @return Data area size (bytes), 0 if buf is NULL
 */
static inline size_t ipc_buf_size(const ipc_shared_buf_t *buf)
{
    if (!buf)
        return 0;
    return buf->size;
}

/**
 * @brief Compute the total memory footprint of a buffer.
 * @param buf Buffer pointer
 * @return Total bytes (struct + data area), 0 if buf is NULL
 */
static inline size_t ipc_buf_total_size(const ipc_shared_buf_t *buf)
{
    if (!buf)
        return 0;
    return sizeof(ipc_shared_buf_t) + buf->size;
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_SHARED_BUF_H */