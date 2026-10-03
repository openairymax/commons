// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file sync_platform.c
 * @brief Platform abstraction layer implementation for sync primitives
 *
 * Provides cross-platform implementations of mutex, condition variable,
 * semaphore, rwlock, spinlock, barrier, and event primitives.
 *
 */

#include "sync_platform.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <synchapi.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

#include "error.h"

/* macOS 无 pthread_spinlock_t，与 Windows 一样使用 C11 原子 CAS 自旋。 */
#if defined(__APPLE__) && defined(__MACH__)
#define AIRY_SPINLOCK_CAS 1
#elif defined(_WIN32)
#define AIRY_SPINLOCK_CAS 1
#else
#define AIRY_SPINLOCK_CAS 0
#endif

/* POSIX 绝对超时时刻的唯一构造点：Linux/其它 POSIX 的 timed 原语共用。
 * Windows 无 timespec、macOS 无 timed 原语（走轮询），故不参与编译。 */
#if !defined(_WIN32) && !(defined(__APPLE__) && defined(__MACH__))
static void make_deadline(uint32_t timeout_ms, struct timespec *ts)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_t)(timeout_ms / 1000);
    ts->tv_nsec += (long)((timeout_ms % 1000) * 1000000L);
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}
#endif

/* 平台互斥量初始化的唯一入口：非递归与递归两种形态由 recursive 选择，
 * 使 sync 家族各具锁无需触碰底层原语。 */
int platform_mtx_init(platform_mutex_t *mutex, bool recursive)
{
#ifdef _WIN32
    (void)recursive;
    InitializeCriticalSection(mutex);
    return 0;
#else
    if (!recursive) {
        return pthread_mutex_init(mutex, NULL);
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int ret = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret;
#endif
}

int platform_mutex_init(platform_mutex_t *mutex)
{
    return platform_mtx_init(mutex, false);
}

int platform_mutex_destroy(platform_mutex_t *mutex)
{
#ifdef _WIN32
    DeleteCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_destroy(mutex);
#endif
}

int platform_mutex_lock(platform_mutex_t *mutex)
{
#ifdef _WIN32
    EnterCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_lock(mutex);
#endif
}

int platform_mutex_unlock(platform_mutex_t *mutex)
{
#ifdef _WIN32
    LeaveCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_unlock(mutex);
#endif
}

int platform_mutex_trylock(platform_mutex_t *mutex)
{
#ifdef _WIN32
    return TryEnterCriticalSection(mutex) ? 0 : EBUSY;
#else
    return pthread_mutex_trylock(mutex);
#endif
}

/* Windows 无 pthread_mutex_timedlock：GetTickCount 轮询 TryEnterCriticalSection；
 * macOS 同样无 timed 版本，以 trylock + 1ms 短眠轮询近似；Linux 走原生
 * pthread_mutex_timedlock。超时统一返回 ETIMEDOUT。 */
int platform_mtx_timed(platform_mutex_t *mutex, uint32_t timeout_ms)
{
#ifdef _WIN32
    DWORD start_tick = GetTickCount();
    while (!TryEnterCriticalSection(mutex)) {
        if (GetTickCount() - start_tick >= (DWORD)timeout_ms) {
            return ETIMEDOUT;
        }
        Sleep(1);
    }
    return 0;
#elif defined(__APPLE__) && defined(__MACH__)
    int rc = EBUSY;
    int64_t remaining_ms = (int64_t)timeout_ms;
    while (rc == EBUSY && remaining_ms-- > 0) {
        rc = pthread_mutex_trylock(mutex);
        if (rc == EBUSY) {
            struct timespec nap = {0, 1000000L};
            nanosleep(&nap, NULL);
        }
    }
    return (rc == EBUSY) ? ETIMEDOUT : rc;
#else
    struct timespec ts;
    make_deadline(timeout_ms, &ts);
    return pthread_mutex_timedlock(mutex, &ts);
#endif
}

int platform_rwlock_init(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    InitializeSRWLock(rwlock);
    return 0;
#else
    return pthread_rwlock_init(rwlock, NULL);
#endif
}

int platform_rwlock_destroy(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    (void)rwlock;
    return 0;
#else
    return pthread_rwlock_destroy(rwlock);
#endif
}

int platform_rwlock_rdlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    AcquireSRWLockShared(rwlock);
    return 0;
#else
    return pthread_rwlock_rdlock(rwlock);
#endif
}

int platform_rwlock_wrlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    AcquireSRWLockExclusive(rwlock);
    return 0;
#else
    return pthread_rwlock_wrlock(rwlock);
#endif
}

int platform_rwlock_tryrdlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    return TryAcquireSRWLockShared(rwlock) ? 0 : -1;
#else
    return pthread_rwlock_tryrdlock(rwlock);
#endif
}

int platform_rwlock_trywrlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    return TryAcquireSRWLockExclusive(rwlock) ? 0 : -1;
#else
    return pthread_rwlock_trywrlock(rwlock);
#endif
}

/* read（write=false）与 write（write=true）两条 timed 路径共用的唯一实现。
 * Windows SRWLock 无 timed 等待，沿用既有语义仅尝试一次；macOS 无 timed
 * 原语，try + 1ms 短眠轮询近似；Linux 走 pthread_rwlock_timed{rd,wr}lock。
 * 超时统一返回 ETIMEDOUT。 */
int platform_rw_timed(platform_rwlock_t *rwlock, bool write, uint32_t timeout_ms)
{
#ifdef _WIN32
    (void)timeout_ms;
    BOOL ok = write ? TryAcquireSRWLockExclusive(rwlock) : TryAcquireSRWLockShared(rwlock);
    return ok ? 0 : ETIMEDOUT;
#elif defined(__APPLE__) && defined(__MACH__)
    int rc = EBUSY;
    int64_t remaining_ms = (int64_t)timeout_ms;
    while (rc == EBUSY && remaining_ms-- > 0) {
        rc = write ? pthread_rwlock_trywrlock(rwlock) : pthread_rwlock_tryrdlock(rwlock);
        if (rc == EBUSY) {
            struct timespec nap = {0, 1000000L};
            nanosleep(&nap, NULL);
        }
    }
    return (rc == EBUSY) ? ETIMEDOUT : rc;
#else
    struct timespec ts;
    make_deadline(timeout_ms, &ts);
    return write ? pthread_rwlock_timedwrlock(rwlock, &ts)
                 : pthread_rwlock_timedrdlock(rwlock, &ts);
#endif
}

int platform_rwlock_unlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    void *state = *(void **)rwlock;
    if (state && ((uintptr_t)state & 0x1) == 0) {
        ReleaseSRWLockShared(rwlock);
    } else {
        ReleaseSRWLockExclusive(rwlock);
    }
    return 0;
#else
    return pthread_rwlock_unlock(rwlock);
#endif
}

int platform_spinlock_init(platform_spinlock_t *spinlock)
{
#if AIRY_SPINLOCK_CAS
    atomic_init(spinlock, 0);
    return 0;
#else
    return pthread_spin_init(spinlock, PTHREAD_PROCESS_PRIVATE);
#endif
}

int platform_spinlock_destroy(platform_spinlock_t *spinlock)
{
#if AIRY_SPINLOCK_CAS
    atomic_store(spinlock, 0);
    return 0;
#else
    return pthread_spin_destroy(spinlock);
#endif
}

int platform_spinlock_lock(platform_spinlock_t *spinlock)
{
#if AIRY_SPINLOCK_CAS && defined(_WIN32)
    int expected = 0;
    while (!atomic_compare_exchange_strong_explicit(spinlock, &expected, 1, memory_order_acquire,
                                                    memory_order_relaxed)) {
        expected = 0;
        SwitchToThread();
    }
    return 0;
#elif AIRY_SPINLOCK_CAS
    int expected = 0;
    while (!atomic_compare_exchange_strong_explicit(spinlock, &expected, 1, memory_order_acquire,
                                                    memory_order_relaxed)) {
        expected = 0;
        sched_yield();
    }
    return 0;
#else
    return pthread_spin_lock(spinlock);
#endif
}

int platform_spinlock_unlock(platform_spinlock_t *spinlock)
{
#if AIRY_SPINLOCK_CAS
    atomic_store_explicit(spinlock, 0, memory_order_release);
    return 0;
#else
    return pthread_spin_unlock(spinlock);
#endif
}

int platform_semaphore_init(platform_semaphore_t *semaphore, unsigned int value)
{
#ifdef _WIN32
    *semaphore = CreateSemaphore(NULL, (LONG)value, (LONG)0x7FFFFFFF, NULL);
    return (*semaphore != NULL) ? 0 : -1;
#else
    return sem_init(semaphore, 0, value);
#endif
}

int platform_semaphore_destroy(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    CloseHandle(*semaphore);
    return 0;
#else
    return sem_destroy(semaphore);
#endif
}

int platform_semaphore_wait(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    return (WaitForSingleObject(*semaphore, INFINITE) == WAIT_OBJECT_0) ? 0 : -1;
#else
    return (sem_wait(semaphore) == 0) ? 0 : -1;
#endif
}

int platform_semaphore_post(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    return ReleaseSemaphore(*semaphore, 1, NULL) ? 0 : -1;
#else
    return (sem_post(semaphore) == 0) ? 0 : -1;
#endif
}

int platform_semaphore_timedwait(platform_semaphore_t *semaphore, uint32_t timeout_ms)
{
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(*semaphore, timeout_ms);
    return (ret == WAIT_OBJECT_0) ? 0 : -1;
#elif defined(__APPLE__) && defined(__MACH__)
    /* macOS 无 sem_timedwait：sem_trywait 轮询 + 短眠近似超时，
     * 返回语义与 Linux 分支一致（0=获得信号量，-1=超时/失败） */
    const uint32_t step_ms = 2;
    uint32_t waited = 0;
    while (waited < timeout_ms) {
        if (sem_trywait(semaphore) == 0) {
            return 0;
        }
        if (errno != EAGAIN) {
            return -1;
        }
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = step_ms * 1000000L;
        nanosleep(&ts, NULL);
        waited += step_ms;
    }
    return -1;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return (sem_timedwait(semaphore, &ts) == 0) ? 0 : -1;
#endif
}

int platform_semaphore_trywait(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(*semaphore, 0);
    return (ret == WAIT_OBJECT_0) ? 0 : -1;
#else
    return (sem_trywait(semaphore) == 0) ? 0 : -1;
#endif
}

int platform_condition_init(platform_condition_t *cond)
{
#ifdef _WIN32
    InitializeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_init(cond, NULL);
#endif
}

int platform_condition_destroy(platform_condition_t *cond)
{
#ifdef _WIN32
    (void)cond;
    return 0;
#else
    return pthread_cond_destroy(cond);
#endif
}

int platform_condition_wait(platform_condition_t *cond, platform_mutex_t *mutex)
{
#ifdef _WIN32
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
#else
    return pthread_cond_wait(cond, mutex);
#endif
}

int platform_condition_timedwait(platform_condition_t *cond, platform_mutex_t *mutex,
                                 uint32_t timeout_ms)
{
#ifdef _WIN32
    return SleepConditionVariableCS(cond, mutex, timeout_ms) ? 0 : -1;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return pthread_cond_timedwait(cond, mutex, &ts);
#endif
}

int platform_condition_signal(platform_condition_t *cond)
{
#ifdef _WIN32
    WakeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_signal(cond);
#endif
}

int platform_condition_broadcast(platform_condition_t *cond)
{
#ifdef _WIN32
    WakeAllConditionVariable(cond);
    return 0;
#else
    return pthread_cond_broadcast(cond);
#endif
}

int platform_barrier_init(platform_barrier_t *barrier, unsigned int count)
{
#ifdef _WIN32
    InitializeCriticalSection(&barrier->cs);
    InitializeConditionVariable(&barrier->cond);
    barrier->count = count;
    barrier->current = 0;
    barrier->generation = 0;
    return 0;
#elif defined(__APPLE__) && defined(__MACH__)
    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->cond, NULL);
    barrier->count = count;
    barrier->current = 0;
    barrier->generation = 0;
    return 0;
#else
    return pthread_barrier_init(barrier, NULL, count);
#endif
}

int platform_barrier_destroy(platform_barrier_t *barrier)
{
#ifdef _WIN32
    DeleteCriticalSection(&barrier->cs);
    return 0;
#elif defined(__APPLE__) && defined(__MACH__)
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    return 0;
#else
    return pthread_barrier_destroy(barrier);
#endif
}

int platform_barrier_wait(platform_barrier_t *barrier)
{
#ifdef _WIN32
    EnterCriticalSection(&barrier->cs);
    unsigned int gen = barrier->generation;
    barrier->current++;
    if (barrier->current >= barrier->count) {
        barrier->current = 0;
        barrier->generation++;
        WakeAllConditionVariable(&barrier->cond);
        LeaveCriticalSection(&barrier->cs);
        return 1;
    }
    while (gen == barrier->generation) {
        SleepConditionVariableCS(&barrier->cond, &barrier->cs, INFINITE);
    }
    LeaveCriticalSection(&barrier->cs);
    return 0;
#elif defined(__APPLE__) && defined(__MACH__)
    /* 语义与 _WIN32 分支一致：1 = 最后一个到达线程（serial thread）。 */
    pthread_mutex_lock(&barrier->mutex);
    unsigned int gen = barrier->generation;
    barrier->current++;
    if (barrier->current >= barrier->count) {
        barrier->current = 0;
        barrier->generation++;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;
    }
    while (gen == barrier->generation) {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
    return 0;
#else
    int ret = pthread_barrier_wait(barrier);
    return (ret == PTHREAD_BARRIER_SERIAL_THREAD) ? 1 : (ret == 0 ? 0 : -1);
#endif
}

uint64_t platform_get_timestamp_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

uint64_t platform_get_thread_id(void)
{
#ifdef _WIN32
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}
