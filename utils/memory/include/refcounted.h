/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file refcounted.h
 * @brief P1.21: ownership model + reference counting spec.
 *
 * Provides the thread-safe reference counting base structure
 * refcounted_t, paired with the refcount_alloc/retain/release API to
 * manage the lifetime of shared objects.
 *
 * Design:
 *   - _Atomic uint32_t refcount for thread-safe refcounting
 *   - deleter callback invoked automatically when the count hits zero
 *   - CAS-based lock-free increment/decrement
 *
 * Ownership model:
 *   @ownership alloc   - refcount_alloc() returns an object holding 1 ref
 *   @ownership retain  - refcount_retain() adds 1 ref, caller gains ownership
 *   @ownership release - refcount_release() drops 1 ref, destroys at zero
 *   @ownership borrow  - bare pointer access does not touch the refcount;
 *                        the caller must not hold it across scopes
 */

#ifndef AIRY_RT_REFCOUNTED_H
#define AIRY_RT_REFCOUNTED_H

#include "airy_memory.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief P1.21.1: refcount base structure (embedded at the object head).
 *
 * Usage:
 *   typedef struct {
 *       refcounted_t rc;
 *       char data[];
 *   } my_shared_buf_t;
 */
typedef struct {
    _Atomic uint32_t refcount;
    void (*deleter)(void *obj);
} refcounted_t;


/**
 * @brief Get the current refcount (debug only).
 * @param rc refcounted_t pointer
 * @return Current refcount
 */
static inline uint32_t refcount_get(const refcounted_t *rc)
{
    if (!rc)
        return 0;
    return atomic_load_explicit(&rc->refcount, memory_order_acquire);
}


/**
 * @brief P1.21.2: Allocate a refcounted object.
 *
 * @ownership alloc - the returned object holds 1 reference
 *
 * @param obj_size    Total object size (refcounted_t + data)
 * @param deleter     Destroy callback at zero (NULL uses default free)
 * @return Allocated object pointer, NULL on failure
 */
static inline void *refcount_alloc(size_t obj_size, void (*deleter)(void *obj))
{
    if (obj_size < sizeof(refcounted_t)) {
        return NULL;
    }

    refcounted_t *rc = (refcounted_t *)AIRY_CALLOC(1, obj_size);
    if (!rc)
        return NULL;

    atomic_init(&rc->refcount, 1);
    rc->deleter = deleter;

    return (void *)rc;
}


/**
 * @brief P1.21.2: Increment the refcount (retain).
 *
 * @ownership retain - the caller obtains 1 new reference
 *
 * @param obj  Object pointer (must point at the refcounted_t head)
 * @return obj itself (for chaining), NULL if obj is NULL
 *
 * Usage example:
 *   my_buf_t *buf2 = (my_buf_t *)refcount_retain((refcounted_t *)buf1);
 */
static inline void *refcount_retain(void *obj)
{
    if (!obj)
        return NULL;

    refcounted_t *rc = (refcounted_t *)obj;
    /* V4.0 fix: use a CAS loop instead of fetch_add(relaxed) followed by
     * an old==0 check.
     *
     * The old implementation still performed fetch_add on already-freed
     * memory after the refcount hit zero (object destroyed), causing
     * use-after-free; the relaxed order also failed to synchronize with
     * the releasing thread's deleter call.
     *
     * Fix: load the current value with acquire and only increment via CAS
     * while it is > 0.
     * - If refcount==0 (object destroyed or being destroyed), the CAS is
     *   not executed and NULL is returned directly.
     * - acq_rel ordering ensures the increment synchronizes with the
     *   releasing thread's fetch_sub.
     * - compare_exchange_weak is usable in a loop (spurious failures only
     *   cause a reload and retry). */
    uint32_t cur = atomic_load_explicit(&rc->refcount, memory_order_acquire);
    while (cur != 0) {
        if (atomic_compare_exchange_weak_explicit(&rc->refcount, &cur, cur + 1,
                                                  memory_order_acq_rel, memory_order_acquire)) {
            return obj;
        }
    }
    return NULL;
}

/**
 * @brief P1.21.2: Release a reference (release).
 *
 * @ownership release - the caller releases 1 reference
 *
 * When the refcount hits zero, deleter is invoked automatically to
 * destroy the object. obj becomes invalid after the call; the caller
 * must not access it.
 *
 * @param obj  Object pointer (must point at the refcounted_t head)
 * @return true if the object was destroyed (refcount hit zero),
 *         false if other references remain
 */
static inline bool refcount_release(void *obj)
{
    if (!obj)
        return false;

    refcounted_t *rc = (refcounted_t *)obj;
    uint32_t old = atomic_fetch_sub_explicit(&rc->refcount, 1, memory_order_acq_rel);

    if (old == 1) {

        if (rc->deleter) {
            rc->deleter(obj);
        } else {
            AIRY_FREE(obj);
        }
        return true;
    }

    if (old == 0) {

        atomic_store_explicit(&rc->refcount, 0, memory_order_release);
    }

    return false;
}


/**
 * @brief Convenience macro to embed refcounted_t at the struct head.
 *
 * Usage:
 *   typedef struct {
 *       AIRY_REFCOUNTED_HEADER;
 *       char buffer[4096];
 *   } ipc_shared_buf_t;
 *
 *   ipc_shared_buf_t *buf = (ipc_shared_buf_t *)
 *       refcount_alloc(sizeof(ipc_shared_buf_t), NULL);
 */
#define AIRY_REFCOUNTED_HEADER refcounted_t _rc

/**
 * @brief Get the containing struct from a refcounted_t pointer.
 * @param ptr    refcounted_t head address
 * @param type   Containing struct type
 * @param member refcounted_t member name
 */
#define REFCOUNTED_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * @brief Convenience retain macro (auto type conversion).
 * @param obj  Object pointer
 * @return Pointer of the same type
 */
#define REFCOUNT_RETAIN(obj) ((typeof(obj))refcount_retain((void *)(obj)))

/**
 * @brief Convenience release macro.
 * @param obj  Object pointer
 * @return true if destroyed
 */
#define REFCOUNT_RELEASE(obj) refcount_release((void *)(obj))

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_REFCOUNTED_H */