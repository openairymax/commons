// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file sync.c
 * @brief Unified thread synchronization primitives - core layer.
 *
 * Provides cross-platform, safe, efficient thread synchronization
 * primitives for Windows and POSIX: mutex, condition variable, semaphore,
 * rwlock, etc.
 *
 * @note This file is the module entry point; implementations are split
 *       across:
 *       - sync_mutex.c: mutex
 *       - sync_recursive_mutex.c: recursive mutex
 *       - sync_rwlock.c: rwlock
 *       - sync_spinlock.c: spinlock
 *       - sync_semaphore.c: semaphore
 *       - sync_condition.c: condition variable
 *       - sync_barrier.c: barrier
 *       - sync_event.c: event
 */

#include "sync.h"

#include "error.h"
#include "sync_internal.h"
#include "sync_types.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#include <synchapi.h>
#include <windows.h>
#else
#include <errno.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "sync_platform.h"

#include "logging.h"

typedef struct {
    sync_error_callback_t error_callback;
    void *user_context;
    bool initialized;
} sync_global_state_t;

static sync_global_state_t g_sync_state = {NULL, NULL, false};

static bool g_initialized = false;

sync_result_t sync_init(sync_error_callback_t error_callback, void *context)
{
    if (g_initialized) {
        return SYNC_SUCCESS;
    }

    g_sync_state.error_callback = error_callback;
    g_sync_state.user_context = context;
    g_sync_state.initialized = true;
    g_initialized = true;

    return SYNC_SUCCESS;
}

void sync_cleanup(void)
{
    if (!g_initialized) {
        return;
    }

    g_sync_state.error_callback = NULL;
    g_sync_state.user_context = NULL;
    g_sync_state.initialized = false;
    g_initialized = false;
}

sync_type_t sync_get_type(void *lock, sync_lock_type_t lock_type)
{
    (void)lock;
    switch (lock_type) {
    case SYNC_LOCK_MUTEX:
        return SYNC_TYPE_MUTEX;
    case SYNC_LOCK_RECURSIVE_MUTEX:
        return SYNC_TYPE_RECURSIVE_MUTEX;
    case SYNC_LOCK_RWLOCK:
        return SYNC_TYPE_RWLOCK;
    case SYNC_LOCK_SPINLOCK:
        return SYNC_TYPE_SPINLOCK;
    case SYNC_LOCK_SEMAPHORE:
        return SYNC_TYPE_SEMAPHORE;
    case SYNC_LOCK_CONDITION:
        return SYNC_TYPE_CONDITION;
    case SYNC_LOCK_BARRIER:
        return SYNC_TYPE_BARRIER;
    case SYNC_LOCK_EVENT:
        return SYNC_TYPE_EVENT;
    default:
        return SYNC_TYPE_UNKNOWN;
    }
}

const char *sync_get_name(void *lock)
{
    if (lock == NULL) {
        return NULL;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;

    if (!base->initialized) {
        return NULL;
    }

    switch (base->type) {
    case SYNC_TYPE_MUTEX:
    case SYNC_TYPE_RECURSIVE_MUTEX:
        return ((struct sync_mutex *)lock)->name;
    case SYNC_TYPE_RWLOCK:
        return ((struct sync_rwlock *)lock)->name;
    case SYNC_TYPE_SPINLOCK:
        return ((struct sync_spinlock *)lock)->name;
    case SYNC_TYPE_SEMAPHORE:
        return ((struct sync_semaphore *)lock)->name;
    case SYNC_TYPE_CONDITION:
        return ((struct sync_condition *)lock)->name;
    case SYNC_TYPE_BARRIER:
        return ((struct sync_barrier *)lock)->name;
    case SYNC_TYPE_EVENT:
        return ((struct sync_event *)lock)->name;
    default:
        return NULL;
    }
}

sync_result_t sync_get_stats(void *lock, sync_stats_t *stats)
{
    if (lock == NULL || stats == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    *stats = base->stats;

    return SYNC_SUCCESS;
}

sync_result_t sync_reset_stats(void *lock)
{
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    AIRY_MEMSET(&base->stats, 0, sizeof(sync_stats_t));

    return SYNC_SUCCESS;
}

#define SYNC_MAX_OPTION_SLOTS 64

typedef struct {
    void *lock;
    uint64_t timeout_ms;
    bool priority_inherit;
    bool robust;
    bool in_use;
} sync_option_slot_t;

static sync_option_slot_t s_option_slots[SYNC_MAX_OPTION_SLOTS] = {{0}};
static size_t s_option_count = 0;

static sync_option_slot_t *find_option_slot(void *lock)
{
    for (size_t i = 0; i < s_option_count; i++) {
        if (s_option_slots[i].in_use && s_option_slots[i].lock == lock)
            return &s_option_slots[i];
    }
    return NULL;
}

static sync_option_slot_t *alloc_option_slot(void *lock)
{
    sync_option_slot_t *slot = find_option_slot(lock);
    if (slot)
        return slot;
    if (s_option_count >= SYNC_MAX_OPTION_SLOTS)
        return NULL;
    slot = &s_option_slots[s_option_count++];
    slot->lock = lock;
    slot->timeout_ms = 0;
    slot->priority_inherit = false;
    slot->robust = false;
    slot->in_use = true;
    return slot;
}

sync_result_t sync_set_option(void *lock, int option, void *value)
{
    if (lock == NULL || value == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    switch (option) {
    case SYNC_OPTION_NAME: {
        /* v0.1.1 fix: delegate to sync_set_name for strdup + registration;
         * previously base->name = name assigned the raw pointer directly,
         * causing dangling pointers/leaks/illegal free. */
        return sync_set_name(lock, (const char *)value);
    }
    case SYNC_OPTION_TIMEOUT: {
        uint64_t timeout = *(uint64_t *)value;
        sync_option_slot_t *slot = alloc_option_slot(lock);
        if (!slot)
            return SYNC_ERROR_MEMORY;
        slot->timeout_ms = timeout;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_PRIORITY_INHERIT: {
        bool pi = *(bool *)value;
        sync_option_slot_t *slot = alloc_option_slot(lock);
        if (!slot)
            return SYNC_ERROR_MEMORY;
        slot->priority_inherit = pi;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_ROBUST: {
        bool rb = *(bool *)value;
        sync_option_slot_t *slot = alloc_option_slot(lock);
        if (!slot)
            return SYNC_ERROR_MEMORY;
        slot->robust = rb;
        return SYNC_SUCCESS;
    }
    default:
        return SYNC_ERROR_UNSUPPORTED;
    }
}

sync_result_t sync_get_option(void *lock, int option, void *value)
{
    if (lock == NULL || value == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    switch (option) {
    case SYNC_OPTION_NAME: {
        const char **out = (const char **)value;
        *out = base->name;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_TIMEOUT: {
        uint64_t *out = (uint64_t *)value;
        sync_option_slot_t *slot = find_option_slot(lock);
        *out = slot ? slot->timeout_ms : 0;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_PRIORITY_INHERIT: {
        bool *out = (bool *)value;
        sync_option_slot_t *slot = find_option_slot(lock);
        *out = slot ? slot->priority_inherit : false;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_ROBUST: {
        bool *out = (bool *)value;
        sync_option_slot_t *slot = find_option_slot(lock);
        *out = slot ? slot->robust : false;
        return SYNC_SUCCESS;
    }
    default:
        return SYNC_ERROR_UNSUPPORTED;
    }
}

bool sync_is_valid(void *lock)
{
    return lock != NULL;
}

sync_result_t sync_debug(void *lock)
{
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;

    AIRY_LOG_DEBUG("[SYNC DEBUG] ====================");
    AIRY_LOG_DEBUG("[SYNC DEBUG] Lock at: %p", (void *)lock);
    AIRY_LOG_DEBUG("[SYNC DEBUG] Type: %d", base->type);
    AIRY_LOG_DEBUG("[SYNC DEBUG] Initialized: %s", base->initialized ? "true" : "false");

    const char *name = sync_get_name(lock);
    if (name != NULL) {
        AIRY_LOG_DEBUG("[SYNC DEBUG] Name: %s", name);
    } else {
        AIRY_LOG_DEBUG("[SYNC DEBUG] Name: (unnamed)");
    }

    sync_stats_t stats;
    if (sync_get_stats(lock, &stats) == SYNC_SUCCESS) {
        AIRY_LOG_DEBUG("[SYNC DEBUG] --- Statistics ---");
        AIRY_LOG_DEBUG("[SYNC DEBUG] Lock count: %zu", stats.lock_count);
        AIRY_LOG_DEBUG("[SYNC DEBUG] Unlock count: %zu", stats.unlock_count);
        AIRY_LOG_DEBUG("[SYNC DEBUG] Wait count: %zu", stats.wait_count);
        AIRY_LOG_DEBUG("[SYNC DEBUG] Timeout count: %zu", stats.timeout_count);
        AIRY_LOG_DEBUG("[SYNC DEBUG] Deadlock count: %zu", stats.deadlock_count);
        AIRY_LOG_DEBUG("[SYNC DEBUG] Total wait time: %lu ms", (unsigned long)stats.total_wait_time_ms);
        AIRY_LOG_DEBUG("[SYNC DEBUG] Max wait time: %lu ms", (unsigned long)stats.max_wait_time_ms);
    }

    AIRY_LOG_DEBUG("[SYNC DEBUG] ====================");

    return SYNC_SUCCESS;
}

uint64_t sync_get_timestamp_ms(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

void sync_sleep_ms(uint64_t ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

void sync_yield(void)
{
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

void sync_sleep(unsigned int ms)
{
    sync_sleep_ms((uint64_t)ms);
}

#define SYNC_MAX_REGISTRY 256

typedef struct {
    void *lock;
    char *name;
    sync_type_t type;
    bool in_use;
} sync_registry_entry_t;

static sync_registry_entry_t s_lock_registry[SYNC_MAX_REGISTRY];
static size_t s_registry_count = 0;

#ifdef _WIN32
static CRITICAL_SECTION s_registry_mutex;
static bool s_registry_mutex_init = false;
static void registry_ensure_init(void)
{
    if (!s_registry_mutex_init) {
        InitializeCriticalSection(&s_registry_mutex);
        s_registry_mutex_init = true;
    }
}
#define REGISTRY_LOCK()                          \
    do {                                         \
        registry_ensure_init();                  \
        EnterCriticalSection(&s_registry_mutex); \
    } while (0)
#define REGISTRY_UNLOCK() LeaveCriticalSection(&s_registry_mutex)
#else
static pthread_mutex_t s_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
#define REGISTRY_LOCK() (void)pthread_mutex_lock(&s_registry_mutex)
#define REGISTRY_UNLOCK() (void)pthread_mutex_unlock(&s_registry_mutex)
#endif

static void registry_register(void *lock, const char *name, sync_type_t type)
{
    REGISTRY_LOCK();

    for (size_t i = 0; i < s_registry_count; i++) {
        if (s_lock_registry[i].in_use && s_lock_registry[i].lock == lock) {
            if (s_lock_registry[i].name)
                AIRY_FREE(s_lock_registry[i].name);
            s_lock_registry[i].name = name ? sync_internal_strdup(name) : NULL;
            s_lock_registry[i].type = type;
            REGISTRY_UNLOCK();
            return;
        }
    }

    for (size_t i = 0; i < s_registry_count; i++) {
        if (!s_lock_registry[i].in_use) {
            s_lock_registry[i].lock = lock;
            s_lock_registry[i].name = name ? sync_internal_strdup(name) : NULL;
            s_lock_registry[i].type = type;
            s_lock_registry[i].in_use = true;
            REGISTRY_UNLOCK();
            return;
        }
    }

    if (s_registry_count < SYNC_MAX_REGISTRY) {
        s_lock_registry[s_registry_count].lock = lock;
        s_lock_registry[s_registry_count].name = name ? sync_internal_strdup(name) : NULL;
        s_lock_registry[s_registry_count].type = type;
        s_lock_registry[s_registry_count].in_use = true;
        s_registry_count++;
    }

    REGISTRY_UNLOCK();
}

static void registry_unregister(void *lock)
{
    REGISTRY_LOCK();
    for (size_t i = 0; i < s_registry_count; i++) {
        if (s_lock_registry[i].in_use && s_lock_registry[i].lock == lock) {
            if (s_lock_registry[i].name)
                AIRY_FREE(s_lock_registry[i].name);
            s_lock_registry[i].in_use = false;
            s_lock_registry[i].lock = NULL;
            s_lock_registry[i].name = NULL;
            break;
        }
    }
    REGISTRY_UNLOCK();
}

static bool registry_lock_is_held(void *lock, sync_type_t type)
{
    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized)
        return false;

    switch (type) {
    case SYNC_TYPE_MUTEX:
    case SYNC_TYPE_RECURSIVE_MUTEX: {
        struct sync_mutex *m = (struct sync_mutex *)lock;
#ifdef _WIN32
        if (TryEnterCriticalSection(&m->mutex)) {
            LeaveCriticalSection(&m->mutex);
            return false;
        }
        return true;
#else
        int rc = pthread_mutex_trylock(&m->mutex);
        if (rc == 0) {
            pthread_mutex_unlock(&m->mutex);
            return false;
        }
        return true;
#endif
    }
    case SYNC_TYPE_SPINLOCK: {
        struct sync_spinlock *sp = (struct sync_spinlock *)lock;
#ifdef _WIN32

        int expected = 0;
        if (_InterlockedCompareExchange((volatile LONG *)&sp->lock, 1, expected) == expected) {
            _InterlockedExchange((volatile LONG *)&sp->lock, 0);
            return false;
        }
        return true;
#else
        int rc = pthread_spin_trylock(&sp->lock);
        if (rc == 0) {
            pthread_spin_unlock(&sp->lock);
            return false;
        }
        return true;
#endif
    }
    case SYNC_TYPE_RWLOCK: {
        struct sync_rwlock *rw = (struct sync_rwlock *)lock;
#ifdef _WIN32
        if (TryAcquireSRWLockShared(&rw->rwlock)) {
            ReleaseSRWLockShared(&rw->rwlock);
            return false;
        }
        return true;
#else
        int rc = pthread_rwlock_tryrdlock(&rw->rwlock);
        if (rc == 0) {
            pthread_rwlock_unlock(&rw->rwlock);
            return false;
        }
        return true;
#endif
    }
    case SYNC_TYPE_SEMAPHORE: {
        struct sync_semaphore *sem = (struct sync_semaphore *)lock;
#ifdef _WIN32
        DWORD wr = WaitForSingleObject(sem->semaphore, 0);
        if (wr == WAIT_OBJECT_0) {
            ReleaseSemaphore(sem->semaphore, 1, NULL);
            return false;
        }
        return true;
#else
        int rc = sem_trywait(&sem->semaphore);
        if (rc == 0) {
            sem_post(&sem->semaphore);
            return false;
        }
        return true;
#endif
    }
    default:

        return false;
    }
}

sync_result_t sync_set_name(void *lock, const char *name)
{
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    sync_type_t type = base->type;

    const char *old_name = sync_get_name(lock);

    char *new_name = NULL;
    if (name) {
        new_name = sync_internal_strdup(name);
        if (!new_name)
            return SYNC_ERROR_MEMORY;
    }

    switch (type) {
    case SYNC_TYPE_MUTEX:
    case SYNC_TYPE_RECURSIVE_MUTEX: {
        struct sync_mutex *m = (struct sync_mutex *)lock;
        if (old_name)
            AIRY_FREE((void *)m->name);
        m->name = new_name;
        break;
    }
    case SYNC_TYPE_RWLOCK: {
        struct sync_rwlock *rw = (struct sync_rwlock *)lock;
        if (old_name)
            AIRY_FREE((void *)rw->name);
        rw->name = new_name;
        break;
    }
    case SYNC_TYPE_SPINLOCK: {
        struct sync_spinlock *sp = (struct sync_spinlock *)lock;
        if (old_name)
            AIRY_FREE((void *)sp->name);
        sp->name = new_name;
        break;
    }
    case SYNC_TYPE_SEMAPHORE: {
        struct sync_semaphore *sem = (struct sync_semaphore *)lock;
        if (old_name)
            AIRY_FREE((void *)sem->name);
        sem->name = new_name;
        break;
    }
    case SYNC_TYPE_CONDITION: {
        struct sync_condition *c = (struct sync_condition *)lock;
        if (old_name)
            AIRY_FREE((void *)c->name);
        c->name = new_name;
        break;
    }
    case SYNC_TYPE_BARRIER: {
        struct sync_barrier *b = (struct sync_barrier *)lock;
        if (old_name)
            AIRY_FREE((void *)b->name);
        b->name = new_name;
        break;
    }
    case SYNC_TYPE_EVENT: {
        struct sync_event *e = (struct sync_event *)lock;
        if (old_name)
            AIRY_FREE((void *)e->name);
        e->name = new_name;
        break;
    }
    default:
        if (new_name)
            AIRY_FREE(new_name);
        return SYNC_ERROR_UNSUPPORTED;
    }

    if (name) {
        registry_register(lock, name, type);
    } else {
        registry_unregister(lock);
    }

    return SYNC_SUCCESS;
}

sync_result_t sync_check_deadlock(sync_deadlock_info_t *info, size_t max_info_size)
{
    if (info == NULL || max_info_size == 0) {
        return SYNC_ERROR_INVALID;
    }

    AIRY_MEMSET(info, 0, sizeof(sync_deadlock_info_t));
    info->detection_time = (uint64_t)time(NULL);

    size_t cap = (max_info_size < SYNC_MAX_REGISTRY) ? max_info_size : SYNC_MAX_REGISTRY;
    char *held_names[SYNC_MAX_REGISTRY];
    for (size_t i = 0; i < SYNC_MAX_REGISTRY; i++)
        held_names[i] = NULL;
    size_t held_count = 0;
    size_t total_held = 0;

    REGISTRY_LOCK();
    for (size_t i = 0; i < s_registry_count; i++) {
        if (!s_lock_registry[i].in_use)
            continue;

        struct sync_mutex *base = (struct sync_mutex *)s_lock_registry[i].lock;

        if (base == NULL || !base->initialized) {
            if (s_lock_registry[i].name)
                AIRY_FREE(s_lock_registry[i].name);
            s_lock_registry[i].in_use = false;
            s_lock_registry[i].lock = NULL;
            s_lock_registry[i].name = NULL;
            continue;
        }

        if (registry_lock_is_held(s_lock_registry[i].lock, s_lock_registry[i].type)) {
            total_held++;
            if (held_count < cap && s_lock_registry[i].name) {
                held_names[held_count] = sync_internal_strdup(s_lock_registry[i].name);
                if (held_names[held_count])
                    held_count++;
            }
        }
    }
    REGISTRY_UNLOCK();

    info->lock_count = total_held;

    if (total_held > 0) {

        if (held_count > 0) {
            info->lock_names = (char **)AIRY_CALLOC(held_count, sizeof(char *));
            if (info->lock_names) {
                for (size_t i = 0; i < held_count; i++)
                    info->lock_names[i] = held_names[i];
            } else {

                for (size_t i = 0; i < held_count; i++) {
                    if (held_names[i])
                        AIRY_FREE(held_names[i]);
                }
            }
        }
        return SYNC_ERROR_DEADLOCK;
    }

    return SYNC_SUCCESS;
}

uint64_t sync_get_thread_id(void)
{
#ifdef _WIN32
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

bool sync_atomic_cas(volatile void *ptr, uintptr_t expected, uintptr_t desired)
{
#ifdef _WIN32
    /* 按指针位宽选择 Interlocked 变体：32 位 Windows 下 uintptr_t 为 32 位，
     * 硬编码 _Interlocked*64 会按 8 字节读写造成越界与语义错误 */
#if defined(_WIN64)
    return _InterlockedCompareExchange64((volatile LONG64 *)ptr, (LONG64)desired,
                                         (LONG64)expected) == (LONG64)expected;
#else
    return _InterlockedCompareExchange((volatile LONG *)ptr, (LONG)desired,
                                       (LONG)expected) == (LONG)expected;
#endif
#else
    return __sync_bool_compare_and_swap((volatile uintptr_t *)ptr, expected, desired);
#endif
}

uintptr_t sync_atomic_add(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
#if defined(_WIN64)
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, (LONG64)value);
#else
    return (uintptr_t)_InterlockedExchangeAdd((volatile LONG *)ptr, (LONG)value);
#endif
#else
    return __sync_fetch_and_add((volatile uintptr_t *)ptr, value);
#endif
}

uintptr_t sync_atomic_sub(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
#if defined(_WIN64)
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, -(LONG64)value);
#else
    return (uintptr_t)_InterlockedExchangeAdd((volatile LONG *)ptr, -(LONG)value);
#endif
#else
    return __sync_fetch_and_sub((volatile uintptr_t *)ptr, value);
#endif
}

uintptr_t sync_atomic_load(volatile void *ptr)
{
#ifdef _WIN32
#if defined(_WIN64)
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, 0);
#else
    return (uintptr_t)_InterlockedExchangeAdd((volatile LONG *)ptr, 0);
#endif
#else
    return __sync_fetch_and_add((volatile uintptr_t *)ptr, 0);
#endif
}

void sync_atomic_store(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
#if defined(_WIN64)
    _InterlockedExchange64((volatile LONG64 *)ptr, (LONG64)value);
#else
    _InterlockedExchange((volatile LONG *)ptr, (LONG)value);
#endif
#else
    __sync_lock_test_and_set((volatile uintptr_t *)ptr, value);
#endif
}
