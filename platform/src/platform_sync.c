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

/* CreateThread 回调必须是 __stdcall（LPTHREAD_START_ROUTINE），而
 * airy_thread_func_t 为默认 __cdecl；x86 上直接强转会调用约定不匹配
 * 导致栈不平衡。经参数块转发：param 指向 {func, arg, ...}。
 *
 * POSIX 语义中 pthread_join 可回填线程入口返回值（retval）；CreateThread
 * 仅提供 DWORD 退出码，无法承载 void* 返回值。此处以 registry 记录
 * 每个存活线程的上下文：入口返回后先写 ctx->result 再置 state=done，
 * join 等待句柄就绪后即可读取并释放；detach 通过 state 状态机协调由
 * 线程自释或 detach 侧释放，保证上下文恰好释放一次。 */
typedef struct airy_thread_win_ctx {
    airy_thread_func_t func;
    void *arg;
    void *result;
    HANDLE h;
    /* 0=running, 1=done（入口已返回）, 2=detach 已请求（线程退出时自释）,
     * 3=已释放 */
    volatile LONG state;
    struct airy_thread_win_ctx *next;
} airy_thread_win_ctx_t;

static SRWLOCK g_airy_thread_win_guard = SRWLOCK_INIT;
static airy_thread_win_ctx_t *g_airy_thread_win_list = NULL;

static void airy_thread_win_register(airy_thread_win_ctx_t *c)
{
    AcquireSRWLockExclusive(&g_airy_thread_win_guard);
    c->next = g_airy_thread_win_list;
    g_airy_thread_win_list = c;
    ReleaseSRWLockExclusive(&g_airy_thread_win_guard);
}

static airy_thread_win_ctx_t *airy_thread_win_unlink(HANDLE thread)
{
    airy_thread_win_ctx_t *c = NULL;
    AcquireSRWLockExclusive(&g_airy_thread_win_guard);
    airy_thread_win_ctx_t **pp = &g_airy_thread_win_list;
    while (*pp != NULL) {
        if ((*pp)->h == thread) {
            c = *pp;
            *pp = c->next;
            break;
        }
        pp = &(*pp)->next;
    }
    ReleaseSRWLockExclusive(&g_airy_thread_win_guard);
    return c;
}

static DWORD WINAPI airy_thread_start_routine(LPVOID param)
{
    airy_thread_win_ctx_t *ctx = (airy_thread_win_ctx_t *)param;
    ctx->result = ctx->func(ctx->arg);
    /* 先记 done 再释放：join 只有在句柄 signaled（入口已返回）后才会读
     * result，此时 state 必然 >= 1；detach 若先行请求（prev==2）则由本
     * 线程完成自释。 */
    LONG prev = InterlockedExchange(&ctx->state, 1);
    if (prev == 2) {
        InterlockedExchange(&ctx->state, 3);
        AIRY_FREE(ctx);
    }
    return 0;
}

/* 回收已从 registry 摘出的线程上下文：线程尚存则请其退出时自释
 * （state 0→2），已退出则本侧直接释放（state 1→3）。恰好一次。 */
static void airy_thread_win_reclaim(airy_thread_win_ctx_t *ctx)
{
    for (;;) {
        LONG s = ctx->state;
        if (s == 0) {
            if (InterlockedCompareExchange(&ctx->state, 2, 0) == 0)
                break; /* 线程退出时自释（routine prev==2 分支） */
        } else if (s == 1) {
            if (InterlockedCompareExchange(&ctx->state, 3, 1) == 1) {
                AIRY_FREE(ctx);
                break;
            }
        } else if (s == 3) {
            break;
        } else if (s == 2) {
            break;
        }
    }
}

int airy_platform_thread_create(airy_thread_t *thread, airy_thread_func_t func, void *arg)
{
    airy_thread_win_ctx_t *ctx =
        (airy_thread_win_ctx_t *)AIRY_MALLOC(sizeof(airy_thread_win_ctx_t));
    if (!ctx)
        return AIRY_ENOMEM;
    ctx->func = func;
    ctx->arg = arg;
    ctx->result = NULL;
    ctx->h = NULL;
    ctx->state = 0;
    ctx->next = NULL;
    /* 先入 registry 再 CreateThread：即使线程瞬间跑完，join 也能命中 */
    airy_thread_win_register(ctx);
    HANDLE h = CreateThread(NULL, 0, airy_thread_start_routine, ctx, 0, NULL);
    if (h == NULL) {
        DWORD err = GetLastError();
        AcquireSRWLockExclusive(&g_airy_thread_win_guard);
        airy_thread_win_ctx_t **pp = &g_airy_thread_win_list;
        while (*pp != NULL && *pp != ctx)
            pp = &(*pp)->next;
        if (*pp == ctx)
            *pp = ctx->next;
        ReleaseSRWLockExclusive(&g_airy_thread_win_guard);
        AIRY_FREE(ctx);
        return (int)err;
    }
    ctx->h = h;
    *thread = h;
    return 0;
}

int airy_platform_thread_join(airy_thread_t thread, void **retval)
{
    if (!thread)
        return AIRY_EINVAL;
    airy_thread_win_ctx_t *ctx = airy_thread_win_unlink(thread);
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result != WAIT_OBJECT_0) {
        if (ctx)
            airy_thread_win_reclaim(ctx);
        CloseHandle(thread);
        return AIRY_EINVAL;
    }
    if (retval && ctx)
        *retval = ctx->result;
    if (ctx) {
        InterlockedExchange(&ctx->state, 3);
        AIRY_FREE(ctx);
    }
    CloseHandle(thread);
    return 0;
}

int airy_platform_thread_detach(airy_thread_t thread)
{
    /* Windows has no pthread_detach equivalent: closing the thread handle
     * drops our reference; the thread keeps running and its resources are
     * reclaimed by the system when it exits. After detach the thread must
     * not be joined. 上下文经 state 状态机由运行线程退出时自释（detach 在
     * 线程尚存时置 state=2），或由本函数在线程已退出后直接回收。 */
    airy_thread_win_ctx_t *ctx = airy_thread_win_unlink(thread);
    if (ctx) {
        airy_thread_win_reclaim(ctx);
    }
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

/* CRITICAL_SECTION 零初始化兼容（Windows）。
 *
 * POSIX 端 pthread_mutex_t 全零 == PTHREAD_MUTEX_INITIALIZER（glibc 实证），
 * 既有代码大量以 `static airy_mtx_t lock;` / `= {0}` 声明互斥量后直接
 * airy_mtx_lock()（observability/atomic_logging/taskflow/heapstore 等全树
 * 实证）。Windows 的 CRITICAL_SECTION 必须经 InitializeCriticalSection
 * 才能使用：对全零对象直接 EnterCriticalSection，单线程快路径碰巧可用，
 * 但首个竞争线程进入 RtlpWaitOnCriticalSection 会访问空 LockSemaphore →
 * Access Violation（windows ctest 35 项 SegFault 栈 #127 实证）。
 *
 * 此处以 DebugInfo==NULL 判"未初始化"，用全零即可用的 SRWLOCK
 * （SRWLOCK_INIT 即 {0}，等价 POSIX 零初始化）串行补做一次
 * InitializeCriticalSection，还原 POSIX 端零初始化语义；显式调用
 * airy_mtx_init 的路径不受影响（二次 Ensure 因 DebugInfo 非空直接跳过）。
 * airy_mtx_t 布局不变（仍为 CRITICAL_SECTION），递归语义保持。 */
static SRWLOCK g_airy_mtx_lazy_guard = SRWLOCK_INIT;

static void airy_mtx_ensure_initialized(airy_mtx_t *mutex)
{
    if (mutex && mutex->DebugInfo == NULL) {
        AcquireSRWLockExclusive(&g_airy_mtx_lazy_guard);
        if (mutex->DebugInfo == NULL) {
            InitializeCriticalSection(mutex);
        }
        ReleaseSRWLockExclusive(&g_airy_mtx_lazy_guard);
    }
}

/* POSIX 侧 airy_mtx_init（含零初始化静态锁）= 递归互斥量，而
 * airy_mtx_create = 非递归互斥量（corekern 同步测试契约：已持有锁时
 * trylock 必须失败）。CRITICAL_SECTION 恒为递归，无法原生表达非递归
 * trylock，故对 create() 产出的互斥量登记集合，trylock 时以自属检测
 * （RTL_CRITICAL_SECTION.OwningThread == 本线程 id）模拟非递归语义。 */
typedef struct {
    SRWLOCK guard;
    airy_mtx_t **items;
    size_t count;
    size_t cap;
} airy_mtx_created_set_t;

static airy_mtx_created_set_t g_airy_mtx_created = {SRWLOCK_INIT, NULL, 0, 0};

static void airy_mtx_created_add(airy_mtx_t *m)
{
    AcquireSRWLockExclusive(&g_airy_mtx_created.guard);
    if (g_airy_mtx_created.count == g_airy_mtx_created.cap) {
        size_t ncap = g_airy_mtx_created.cap ? g_airy_mtx_created.cap * 2 : 16;
        airy_mtx_t **ni = (airy_mtx_t **)AIRY_MALLOC(ncap * sizeof(airy_mtx_t *));
        if (!ni) {
            ReleaseSRWLockExclusive(&g_airy_mtx_created.guard);
            return; /* 集合只是优化提示：漏登记时退化为递归 trylock */
        }
        if (g_airy_mtx_created.items) {
            for (size_t i = 0; i < g_airy_mtx_created.count; i++) {
                ni[i] = g_airy_mtx_created.items[i];
            }
            AIRY_FREE(g_airy_mtx_created.items);
        }
        g_airy_mtx_created.items = ni;
        g_airy_mtx_created.cap = ncap;
    }
    g_airy_mtx_created.items[g_airy_mtx_created.count++] = m;
    ReleaseSRWLockExclusive(&g_airy_mtx_created.guard);
}

static void airy_mtx_created_remove(airy_mtx_t *m)
{
    AcquireSRWLockExclusive(&g_airy_mtx_created.guard);
    for (size_t i = 0; i < g_airy_mtx_created.count; i++) {
        if (g_airy_mtx_created.items[i] == m) {
            g_airy_mtx_created.items[i] = g_airy_mtx_created.items[--g_airy_mtx_created.count];
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_airy_mtx_created.guard);
}

static bool airy_mtx_created_has(airy_mtx_t *m)
{
    bool found = false;
    AcquireSRWLockShared(&g_airy_mtx_created.guard);
    for (size_t i = 0; i < g_airy_mtx_created.count; i++) {
        if (g_airy_mtx_created.items[i] == m) {
            found = true;
            break;
        }
    }
    ReleaseSRWLockShared(&g_airy_mtx_created.guard);
    return found;
}

int airy_mtx_init(airy_mtx_t *mutex)
{
    InitializeCriticalSection(mutex);
    return 0;
}

int airy_mtx_lock(airy_mtx_t *mutex)
{
    airy_mtx_ensure_initialized(mutex);
    EnterCriticalSection(mutex);
    return 0;
}

int airy_mtx_trylock(airy_mtx_t *mutex)
{
    if (!mutex)
        return -1;
    airy_mtx_ensure_initialized(mutex);
    if (airy_mtx_created_has(mutex)) {
        /* create() 路径为非递归互斥量：本线程已持锁时 trylock 必须失败，
         * 与 POSIX pthread_mutex_trylock 非递归语义对齐。 */
        if (mutex->OwningThread == (HANDLE)(uintptr_t)GetCurrentThreadId()) {
            return -1;
        }
    }
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
        airy_mtx_created_add(mutex);
    }
    return mutex;
}

void airy_mtx_free(airy_mtx_t *mutex)
{
    if (mutex) {
        DeleteCriticalSection(mutex);
        airy_mtx_created_remove(mutex);
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
        /* 非递归互斥量（与 airy_mtx_init 的递归属性为有意差异）：
         * corekern 同步测试契约依赖 create 路径的非递归语义
         * （已持有锁时 trylock 必须失败）。 */
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
    airy_mtx_ensure_initialized(mutex);
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int airy_cond_timedwait(airy_cond_t *cond, airy_mtx_t *mutex, uint32_t timeout_ms)
{
    airy_mtx_ensure_initialized(mutex);
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
