// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file sync_compat.h
 * @brief 统一线程同步原语模块 - 向后兼容层
 *
 * 提供与POSIX线程和Windows线程API兼容的接口，便于现有代码逐步迁移到统一同步原语模块。
 * 包含跨平台同步原语包装器和迁移辅助宏。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_SYNC_COMPAT_H
#define AGENTRT_SYNC_COMPAT_H

#include "sync.h"

#include <limits.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sync_compat_api 同步兼容API
 * @{
 */

/* ==================== 互斥锁兼容API ==================== */

#ifndef AGENTRT_PLATFORM_H  /* 避免与 platform.h 中的 agentrt_mutex_init 等冲突 */

/**
 * @brief 创建互斥锁（兼容pthread_mutex_init）
 */
static inline int agentrt_mutex_init(sync_mutex_t *mutex, const void *attrs)
{
    (void)attrs;
    return sync_mutex_create(mutex, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁互斥锁（兼容pthread_mutex_destroy）
 */
static inline int agentrt_mutex_destroy(sync_mutex_t *mutex)
{
    return sync_mutex_free(*mutex) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 锁定互斥锁（兼容pthread_mutex_lock）
 */
static inline int agentrt_mutex_lock(sync_mutex_t *mutex)
{
    return sync_mutex_lock_ex(*mutex, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试锁定互斥锁（兼容pthread_mutex_trylock）
 */
static inline int agentrt_mutex_trylock(sync_mutex_t *mutex)
{
    return sync_mutex_try_lock(*mutex) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 解锁互斥锁（兼容pthread_mutex_unlock）
 */
static inline int agentrt_mutex_unlock(sync_mutex_t *mutex)
{
    return sync_mutex_unlock_ex(*mutex) == SYNC_SUCCESS ? 0 : -1;
}

/* ==================== 条件变量兼容API ==================== */

/**
 * @brief 创建条件变量（兼容pthread_cond_init）
 */
static inline int agentrt_cond_init(sync_condition_t *cond, const void *attrs)
{
    (void)attrs;
    return sync_condition_create(cond, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁条件变量（兼容pthread_cond_destroy）
 */
static inline int agentrt_cond_destroy(sync_condition_t *cond)
{
    return sync_condition_free(*cond) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 等待条件变量（兼容pthread_cond_wait）
 */
static inline int agentrt_cond_wait(sync_condition_t *cond, sync_mutex_t *mutex)
{
    return sync_condition_wait_ex(*cond, *mutex, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 限时等待条件变量（兼容pthread_cond_timedwait）
 */
static inline int agentrt_cond_timedwait(sync_condition_t *cond, sync_mutex_t *mutex, int timeout_ms)
{
    sync_timeout_t timeout;
    timeout.absolute = false;
    timeout.timeout_ms = (uint64_t)timeout_ms;
    return sync_condition_wait_ex(*cond, *mutex, &timeout) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 通知一个等待线程（兼容pthread_cond_signal）
 */
static inline int agentrt_cond_signal(sync_condition_t *cond)
{
    return sync_condition_signal_ex(*cond) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 通知所有等待线程（兼容pthread_cond_broadcast）
 */
static inline int agentrt_cond_broadcast(sync_condition_t *cond)
{
    return sync_condition_broadcast_ex(*cond) == SYNC_SUCCESS ? 0 : -1;
}

/* ==================== 读写锁兼容API ==================== */

/**
 * @brief 创建读写锁（兼容pthread_rwlock_init）
 */
static inline int agentrt_rwlock_init(sync_rwlock_t *rwlock, const void *attrs)
{
    (void)attrs;
    return sync_rwlock_create(rwlock, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁读写锁（兼容pthread_rwlock_destroy）
 */
static inline int agentrt_rwlock_destroy(sync_rwlock_t *rwlock)
{
    return sync_rwlock_free(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 获取读锁（兼容pthread_rwlock_rdlock）
 */
static inline int agentrt_rwlock_rdlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_read_lock_ex(*rwlock, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试获取读锁（兼容pthread_rwlock_tryrdlock）
 */
static inline int agentrt_rwlock_tryrdlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_try_read_lock(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 获取写锁（兼容pthread_rwlock_wrlock）
 */
static inline int agentrt_rwlock_wrlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_write_lock_ex(*rwlock, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试获取写锁（兼容pthread_rwlock_trywrlock）
 */
static inline int agentrt_rwlock_trywrlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_try_write_lock(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 解锁读写锁（兼容pthread_rwlock_unlock）
 */
static inline int agentrt_rwlock_unlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_unlock_ex(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/* ==================== 信号量兼容API ==================== */

/**
 * @brief 创建信号量（兼容sem_init）
 */
static inline int agentrt_sem_init(sync_semaphore_t *sem, int pshared, unsigned int value)
{
    (void)pshared;
    return sync_semaphore_create(sem, value, UINT_MAX, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁信号量（兼容sem_destroy）
 */
static inline int agentrt_sem_destroy(sync_semaphore_t *sem)
{
    return sync_semaphore_free(*sem) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 等待信号量（兼容sem_wait）
 */
static inline int agentrt_sem_wait(sync_semaphore_t *sem)
{
    return sync_semaphore_wait_ex(*sem, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试等待信号量（兼容sem_trywait）
 */
static inline int agentrt_sem_trywait(sync_semaphore_t *sem)
{
    return sync_semaphore_try_wait(*sem) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 限时等待信号量（兼容sem_timedwait）
 */
static inline int agentrt_sem_timedwait(sync_semaphore_t *sem, int timeout_ms)
{
    sync_timeout_t timeout;
    timeout.absolute = false;
    timeout.timeout_ms = (uint64_t)timeout_ms;
    return sync_semaphore_wait_ex(*sem, &timeout) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 发布信号量（兼容sem_post）
 */
static inline int agentrt_sem_post(sync_semaphore_t *sem)
{
    return sync_semaphore_post_ex(*sem) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 获取信号量值（兼容sem_getvalue）
 */
static inline int agentrt_sem_getvalue(sync_semaphore_t *sem, int *value)
{
    unsigned int uval = 0;
    sync_result_t r = sync_semaphore_get_value(*sem, &uval);
    if (value) *value = (int)uval;
    return r == SYNC_SUCCESS ? 0 : -1;
}

/* ==================== 兼容性宏定义 ==================== */

/**
 * @def AGENTRT_MUTEX_INIT(mutex, attrs)
 * @brief 安全互斥锁初始化宏
 */
#define AGENTRT_MUTEX_INIT(mutex, attrs) agentrt_mutex_init(mutex, attrs)

/**
 * @def AGENTRT_MUTEX_DESTROY(mutex)
 * @brief 安全互斥锁销毁宏
 */
#define AGENTRT_MUTEX_DESTROY(mutex) agentrt_mutex_destroy(mutex)

/**
 * @def AGENTRT_MUTEX_LOCK(mutex)
 * @brief 安全互斥锁锁定宏
 */
#define AGENTRT_MUTEX_LOCK(mutex) agentrt_mutex_lock(mutex)

/**
 * @def AGENTRT_MUTEX_TRYLOCK(mutex)
 * @brief 安全互斥锁尝试锁定宏
 */
#define AGENTRT_MUTEX_TRYLOCK(mutex) agentrt_mutex_trylock(mutex)

/**
 * @def AGENTRT_MUTEX_UNLOCK(mutex)
 * @brief 安全互斥锁解锁宏
 */
#define AGENTRT_MUTEX_UNLOCK(mutex) agentrt_mutex_unlock(mutex)

/**
 * @def AGENTRT_COND_INIT(cond, attrs)
 * @brief 安全条件变量初始化宏
 */
#define AGENTRT_COND_INIT(cond, attrs) agentrt_cond_init(cond, attrs)

/**
 * @def AGENTRT_COND_DESTROY(cond)
 * @brief 安全条件变量销毁宏
 */
#define AGENTRT_COND_DESTROY(cond) agentrt_cond_destroy(cond)

/**
 * @def AGENTRT_COND_WAIT(cond, mutex)
 * @brief 安全条件变量等待宏
 */
#define AGENTRT_COND_WAIT(cond, mutex) agentrt_cond_wait(cond, mutex)

/**
 * @def AGENTRT_COND_TIMEDWAIT(cond, mutex, timeout_ms)
 * @brief 安全条件变量限时等待宏
 */
#define AGENTRT_COND_TIMEDWAIT(cond, mutex, timeout_ms) \
    agentrt_cond_timedwait(cond, mutex, timeout_ms)

/**
 * @def AGENTRT_COND_SIGNAL(cond)
 * @brief 安全条件变量通知一个线程宏
 */
#define AGENTRT_COND_SIGNAL(cond) agentrt_cond_signal(cond)

/**
 * @def AGENTRT_COND_BROADCAST(cond)
 * @brief 安全条件变量通知所有线程宏
 */
#define AGENTRT_COND_BROADCAST(cond) agentrt_cond_broadcast(cond)

/**
 * @def AGENTRT_RWLOCK_INIT(rwlock, attrs)
 * @brief 安全读写锁初始化宏
 */
#define AGENTRT_RWLOCK_INIT(rwlock, attrs) agentrt_rwlock_init(rwlock, attrs)

/**
 * @def AGENTRT_RWLOCK_DESTROY(rwlock)
 * @brief 安全读写锁销毁宏
 */
#define AGENTRT_RWLOCK_DESTROY(rwlock) agentrt_rwlock_destroy(rwlock)

/**
 * @def AGENTRT_RWLOCK_RDLOCK(rwlock)
 * @brief 安全读锁获取宏
 */
#define AGENTRT_RWLOCK_RDLOCK(rwlock) agentrt_rwlock_rdlock(rwlock)

/**
 * @def AGENTRT_RWLOCK_TRYRDLOCK(rwlock)
 * @brief 安全读锁尝试获取宏
 */
#define AGENTRT_RWLOCK_TRYRDLOCK(rwlock) agentrt_rwlock_tryrdlock(rwlock)

/**
 * @def AGENTRT_RWLOCK_WRLOCK(rwlock)
 * @brief 安全写锁获取宏
 */
#define AGENTRT_RWLOCK_WRLOCK(rwlock) agentrt_rwlock_wrlock(rwlock)

/**
 * @def AGENTRT_RWLOCK_TRYWRLOCK(rwlock)
 * @brief 安全写锁尝试获取宏
 */
#define AGENTRT_RWLOCK_TRYWRLOCK(rwlock) agentrt_rwlock_trywrlock(rwlock)

/**
 * @def AGENTRT_RWLOCK_UNLOCK(rwlock)
 * @brief 安全读写锁解锁宏
 */
#define AGENTRT_RWLOCK_UNLOCK(rwlock) agentrt_rwlock_unlock(rwlock)

/**
 * @def AGENTRT_SEM_INIT(sem, pshared, value)
 * @brief 安全信号量初始化宏
 */
#define AGENTRT_SEM_INIT(sem, pshared, value) agentrt_sem_init(sem, pshared, value)

/**
 * @def AGENTRT_SEM_DESTROY(sem)
 * @brief 安全信号量销毁宏
 */
#define AGENTRT_SEM_DESTROY(sem) agentrt_sem_destroy(sem)

/**
 * @def AGENTRT_SEM_WAIT(sem)
 * @brief 安全信号量等待宏
 */
#define AGENTRT_SEM_WAIT(sem) agentrt_sem_wait(sem)

/**
 * @def AGENTRT_SEM_TRYWAIT(sem)
 * @brief 安全信号量尝试等待宏
 */
#define AGENTRT_SEM_TRYWAIT(sem) agentrt_sem_trywait(sem)

/**
 * @def AGENTRT_SEM_TIMEDWAIT(sem, timeout_ms)
 * @brief 安全信号量限时等待宏
 */
#define AGENTRT_SEM_TIMEDWAIT(sem, timeout_ms) agentrt_sem_timedwait(sem, timeout_ms)

/**
 * @def AGENTRT_SEM_POST(sem)
 * @brief 安全信号量发布宏
 */
#define AGENTRT_SEM_POST(sem) agentrt_sem_post(sem)

/**
 * @def AGENTRT_SEM_GETVALUE(sem, value)
 * @brief 安全信号量获取值宏
 */
#define AGENTRT_SEM_GETVALUE(sem, value) agentrt_sem_getvalue(sem, value)

#endif /* AGENTRT_PLATFORM_H */

/* ==================== 兼容性宏定义（始终可用） ====================
 * 注意：这些宏始终可用，即使 platform.h 已包含。
 * 当 platform.h 未包含时，宏调用上面的 sync_compat inline 包装函数。
 * 当 platform.h 已包含时，宏调用 platform.h 中的 agentrt_* 函数。
 */
#ifndef AGENTRT_PLATFORM_H

#define AGENTRT_MUTEX_INIT(mutex, attrs) agentrt_mutex_init(mutex, attrs)
#define AGENTRT_MUTEX_DESTROY(mutex) agentrt_mutex_destroy(mutex)
#define AGENTRT_MUTEX_LOCK(mutex) agentrt_mutex_lock(mutex)
#define AGENTRT_MUTEX_TRYLOCK(mutex) agentrt_mutex_trylock(mutex)
#define AGENTRT_MUTEX_UNLOCK(mutex) agentrt_mutex_unlock(mutex)

#define AGENTRT_COND_INIT(cond, attrs) agentrt_cond_init(cond, attrs)
#define AGENTRT_COND_DESTROY(cond) agentrt_cond_destroy(cond)
#define AGENTRT_COND_WAIT(cond, mutex) agentrt_cond_wait(cond, mutex)
#define AGENTRT_COND_TIMEDWAIT(cond, mutex, timeout_ms) \
    agentrt_cond_timedwait(cond, mutex, timeout_ms)
#define AGENTRT_COND_SIGNAL(cond) agentrt_cond_signal(cond)
#define AGENTRT_COND_BROADCAST(cond) agentrt_cond_broadcast(cond)

#define AGENTRT_RWLOCK_INIT(rwlock, attrs) agentrt_rwlock_init(rwlock, attrs)
#define AGENTRT_RWLOCK_DESTROY(rwlock) agentrt_rwlock_destroy(rwlock)
#define AGENTRT_RWLOCK_RDLOCK(rwlock) agentrt_rwlock_rdlock(rwlock)
#define AGENTRT_RWLOCK_TRYRDLOCK(rwlock) agentrt_rwlock_tryrdlock(rwlock)
#define AGENTRT_RWLOCK_WRLOCK(rwlock) agentrt_rwlock_wrlock(rwlock)
#define AGENTRT_RWLOCK_TRYWRLOCK(rwlock) agentrt_rwlock_trywrlock(rwlock)
#define AGENTRT_RWLOCK_UNLOCK(rwlock) agentrt_rwlock_unlock(rwlock)

#define AGENTRT_SEM_INIT(sem, pshared, value) agentrt_sem_init(sem, pshared, value)
#define AGENTRT_SEM_DESTROY(sem) agentrt_sem_destroy(sem)
#define AGENTRT_SEM_WAIT(sem) agentrt_sem_wait(sem)
#define AGENTRT_SEM_TRYWAIT(sem) agentrt_sem_trywait(sem)
#define AGENTRT_SEM_TIMEDWAIT(sem, timeout_ms) agentrt_sem_timedwait(sem, timeout_ms)
#define AGENTRT_SEM_POST(sem) agentrt_sem_post(sem)
#define AGENTRT_SEM_GETVALUE(sem, value) agentrt_sem_getvalue(sem, value)

#else /* AGENTRT_PLATFORM_H defined */

#define AGENTRT_MUTEX_INIT(mutex, attrs) agentrt_mutex_init(mutex)
#define AGENTRT_MUTEX_DESTROY(mutex) agentrt_mutex_destroy(mutex)
#define AGENTRT_MUTEX_LOCK(mutex) agentrt_mutex_lock(mutex)
#define AGENTRT_MUTEX_TRYLOCK(mutex) agentrt_mutex_trylock(mutex)
#define AGENTRT_MUTEX_UNLOCK(mutex) agentrt_mutex_unlock(mutex)

#define AGENTRT_COND_INIT(cond, attrs) agentrt_cond_init(cond)
#define AGENTRT_COND_DESTROY(cond) agentrt_cond_destroy(cond)
#define AGENTRT_COND_WAIT(cond, mutex) agentrt_cond_wait(cond, mutex)
#define AGENTRT_COND_TIMEDWAIT(cond, mutex, timeout_ms) \
    agentrt_cond_timedwait(cond, mutex, timeout_ms)
#define AGENTRT_COND_SIGNAL(cond) agentrt_cond_signal(cond)
#define AGENTRT_COND_BROADCAST(cond) agentrt_cond_broadcast(cond)

#define AGENTRT_RWLOCK_INIT(rwlock, attrs) ((void)(rwlock), (void)(attrs), 0)
#define AGENTRT_RWLOCK_DESTROY(rwlock) ((void)(rwlock))
#define AGENTRT_RWLOCK_RDLOCK(rwlock) ((void)(rwlock))
#define AGENTRT_RWLOCK_TRYRDLOCK(rwlock) ((void)(rwlock))
#define AGENTRT_RWLOCK_WRLOCK(rwlock) ((void)(rwlock))
#define AGENTRT_RWLOCK_TRYWRLOCK(rwlock) ((void)(rwlock))
#define AGENTRT_RWLOCK_UNLOCK(rwlock) ((void)(rwlock))

#define AGENTRT_SEM_INIT(sem, pshared, value) ((void)(sem), (void)(pshared), (void)(value), 0)
#define AGENTRT_SEM_DESTROY(sem) ((void)(sem))
#define AGENTRT_SEM_WAIT(sem) ((void)(sem))
#define AGENTRT_SEM_TRYWAIT(sem) ((void)(sem))
#define AGENTRT_SEM_TIMEDWAIT(sem, timeout_ms) ((void)(sem), (void)(timeout_ms))
#define AGENTRT_SEM_POST(sem) ((void)(sem))
#define AGENTRT_SEM_GETVALUE(sem, value) ((void)(sem), (void)(value))

#endif /* AGENTRT_PLATFORM_H */

/* ==================== P0.18.3: AGENTRT_MUTEX_LOCK_GUARD RAII 自动解锁 ==================== */

/**
 * @defgroup mutex_guard RAII 互斥锁守卫（P0.18.3）
 * @{
 *
 * AGENTRT_MUTEX_LOCK_GUARD 将 mutex 加锁 + 作用域退出自动解锁二合一，
 * 消除手工 lock/unlock 配对样板（POSIX pthread 或 sync 抽象均适用）。
 *
 * 基于 GCC/Clang 的 __attribute__((cleanup)) 实现 RAII 语义。
 * guard 跟踪锁状态，仅当加锁成功时才在 cleanup 中解锁，避免对未锁定的
 * 互斥锁调用 unlock。MSVC 回退到仅加锁（需手动解锁）。
 *
 * 用法：
 *   AGENTRT_MUTEX_LOCK_GUARD(m);   // 加锁，函数返回时自动解锁
 *   // 临界区操作...
 *
 * 注意：宏不检查加锁是否成功。若加锁可能失败（如死锁检测），请使用
 * AGENTRT_MUTEX_LOCK + 手动 if 检查 + AGENTRT_MUTEX_UNLOCK。
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief 互斥锁守卫类型 — 跟踪锁状态
 *
 * 保存互斥锁指针和加锁状态，用于 cleanup 时判断是否需要解锁。
 *
 * 类型兼容：当 platform.h 被引入（AGENTRT_PLATFORM_H 定义）时，mutex 字段
 * 使用 platform.h 的 agentrt_mutex_t（值类型：Linux=pthread_mutex_t，
 * Windows=CRITICAL_SECTION）；否则使用 sync 库的 sync_mutex_t（指针类型）。
 * 两种模式下 agentrt_mutex_lock/unlock 均可用，cleanup 逻辑无需区分。
 */
#ifdef AGENTRT_PLATFORM_H
typedef struct {
    agentrt_mutex_t *mutex;   /**< platform.h 模式：指向 agentrt_mutex_t 变量 */
    bool             acquired; /**< 是否已成功获取锁 */
} agentrt_mutex_guard_t;
#else
typedef struct {
    sync_mutex_t *mutex;   /**< sync 模式：指向 sync_mutex_t 变量 */
    bool          acquired; /**< 是否已成功获取锁 */
} agentrt_mutex_guard_t;
#endif

/**
 * @brief 互斥锁守卫 cleanup 函数（由 cleanup 属性自动调用）
 *
 * 当 AGENTRT_MUTEX_LOCK_GUARD 标记的变量离开作用域时自动调用。
 * 仅当 acquired 为 true 且 mutex 非空时才解锁，避免对未锁定的互斥锁调用 unlock。
 *
 * @param g 指向 guard 变量的指针
 */
static inline void agentrt_mutex_guard_cleanup(agentrt_mutex_guard_t *g)
{
    if (g->acquired && g->mutex) {
        agentrt_mutex_unlock(g->mutex);
        g->acquired = false;
        g->mutex = NULL;
    }
}

/**
 * @def AGENTRT_MUTEX_LOCK_GUARD(m)
 * @brief RAII 互斥锁守卫：加锁 + 作用域退出自动解锁
 *
 * @param m 互斥锁变量（agentrt_mutex_t 或 sync_mutex_t 类型，宏内部取地址 &m）
 *
 * 使用示例（platform.h 模式，引入 platform.h 时）：
 *   static agentrt_mutex_t my_lock;
 *   agentrt_mutex_init(&my_lock);
 *   {
 *       AGENTRT_MUTEX_LOCK_GUARD(my_lock);
 *       // 临界区操作...
 *   }  // 离开作用域时自动 agentrt_mutex_unlock(&my_lock)
 *
 * 使用示例（sync 模式，未引入 platform.h 时）：
 *   static sync_mutex_t my_lock = NULL;
 *   agentrt_mutex_init(&my_lock, NULL);
 *   {
 *       AGENTRT_MUTEX_LOCK_GUARD(my_lock);
 *       // 临界区操作...
 *   }
 *
 * @note 使用 __COUNTER__ 生成唯一变量名，同一作用域可多次使用
 * @note 加锁失败时 acquired=false，cleanup 不会解锁；后续代码在未加锁状态下执行
 */
#define AGENTRT_MUTEX_LOCK_GUARD_(m, counter) \
    agentrt_mutex_guard_t __attribute__((cleanup(agentrt_mutex_guard_cleanup))) \
    __guard_##counter = { .mutex = &(m), .acquired = (agentrt_mutex_lock(&(m)) == 0) }

/* 两层间接：强制 __COUNTER__ 在 ## 拼接前展开为数字，避免变量名冲突。
 * 直接 AGENTRT_MUTEX_LOCK_GUARD_(m, __COUNTER__) 会拼接成 __guard___COUNTER__
 * （字面值，不展开），导致同作用域多次使用时变量名冲突。 */
#define AGENTRT_MUTEX_LOCK_GUARD_EXPAND(m, counter) AGENTRT_MUTEX_LOCK_GUARD_(m, counter)
#define AGENTRT_MUTEX_LOCK_GUARD(m) AGENTRT_MUTEX_LOCK_GUARD_EXPAND(m, __COUNTER__)

#elif defined(_MSC_VER)

/**
 * @def AGENTRT_MUTEX_LOCK_GUARD(m)
 * @brief RAII 互斥锁守卫（MSVC — 回退到仅加锁，需手动解锁）
 *
 * MSVC 不支持 __attribute__((cleanup))，此宏仅执行加锁。
 * 使用 MSVC 时需在函数返回前手动调用 AGENTRT_MUTEX_UNLOCK。
 */
#define AGENTRT_MUTEX_LOCK_GUARD(m) ((void)agentrt_mutex_lock(&(m)))

#else

/**
 * @def AGENTRT_MUTEX_LOCK_GUARD(m)
 * @brief RAII 互斥锁守卫（未知编译器 — 回退到仅加锁，需手动解锁）
 */
#define AGENTRT_MUTEX_LOCK_GUARD(m) ((void)agentrt_mutex_lock(&(m)))

#endif

/** @} */  // end of mutex_guard

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_SYNC_COMPAT_H */