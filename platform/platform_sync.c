// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_sync.c
 * @brief Sync primitives domain: cross-platform thread create/join/detach,
 * mutex and condition variable implementations.
 */

/* pthread_setname_np（Linux）需 _GNU_SOURCE；必须在任何系统头之前定义 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#define strdup _strdup
#define access _access /* flawfinder: ignore */
#ifndef EEXIST
#define EEXIST 17
#endif
#pragma comment(lib, "bcrypt.lib")
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "error.h"
#include "platform.h"
#include "cancel_token.h"

#include "airy_memory.h"

#if AIRY_PLATFORM_WINDOWS

int airy_platform_thread_create(airy_thread_t *thread, airy_thread_func_t func, void *arg)
{
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    if (h == NULL) {
        return (int)GetLastError();
    }
    *thread = h;
    return 0;
}

int airy_platform_thread_join(airy_thread_t thread, void **retval)
{
    (void)retval;
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return AIRY_EINVAL;
    }
    CloseHandle(thread);
    return 0;
}

int airy_platform_thread_detach(airy_thread_t thread)
{
    /* Windows has no pthread_detach equivalent: closing the thread handle
     * drops our reference; the thread keeps running and its resources are
     * reclaimed by the system when it exits. After detach the thread must
     * not be joined. */
    if (thread != NULL) {
        CloseHandle(thread);
    }
    return 0;
}

#else

int airy_platform_thread_create(airy_thread_t *thread, airy_thread_func_t func, void *arg)
{
    return pthread_create(thread, NULL, func, arg);
}

int airy_platform_thread_join(airy_thread_t thread, void **retval)
{
    return pthread_join(thread, retval);
}

int airy_platform_thread_detach(airy_thread_t thread)
{
    return pthread_detach(thread);
}

#endif

int airy_thread_set_name(const char *name)
{
    if (!name)
        return AIRY_EINVAL;
#if AIRY_PLATFORM_WINDOWS
    int wlen = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
    if (wlen <= 0)
        return AIRY_EINVAL;
    wchar_t *wname = (wchar_t *)AIRY_MALLOC((size_t)wlen * sizeof(wchar_t));
    if (!wname)
        return AIRY_ENOMEM;
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, wlen);
    HRESULT hr = SetThreadDescription(GetCurrentThread(), wname);
    AIRY_FREE(wname);
    return SUCCEEDED(hr) ? 0 : AIRY_EINVAL;
#elif defined(__linux__)
    /* Linux 内核线程名上限 15 字节（含 NUL），超长截断 */
    char tmp[16];
    AIRY_STRNCPY_TERM(tmp, name, sizeof(tmp));
    return pthread_setname_np(pthread_self(), tmp) == 0 ? 0 : AIRY_EINVAL;
#else
    /* macOS */
    return pthread_setname_np(name) == 0 ? 0 : AIRY_EINVAL;
#endif
}

#if AIRY_PLATFORM_WINDOWS

int airy_mtx_init(airy_mtx_t *mutex)
{
    InitializeCriticalSection(mutex);
    return 0;
}

int airy_mtx_lock(airy_mtx_t *mutex)
{
    EnterCriticalSection(mutex);
    return 0;
}

int airy_mtx_trylock(airy_mtx_t *mutex)
{
    return TryEnterCriticalSection(mutex) ? 0 : -1;
}

int airy_mtx_unlock(airy_mtx_t *mutex)
{
    LeaveCriticalSection(mutex);
    return 0;
}

void airy_mtx_destroy(airy_mtx_t *mutex)
{
    DeleteCriticalSection(mutex);
}

airy_mtx_t *airy_mtx_create(void)
{
    airy_mtx_t *mutex = (airy_mtx_t *)AIRY_MALLOC(sizeof(airy_mtx_t));
    if (mutex) {
        InitializeCriticalSection(mutex);
    }
    return mutex;
}

void airy_mtx_free(airy_mtx_t *mutex)
{
    if (mutex) {
        DeleteCriticalSection(mutex);
        AIRY_FREE(mutex);
    }
}

#else

int airy_mtx_init(airy_mtx_t *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int ret = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret;
}

int airy_mtx_lock(airy_mtx_t *mutex)
{
    return pthread_mutex_lock(mutex);
}

int airy_mtx_trylock(airy_mtx_t *mutex)
{
    return pthread_mutex_trylock(mutex);
}

int airy_mtx_unlock(airy_mtx_t *mutex)
{
    return pthread_mutex_unlock(mutex);
}

void airy_mtx_destroy(airy_mtx_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

airy_mtx_t *airy_mtx_create(void)
{
    airy_mtx_t *mutex = (airy_mtx_t *)AIRY_MALLOC(sizeof(airy_mtx_t));
    if (mutex) {
        pthread_mutex_init(mutex, NULL);
    }
    return mutex;
}

void airy_mtx_free(airy_mtx_t *mutex)
{
    if (mutex) {
        pthread_mutex_destroy(mutex);
        AIRY_FREE(mutex);
    }
}

#endif

#if AIRY_PLATFORM_WINDOWS

int airy_cond_init(airy_cond_t *cond)
{
    InitializeConditionVariable(cond);
    return 0;
}

int airy_cond_wait(airy_cond_t *cond, airy_mtx_t *mutex)
{
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int airy_cond_timedwait(airy_cond_t *cond, airy_mtx_t *mutex, uint32_t timeout_ms)
{
    BOOL result = SleepConditionVariableCS(cond, mutex, timeout_ms);
    if (!result) {
        DWORD err = GetLastError();
        if (err == ERROR_TIMEOUT) {
            return AIRY_ERR_TIMEOUT;
        }
        return AIRY_EINVAL;
    }
    return 0;
}

int airy_cond_signal(airy_cond_t *cond)
{
    WakeConditionVariable(cond);
    return 0;
}

int airy_cond_broadcast(airy_cond_t *cond)
{
    WakeAllConditionVariable(cond);
    return 0;
}

void airy_cond_destroy(airy_cond_t *cond)
{
    (void)cond;
}

airy_cond_t *airy_cond_create(void)
{
    airy_cond_t *cond = (airy_cond_t *)AIRY_MALLOC(sizeof(airy_cond_t));
    if (cond) {
        InitializeConditionVariable(cond);
    }
    return cond;
}

void airy_cond_free(airy_cond_t *cond)
{
    if (cond) {
        AIRY_FREE(cond);
    }
}

#else

int airy_cond_init(airy_cond_t *cond)
{
    return pthread_cond_init(cond, NULL);
}

int airy_cond_wait(airy_cond_t *cond, airy_mtx_t *mutex)
{
    return pthread_cond_wait(cond, mutex);
}

int airy_cond_timedwait(airy_cond_t *cond, airy_mtx_t *mutex, uint32_t timeout_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    if (ret == ETIMEDOUT) {
        return AIRY_ERR_TIMEOUT;
    }
    return ret;
}

int airy_cond_signal(airy_cond_t *cond)
{
    return pthread_cond_signal(cond);
}

int airy_cond_broadcast(airy_cond_t *cond)
{
    return pthread_cond_broadcast(cond);
}

void airy_cond_destroy(airy_cond_t *cond)
{
    pthread_cond_destroy(cond);
}

airy_cond_t *airy_cond_create(void)
{
    airy_cond_t *cond = (airy_cond_t *)AIRY_MALLOC(sizeof(airy_cond_t));
    if (cond) {
        pthread_cond_init(cond, NULL);
    }
    return cond;
}

void airy_cond_free(airy_cond_t *cond)
{
    if (cond) {
        pthread_cond_destroy(cond);
        AIRY_FREE(cond);
    }
}

#endif
