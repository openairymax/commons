// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_sync.c
 * @brief 统一线程同步原语模块单元测试
 *
 * 测试同步模块的基本功能：互斥锁、条件变量、信号量、读写锁等。
 * 注意：本测试主要测试单线程功能，多线程并发测试需要更复杂的测试框架。
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../include/sync.h"
#include "include/airy_memory.h"
#include "string_compat.h"

#include <assert.h>
#include <string.h>

/**
 * @brief 测试互斥锁基本功能
 * @return 成功返回0，失败返回值
 */
static int test_mutex_basic(void)
{
    printf("测试互斥锁基本功：%..\n");

    sync_mutex_t mutex = NULL;

    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁创建失败，错误码：%d\n", result);
        return 1;
    }
    if (mutex == NULL) {
        printf("  错误：互斥锁句柄为NULL\n");
        return 1;
    }

    result = sync_mutex_lock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁加锁失败，错误码：%d\n", result);
        sync_mutex_destroy(mutex);
        return 1;
    }

    // 测试尝试加锁（应该失败，因为已经锁定）
    result = sync_mutex_trylock(mutex);
    if (result == SYNC_SUCCESS) {
        printf("  错误：已锁定的互斥锁尝试加锁应该失败\n");
        sync_mutex_unlock(mutex);
        sync_mutex_destroy(mutex);
        return 1;
    }

    result = sync_mutex_unlock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁解锁失败，错误码：%d\n", result);
        sync_mutex_destroy(mutex);
        return 1;
    }

    result = sync_mutex_trylock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：解锁后的互斥锁尝试加锁失败，错误码：%d\n", result);
        sync_mutex_destroy(mutex);
        return 1;
    }

    result = sync_mutex_unlock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁再次解锁失败，错误码：%d\n", result);
        sync_mutex_destroy(mutex);
        return 1;
    }

    result = sync_mutex_destroy(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁销毁失败，错误码：%d\n", result);
        return 1;
    }

    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试递归互斥锁
 * @return 成功返回0，失败返回值
 */
static int test_recursive_mutex(void)
{
    printf("测试递归互斥：%..\n");

    sync_recursive_mutex_t mutex = NULL;

    .type = SYNC_TYPE_RECURSIVE_MUTEX, .flags = SYNC_FLAG_RECURSIVE, .name = "test_recursive_mutex",
    .context = NULL
};

sync_result_t result = sync_recursive_mutex_create(&mutex, &attr);
if (result != SYNC_SUCCESS) {
    printf("  错误：递归互斥锁创建失败，错误码：%d\n", result);
    return 1;
}

for (int i = 0; i < 3; i++) {
    result = sync_recursive_mutex_lock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：递归互斥锁第 %d 次加锁失败，错误码：%d\n", i + 1, result);
        sync_recursive_mutex_destroy(mutex);
        return 1;
    }
}

for (int i = 0; i < 3; i++) {
    result = sync_recursive_mutex_unlock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：递归互斥锁第 %d 次解锁失败，错误码：%d\n", i + 1, result);
        sync_recursive_mutex_destroy(mutex);
        return 1;
    }
}

if (result != SYNC_SUCCESS) {
    printf("  错误：递归互斥锁销毁失败，错误码：%d\n", result);
    return 1;
}

printf("  通过\n");
return 0;
}

/**
 * @brief 测试读写锁基本功能
 * @return 成功返回0，失败返回值
 */
static int test_rwlock_basic(void)
{
    printf("测试读写锁基本功：%..\n");

    sync_rwlock_t rwlock = NULL;

    if (result != SYNC_SUCCESS) {
        printf("  错误：读写锁创建失败，错误码：%d\n", result);
        return 1;
    }

    result = sync_rwlock_rdlock(rwlock);
    if (result != SYNC_SUCCESS) {
        printf("  错误：读锁加锁失败，错误码：%d\n", result);
        sync_rwlock_destroy(rwlock);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：第二个读锁加锁失败，错误码：%d\n", result);
        sync_rwlock_unlock(rwlock);
        sync_rwlock_destroy(rwlock);
        return 1;
    }

    result = sync_rwlock_unlock(rwlock);
    if (result != SYNC_SUCCESS) {
        printf("  错误：第一个读锁解锁失败，错误码：%d\n", result);
        sync_rwlock_destroy(rwlock);
        return 1;
    }

    result = sync_rwlock_unlock(rwlock);
    if (result != SYNC_SUCCESS) {
        printf("  错误：第二个读锁解锁失败，错误码：%d\n", result);
        sync_rwlock_destroy(rwlock);
        return 1;
    }

    result = sync_rwlock_wrlock(rwlock);
    if (result != SYNC_SUCCESS) {
        printf("  错误：写锁加锁失败，错误码：%d\n", result);
        sync_rwlock_destroy(rwlock);
        return 1;
    }

    // 尝试获取读锁（应该失败，因为写锁独占）
    result = sync_rwlock_tryrdlock(rwlock);
    if (result == SYNC_SUCCESS) {
        printf("  错误：写锁持有期间读锁应该失败\n");
        sync_rwlock_unlock(rwlock);
        sync_rwlock_destroy(rwlock);
        return 1;
    }

    result = sync_rwlock_unlock(rwlock);
    if (result != SYNC_SUCCESS) {
        printf("  错误：写锁解锁失败，错误码：%d\n", result);
        sync_rwlock_destroy(rwlock);
        return 1;
    }

    result = sync_rwlock_destroy(rwlock);
    if (result != SYNC_SUCCESS) {
        printf("  错误：读写锁销毁失败，错误码：%d\n", result);
        return 1;
    }

    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试信号量基本功能
 *
 * @return 成功返回0，失败返回值
 */
static int test_semaphore_basic(void)
{
    printf("测试信号量基本功：%..\n");

    sync_semaphore_t sem = NULL;

    if (result != SYNC_SUCCESS) {
        printf("  错误：信号量创建失败，错误码：%d\n", result);
        return 1;
    }

    result = sync_semaphore_getvalue(sem, &value);
    if (result != SYNC_SUCCESS) {
        printf("  错误：获取信号量值失败，错误码：%d\n", result);
        sync_semaphore_destroy(sem);
        return 1;
    }
    if (value != 3) {
        printf("  错误：信号量初始值不正确：%d（期望值）\n", value);
        sync_semaphore_destroy(sem);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：信号量等待失败，错误码：%d\n", result);
        sync_semaphore_destroy(sem);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：获取信号量值失败，错误码：%d\n", result);
        sync_semaphore_destroy(sem);
        return 1;
    }
    if (value != 2) {
        printf("  错误：信号量值不正确：%d（期望值）\n", value);
        sync_semaphore_destroy(sem);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：信号量发布失败，错误码：%d\n", result);
        sync_semaphore_destroy(sem);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：获取信号量值失败，错误码：%d\n", result);
        sync_semaphore_destroy(sem);
        return 1;
    }
    if (value != 3) {
        printf("  错误：信号量值不正确：%d（期望值）\n", value);
        sync_semaphore_destroy(sem);
        return 1;
    }

    result = sync_semaphore_trywait(sem);
    if (result != SYNC_SUCCESS) {
        printf("  错误：信号量尝试等待失败，错误码：%d\n", result);
        sync_semaphore_destroy(sem);
        return 1;
    }

    result = sync_semaphore_destroy(sem);
    if (result != SYNC_SUCCESS) {
        printf("  错误：信号量销毁失败，错误码：%d\n", result);
        return 1;
    }

    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试条件变量基本功能
 *
 * @return 成功返回0，失败返回值
 */
static int test_condition_basic(void)
{
    printf("测试条件变量基本功能...\n");

    sync_condition_t cond = NULL;
    sync_mutex_t mutex = NULL;

    sync_result_t result = sync_condition_create(&cond, NULL);
    if (result != SYNC_SUCCESS) {
        printf("  错误：条件变量创建失败，错误码：%d\n", result);
        return 1;
    }

    result = sync_mutex_create(&mutex, NULL);
    if (result != SYNC_SUCCESS) {
        printf("  错误：关联互斥锁创建失败，错误码：%d\n", result);
        sync_condition_destroy(cond);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁加锁失败，错误码：%d\n", result);
        sync_mutex_destroy(mutex);
        sync_condition_destroy(cond);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：条件变量信号失败，错误码：%d\n", result);
        sync_mutex_unlock(mutex);
        sync_mutex_destroy(mutex);
        sync_condition_destroy(cond);
        return 1;
    }

    result = sync_condition_broadcast(cond);
    if (result != SYNC_SUCCESS) {
        printf("  错误：条件变量广播失败，错误码：%d\n", result);
        sync_mutex_unlock(mutex);
        sync_mutex_destroy(mutex);
        sync_condition_destroy(cond);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁解锁失败，错误码：%d\n", result);
        sync_mutex_destroy(mutex);
        sync_condition_destroy(cond);
        return 1;
    }

    if (result != SYNC_SUCCESS) {
        printf("  错误：条件变量销毁失败，错误码：%d\n", result);
        sync_mutex_destroy(mutex);
        return 1;
    }

    result = sync_mutex_destroy(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：互斥锁销毁失败，错误码：%d\n", result);
        return 1;
    }

    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试锁统计功能
 *
 * @return 成功返回0，失败返回值
 */
static int test_lock_stats(void)
{
    printf("测试锁统计功：%..\n");

    sync_mutex_t mutex = NULL;

    .type = SYNC_TYPE_MUTEX, .flags = SYNC_FLAG_NONE, .name = "test_mutex_with_stats",
    .context = NULL
};

sync_result_t result = sync_mutex_create(&mutex, &attr);
if (result != SYNC_SUCCESS) {
    printf("  错误：带统计的互斥锁创建失败，错误码：%d\n", result);
    return 1;
}

sync_stats_t stats = {0};
result = sync_mutex_get_stats(mutex, &stats);
if (result != SYNC_SUCCESS) {
    printf("  错误：获取锁统计信息失败，错误码：%d\n", result);
    sync_mutex_destroy(mutex);
    return 1;
}

if (stats.lock_count != 0 || stats.unlock_count != 0) {
    printf("  错误：初始统计信息不正确\n");
    sync_mutex_destroy(mutex);
    return 1;
}

for (int i = 0; i < 5; i++) {
    result = sync_mutex_lock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：第 %d 次加锁失败，错误码：%d\n", i + 1, result);
        sync_mutex_destroy(mutex);
        return 1;
    }

    result = sync_mutex_unlock(mutex);
    if (result != SYNC_SUCCESS) {
        printf("  错误：第 %d 次解锁失败，错误码：%d\n", i + 1, result);
        sync_mutex_destroy(mutex);
        return 1;
    }
}

result = sync_mutex_get_stats(mutex, &stats);
if (result != SYNC_SUCCESS) {
    printf("  错误：再次获取锁统计信息失败，错误码：%d\n", result);
    sync_mutex_destroy(mutex);
    return 1;
}

printf("  错误：统计信息不正确：lock_count=%zu, unlock_count=%zu\n", stats.lock_count,
       stats.unlock_count);
sync_mutex_destroy(mutex);
return 1;
}

result = sync_mutex_reset_stats(mutex);
if (result != SYNC_SUCCESS) {
    printf("  错误：重置统计信息失败，错误码：%d\n", result);
    sync_mutex_destroy(mutex);
    return 1;
}

if (result != SYNC_SUCCESS) {
    printf("  错误：重置后获取锁统计信息失败，错误码：%d\n", result);
    sync_mutex_destroy(mutex);
    return 1;
}

if (stats.lock_count != 0 || stats.unlock_count != 0) {
    printf("  错误：重置后统计信息不正确\n");
    sync_mutex_destroy(mutex);
    return 1;
}

result = sync_mutex_destroy(mutex);
if (result != SYNC_SUCCESS) {
    printf("  错误：互斥锁销毁失败，错误码：%d\n", result);
    return 1;
}

printf("  通过\n");
return 0;
}

/**
 * @brief 主测试函数
 * @return 成功返回0，失败返回值
 */
int main(void)
{
    printf("开始统一同步模块单元测试\n");
    printf("========================\n");

    int total_failures = 0;

    total_failures += test_recursive_mutex();
    total_failures += test_rwlock_basic();
    total_failures += test_semaphore_basic();
    total_failures += test_condition_basic();
    total_failures += test_lock_stats();

    printf("========================\n");
    if (total_failures == 0) {
        printf("所有测试通过！\n");
        return 0;
    } else {
        printf("测试失败：%d 个测试未通过\n", total_failures);
        return 1;
    }
}