/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file platform_sync.h
 * @brief Cross-platform compatibility layer - synchronization primitives
 *
 * Mutex / condition variable API, RAII mutex guard and atomic helpers.
 * Domain split of platform.h (2026-08-27).
 *
 * @see platform.h aggregate entry
 */

#ifndef AIRY_RT_PLATFORM_SYNC_H
#define AIRY_RT_PLATFORM_SYNC_H

#include "platform_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initialize a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_mtx_init(airy_mtx_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex mutex pointer
 */
void airy_mtx_destroy(airy_mtx_t *mutex);

/**
 * @brief Lock a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_mtx_lock(airy_mtx_t *mutex);

/**
 * @brief Try to lock a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure or already locked
 */
int airy_mtx_trylock(airy_mtx_t *mutex);

/**
 * @brief Unlock a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_mtx_unlock(airy_mtx_t *mutex);

/**
 * @brief Dynamically create a mutex (allocate and initialize)
 * @return mutex pointer, NULL on failure
 */
airy_mtx_t *airy_mtx_create(void);

/**
 * @brief Dynamically destroy a mutex (destroy and free memory)
 * @param mutex mutex pointer
 */
void airy_mtx_free(airy_mtx_t *mutex);


/**
 * @brief Initialize a condition variable
 * @param cond condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_init(airy_cond_t *cond);

/**
 * @brief Destroy a condition variable
 * @param cond condition variable pointer
 */
void airy_cond_destroy(airy_cond_t *cond);

/**
 * @brief Wait on a condition variable
 * @param cond condition variable pointer
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_wait(airy_cond_t *cond, airy_mtx_t *mutex);

/**
 * @brief Timed wait on a condition variable
 * @param cond condition variable pointer
 * @param mutex mutex pointer
 * @param timeout_ms timeout in milliseconds
 * @return 0 on success, non-zero on failure or timeout
 */
int airy_cond_timedwait(airy_cond_t *cond, airy_mtx_t *mutex, uint32_t timeout_ms);

/**
 * @brief Wake up one waiting thread
 * @param cond condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_signal(airy_cond_t *cond);

/**
 * @brief Wake up all waiting threads
 * @param cond condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_broadcast(airy_cond_t *cond);

/**
 * @brief Dynamically create a condition variable (allocate and initialize)
 * @return condition variable pointer, NULL on failure
 */
airy_cond_t *airy_cond_create(void);

/**
 * @brief Dynamically destroy a condition variable (destroy and free memory)
 * @param cond condition variable pointer
 */
void airy_cond_free(airy_cond_t *cond);


/* d8 cleanup: migrated from sync_compat.h to platform.h (the RAII guard
 * depends on airy_mtx_lock/unlock, logically a helper of the platform.h
 * API). Removes sync_compat.h's compatibility-layer positioning. */

/**
 * @defgroup mutex_guard RAII mutex guard (P0.18.3)
 * @{
 *
 * AIRY_MUTEX_LOCK_GUARD combines mutex lock + automatic unlock on scope
 * exit, eliminating manual lock/unlock pairing boilerplate.
 *
 * Based on GCC/Clang __attribute__((cleanup)) for RAII semantics.
 * The guard tracks lock state and only unlocks in cleanup when the lock
 * was acquired, avoiding unlock on an unheld mutex. MSVC falls back to
 * lock-only (manual unlock required).
 *
 * Usage:
 *   AIRY_MUTEX_LOCK_GUARD(m);
 *
 *
 * Note: the macro does not check whether locking succeeded. If locking can
 * fail (e.g. deadlock detection), use airy_mtx_lock + manual if check +
 * airy_mtx_unlock.
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief Mutex guard type - tracks lock state
 *
 * Stores the mutex pointer and lock state, used to decide at cleanup
 * whether to unlock.
 */
typedef struct {
    airy_mtx_t *mutex;
    bool acquired;
} airy_mtx_guard_t;

/**
 * @brief Mutex guard cleanup function (auto-invoked by the cleanup attribute)
 *
 * Automatically called when a variable marked with AIRY_MUTEX_LOCK_GUARD
 * leaves scope. Unlocks only when acquired is true and mutex is non-NULL,
 * avoiding unlock on an unheld mutex.
 *
 * @param g pointer to the guard variable
 */
static inline void airy_mtx_guard_cleanup(airy_mtx_guard_t *g)
{
    if (g->acquired && g->mutex) {
        airy_mtx_unlock(g->mutex);
        g->acquired = false;
        g->mutex = NULL;
    }
}

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII mutex guard: lock + auto-unlock on scope exit
 *
 * @param m mutex variable (airy_mtx_t type; the macro takes its address &m)
 *
 * Usage example:
 *   static airy_mtx_t my_lock;
 *   airy_mtx_init(&my_lock);
 *   {
 *       AIRY_MUTEX_LOCK_GUARD(my_lock);
 *
 *   }
 *
 * @note Uses __COUNTER__ to generate a unique variable name, so the macro
 *       can be used multiple times in the same scope
 * @note On lock failure acquired=false, cleanup will not unlock; subsequent
 *       code runs in the unlocked state
 */
#define AIRY_MUTEX_LOCK_GUARD_(m, counter)                                                  \
    airy_mtx_guard_t __attribute__((cleanup(airy_mtx_guard_cleanup))) __guard_##counter = { \
        .mutex = &(m), .acquired = (airy_mtx_lock(&(m)) == 0)}

/* Two levels of indirection: force __COUNTER__ to expand to a number before
 * ## concatenation, avoiding variable name collisions. A direct
 * AIRY_MUTEX_LOCK_GUARD_(m, __COUNTER__) would paste into
 * __guard___COUNTER__ (a literal, not expanded), causing name collisions
 * when used multiple times in the same scope. */
#define AIRY_MUTEX_LOCK_GUARD_EXPAND(m, counter) AIRY_MUTEX_LOCK_GUARD_(m, counter)
#define AIRY_MUTEX_LOCK_GUARD(m) AIRY_MUTEX_LOCK_GUARD_EXPAND(m, __COUNTER__)

#elif defined(_MSC_VER)

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII mutex guard (MSVC -- falls back to lock-only, manual unlock
 *        required)
 *
 * MSVC does not support __attribute__((cleanup)); this macro only locks.
 * With MSVC, airy_mtx_unlock must be called manually before returning.
 */
#define AIRY_MUTEX_LOCK_GUARD(m) ((void)airy_mtx_lock(&(m)))

#else

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII mutex guard (unknown compiler - falls back to lock-only,
 *        manual unlock required)
 */
#define AIRY_MUTEX_LOCK_GUARD(m) ((void)airy_mtx_lock(&(m)))

#endif

/** @} */ /* end of mutex_guard */

/* d8 cleanup: sync_compat.h has been migrated, but some code still uses the
 * AIRY_MUTEX_* macro form. These provide compatibility mappings to the
 * airy_mtx_* functions so callers don't need to be rewritten one by one.
 * Calling convention: callers pass a pointer (e.g.
 * AIRY_MUTEX_LOCK(&ctx->mutex)); the macros forward the pointer directly
 * to the airy_mtx_* functions.
 * New code should use the airy_mtx_init/lock/unlock/destroy function form
 * directly. */

/**
 * @def AIRY_MUTEX_INIT(m, attr)
 * @brief Initialize a mutex (compat macro - forwards to airy_mtx_init)
 * @param m airy_mtx_t* pointer
 * @param attr unused (kept for pthread_mutex_init signature compatibility)
 * @return 0 on success, non-zero on failure
 */
#define AIRY_MUTEX_INIT(m, attr) airy_mtx_init(m)

/**
 * @def AIRY_MUTEX_DESTROY(m)
 * @brief Destroy a mutex (compat macro - forwards to airy_mtx_destroy)
 * @param m airy_mtx_t* pointer
 */
#define AIRY_MUTEX_DESTROY(m) airy_mtx_destroy(m)

/**
 * @def AIRY_MUTEX_LOCK(m)
 * @brief Lock a mutex (compat macro - forwards to airy_mtx_lock)
 * @param m airy_mtx_t* pointer
 * @return 0 on success, non-zero on failure
 */
#define AIRY_MUTEX_LOCK(m) airy_mtx_lock(m)

/**
 * @def AIRY_MUTEX_UNLOCK(m)
 * @brief Unlock a mutex (compat macro - forwards to airy_mtx_unlock)
 * @param m airy_mtx_t* pointer
 * @return 0 on success, non-zero on failure
 */
#define AIRY_MUTEX_UNLOCK(m) airy_mtx_unlock(m)

/**
 * @def AIRY_MUTEX_TRYLOCK(m)
 * @brief Try to lock a mutex (compat macro - forwards to airy_mtx_trylock)
 * @param m airy_mtx_t* pointer
 * @return 0 on success, non-zero on failure (EBUSY means already locked)
 */
#define AIRY_MUTEX_TRYLOCK(m) airy_mtx_trylock(m)


/* ==================== Atomic helpers ==================== */

#ifndef AIRY_ATOMIC_INT_T_DEFINED
#define AIRY_ATOMIC_INT_T_DEFINED
#include "atomic_compat.h"
typedef atomic_int airy_atomic_int_t;
#endif

int airy_atomic_load(airy_atomic_int_t *atomic);
void airy_atomic_store(airy_atomic_int_t *atomic, int value);
int airy_atomic_fetch_add(airy_atomic_int_t *atomic, int value);
int airy_atomic_fetch_sub(airy_atomic_int_t *atomic, int value);


#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_SYNC_H */
