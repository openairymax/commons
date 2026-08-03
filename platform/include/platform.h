/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file platform.h
 * @brief 跨平台兼容层 - 统一不同操作系统的API差异
 *
 * 支持平台：
 * - Linux (POSIX)
 * - macOS (Darwin)
 * - Windows (Win32/Win64)
 *
 * 设计原则：
 * - 单一职责：仅处理平台差异
 * - 零开销：内联函数 + 宏定义
 * - 类型安全：强类型封装
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-03-30
 * @version 2.0
 *
 * @note 线程安全：平台抽象层本身不涉及线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性原则
 */

#ifndef AIRY_RT_PLATFORM_H
#define AIRY_RT_PLATFORM_H

#include <compat.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* 注意：不在此处包含 <time.h>，因为项目的 corekern/include/time.h 会覆盖系统 time.h */
/* 需要使用 time.h 定义的代码（如 clockid_t, CLOCK_MONOTONIC）应在 .c 文件中 */
/* 在包含 platform.h 之前先包含系统的 <time.h> */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 平台检测 ==================== */
#if defined(_WIN32) || defined(_WIN64)
#define AIRY_PLATFORM_WINDOWS 1
#define AIRY_PLATFORM_NAME "Windows"
#if defined(_WIN64)
#define AIRY_PLATFORM_BITS 64
#else
#define AIRY_PLATFORM_BITS 32
#endif
#define AIRY_PLATFORM_POSIX 0
#elif defined(__APPLE__) && defined(__MACH__)
#define AIRY_PLATFORM_MACOS 1
#define AIRY_PLATFORM_NAME "macOS"
#define AIRY_PLATFORM_BITS 64
#define AIRY_PLATFORM_POSIX 1
#elif defined(__linux__)
#define AIRY_PLATFORM_LINUX 1
#define AIRY_PLATFORM_NAME "Linux"
#if defined(__x86_64__) || defined(__aarch64__)
#define AIRY_PLATFORM_BITS 64
#else
#define AIRY_PLATFORM_BITS 32
#endif
#define AIRY_PLATFORM_POSIX 1
#else
#error "Unsupported platform"
#endif

/* ==================== 导出宏定义 ==================== */
#include "export.h"

/* ==================== 线程局部存储 ==================== */
#if defined(_WIN32) || defined(_WIN64)
#define AIRY_THREAD_LOCAL __declspec(thread)
#else
#define AIRY_THREAD_LOCAL __thread
#endif

/* ==================== 内联函数 ==================== */
#if defined(_WIN32) || defined(_WIN64)
#ifndef AIRY_INLINE
#define AIRY_INLINE __forceinline
#endif
#else
#define AIRY_INLINE static inline __attribute__((always_inline))
#endif

/* ==================== 未使用参数标记 ==================== */
#ifndef AIRY_UNUSED
#define AIRY_UNUSED(x) ((void)(x))
#endif

/* ==================== 路径分隔符 ==================== */
#if AIRY_PLATFORM_WINDOWS
#define AIRY_PATH_SEP '\\'
#define AIRY_PATH_SEP_STR "\\"
#define AIRY_PATH_MAX 260
#else
#define AIRY_PATH_SEP '/'
#define AIRY_PATH_SEP_STR "/"
#define AIRY_PATH_MAX 4096
#endif

/* ==================== 标准路径常量 (BAN-32合规) ==================== */
/* 注意：使用 #ifndef 守卫，允许 CMake target_compile_definitions 覆盖（如 cupolas/channel_d） */
#if AIRY_PLATFORM_WINDOWS
/* Windows: 系统级数据目录（%ProgramData%） */
#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "C:\\ProgramData\\agentrt\\run"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "C:\\ProgramData\\agentrt\\logs"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "C:\\ProgramData\\agentrt\\config"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "C:\\ProgramData\\agentrt\\data"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "C:\\ProgramData\\agentrt\\tmp"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "C:\\ProgramData\\agentrt\\cache"
#endif
#elif AIRY_PLATFORM_LINUX
/* Linux: FHS 标准路径（保持原有行为不变） */
#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "/tmp/agentrt"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "/var/log/agentrt"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "/etc/agentrt"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "/var/lib/agentrt"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "/var/tmp/agentrt"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "/var/cache/agentrt"
#endif
#else
/* macOS 及其他 POSIX: 相对路径（避免硬编码 /var 与 /etc，保持 Linux 行为不变） */
#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "./agentrt/run"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "./agentrt/logs"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "./agentrt/config"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "./agentrt/data"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "./agentrt/tmp"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "./agentrt/cache"
#endif
#endif

/* ==================== 平台头文件包含 ==================== */
#if AIRY_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#endif

/* ==================== 基础类型定义 ==================== */

#if AIRY_PLATFORM_WINDOWS
typedef HANDLE airy_thread_t;
typedef DWORD airy_thread_id_t;
typedef CRITICAL_SECTION airy_mtx_t;
typedef CONDITION_VARIABLE airy_cond_t;
typedef DWORD airy_pid_t;
typedef SOCKET airy_sock_t;
typedef HANDLE airy_process_t;
#else
typedef pthread_t airy_thread_t;
typedef pthread_t airy_thread_id_t;
typedef pthread_mutex_t airy_mtx_t;
typedef pthread_cond_t airy_cond_t;
typedef pid_t airy_pid_t;
typedef int airy_sock_t;
typedef pid_t airy_process_t;
#endif

#define AIRY_INVALID_THREAD ((airy_thread_t)0)
#define AIRY_INVALID_MUTEX ((airy_mtx_t){0})
#define AIRY_INVALID_SOCKET (-1)
#define AIRY_INVALID_PROCESS ((airy_process_t)0)

/* ==================== 互斥锁接口 ==================== */

/**
 * @brief 初始化互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int airy_mtx_init(airy_mtx_t *mutex);

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁指针
 */
void airy_mtx_destroy(airy_mtx_t *mutex);

/**
 * @brief 加锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int airy_mtx_lock(airy_mtx_t *mutex);

/**
 * @brief 尝试加锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败或已锁定
 */
int airy_mtx_trylock(airy_mtx_t *mutex);

/**
 * @brief 解锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int airy_mtx_unlock(airy_mtx_t *mutex);

/**
 * @brief 动态创建互斥锁（分配内存并初始化）
 * @return 互斥锁指针，失败返回NULL
 */
airy_mtx_t *airy_mtx_create(void);

/**
 * @brief 动态销毁互斥锁（销毁并释放内存）
 * @param mutex 互斥锁指针
 */
void airy_mtx_free(airy_mtx_t *mutex);

/* ==================== 条件变量接口 ==================== */

/**
 * @brief 初始化条件变量
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int airy_cond_init(airy_cond_t *cond);

/**
 * @brief 销毁条件变量
 * @param cond 条件变量指针
 */
void airy_cond_destroy(airy_cond_t *cond);

/**
 * @brief 等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int airy_cond_wait(airy_cond_t *cond, airy_mtx_t *mutex);

/**
 * @brief 超时等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，非0 失败或超时
 */
int airy_cond_timedwait(airy_cond_t *cond, airy_mtx_t *mutex, uint32_t timeout_ms);

/**
 * @brief 唤醒一个等待线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int airy_cond_signal(airy_cond_t *cond);

/**
 * @brief 唤醒所有等待线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int airy_cond_broadcast(airy_cond_t *cond);

/**
 * @brief 动态创建条件变量（分配内存并初始化）
 * @return 条件变量指针，失败返回NULL
 */
airy_cond_t *airy_cond_create(void);

/**
 * @brief 动态销毁条件变量（销毁并释放内存）
 * @param cond 条件变量指针
 */
void airy_cond_free(airy_cond_t *cond);

/* ==================== 线程接口 ==================== */

/**
 * @brief 线程函数类型
 */
typedef void *(*airy_thread_func_t)(void *arg);

/**
 * @brief 创建线程
 * @param thread 线程句柄指针
 * @param func 线程函数
 * @param arg 线程参数
 * @return 0 成功，非0 失败
 */
int airy_platform_thread_create(airy_thread_t *thread, airy_thread_func_t func, void *arg);

/**
 * @brief 等待线程结束
 * @param thread 线程句柄
 * @param retval 返回值指针（可为NULL）
 * @return 0 成功，非0 失败
 */
int airy_platform_thread_join(airy_thread_t thread, void **retval);

/**
 * @brief 分离线程（线程结束后自动回收资源）
 * @param thread 线程句柄
 * @return 0 成功，非0 失败
 */
int airy_platform_thread_detach(airy_thread_t thread);

#ifndef AIRY_USE_SCHEDULER_THREAD_IMPL
#define airy_thread_create airy_platform_thread_create
#define airy_thread_join airy_platform_thread_join
#define airy_thread_detach airy_platform_thread_detach
#endif

/**
 * @brief 获取当前线程ID
 * @return 线程ID
 */
uint64_t airy_thread_id(void);

/* ==================== Socket 接口 ==================== */

/**
 * @brief 创建 TCP Socket
 * @return Socket 句柄，失败返回 AIRY_INVALID_SOCKET
 */
airy_sock_t airy_sock_tcp(void);

/**
 * @brief 创建 Unix Domain Socket（仅 POSIX）
 * @return Socket 句柄，失败返回 AIRY_INVALID_SOCKET
 */
airy_sock_t airy_sock_unix(void);

/**
 * @brief 关闭 Socket
 * @param sock Socket 句柄
 */
void airy_sock_close(airy_sock_t sock);

/**
 * @brief 设置 Socket 非阻塞模式
 * @param sock Socket 句柄
 * @param nonblock 是否非阻塞
 * @return 0 成功，非0 失败
 */
int airy_sock_set_nonblock(airy_sock_t sock, int nonblock);

/**
 * @brief 设置 Socket 复用地址
 * @param sock Socket 句柄
 * @param reuse 是否复用
 * @return 0 成功，非0 失败
 */
int airy_sock_set_reuseaddr(airy_sock_t sock, int reuse);

/* ==================== 进程接口 ==================== */

/**
 * @brief 进程信息结构
 */
typedef struct {
    airy_pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
#if AIRY_PLATFORM_WINDOWS
    /* Windows 子进程句柄，存入结构体以支持多进程并发与线程安全，
     * 替代原先的全局变量（单进程限制）。使用 void* 避免在此处引入 windows.h。 */
    void *process_handle; /* HANDLE to the child process */
    void *thread_handle;  /* HANDLE to the child's primary thread */
#endif
} airy_process_info_t;

/**
 * @brief 启动进程
 * @param executable 可执行文件路径
 * @param argv 参数数组（以NULL结尾）
 * @param envp 环境变量数组（以NULL结尾，可为NULL）
 * @param proc 输出进程信息
 * @return 0 成功，非0 失败
 */
int airy_process_start(const char *executable, char *const argv[], char *const envp[],
                          airy_process_info_t *proc);

/**
 * @brief 等待进程结束
 * @param proc 进程信息
 * @param timeout_ms 超时时间（毫秒），0表示无限等待
 * @param exit_code 输出退出码（可为NULL）
 * @return 0 成功，非0 失败或超时
 */
int airy_process_wait(airy_process_info_t *proc, uint32_t timeout_ms, int *exit_code);

/**
 * @brief 终止进程
 * @param proc 进程信息
 * @return 0 成功，非0 失败
 */
int airy_process_kill(airy_process_info_t *proc);

/**
 * @brief 关闭进程管道
 * @param proc 进程信息
 */
void airy_process_close_pipes(airy_process_info_t *proc);

/**
 * @brief 运行命令并捕获合并输出（高层便捷接口）
 *
 * 内部完成 start → 读取 stdout+stderr 合并输出 → wait → close 全流程。
 * POSIX 路径使用 fork + execvp（不经过 shell，无命令注入风险，BAN-211/235 合规）。
 *
 * @param executable 可执行文件路径（execvp 搜索 PATH）
 * @param argv       参数数组（以 NULL 结尾，argv[0] 通常为程序名）
 * @param envp       环境变量数组（以 NULL 结尾，可为 NULL 表示继承）
 * @param timeout_ms 超时（毫秒），0 表示无限等待
 * @param output     输出缓冲区（可为 NULL 表示不捕获，但仍排空管道防止子进程阻塞）
 * @param output_size 输出缓冲区大小（含 '\0'）
 * @return 退出码(0-255)；-1=启动失败；-2=超时
 */
int airy_process_run_capture(const char *executable, char *const argv[],
                                char *const envp[], uint32_t timeout_ms,
                                char *output, size_t output_size);

/* ==================== 时间接口 ==================== */

/**
 * @brief 获取高精度时间戳（纳秒）
 * @return 时间戳
 */
uint64_t airy_time_ns(void);

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 时间戳
 */
uint64_t airy_time_ms(void);

/**
 * @brief 睡眠指定毫秒数
 * @param ms 毫秒数
 */
void airy_sleep_ms(uint32_t ms);

/* ==================== 随机数接口 ==================== */

/**
 * @brief 初始化随机数生成器（线程安全）
 */
void airy_random_init(void);

/**
 * @brief 生成随机数（线程安全）
 * @param min 最小值
 * @param max 最大值
 * @return 随机数
 */
uint32_t airy_random_uint32(uint32_t min, uint32_t max);

/**
 * @brief 生成随机浮点数（线程安全）
 * @return 0.0 到 1.0 之间的随机数
 */
float airy_random_float(void);

/**
 * @brief 生成随机字节（线程安全）
 * @param buf 缓冲区
 * @param len 长度
 * @return 0 成功，非0 失败
 */
int airy_random_bytes(void *buf, size_t len);

/* ==================== 文件系统接口 ==================== */

/**
 * @brief 检查文件是否存在
 * @param path 文件路径
 * @return 1 存在，0 不存在
 */
int airy_file_exists(const char *path);

/**
 * @brief 创建目录（递归）
 * @param path 目录路径
 * @return 0 成功，非0 失败
 */
int airy_mkdir_p(const char *path);

/**
 * @brief 获取文件大小
 * @param path 文件路径
 * @return 文件大小，失败返回 -1
 */
int64_t airy_file_size(const char *path);

/* ==================== 网络初始化接口 ==================== */

/**
 * @brief 初始化网络库（Windows需要）
 * @return 0 成功
 */
int airy_network_init(void);

/**
 * @brief 清理网络库（Windows需要）
 */
void airy_network_cleanup(void);

/* ==================== 信号处理接口 ==================== */

/**
 * @brief 忽略 SIGPIPE 信号
 */
void airy_ignore_sigpipe(void);

/* ==================== 字符串工具 ==================== */

/**
 * @brief 安全的字符串复制
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 0成功，非0失败
 */
int airy_strlcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全的字符串连接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 0成功，非0失败
 */
int airy_strlcat(char *dest, const char *src, size_t dest_size);

/* ==================== 错误处理接口 ==================== */

/**
 * @brief 获取最后错误的错误码
 * @return 错误码
 */
int airy_get_last_error(void);

/**
 * @brief 获取错误描述字符串
 * @param error 错误码
 * @return 错误描述字符串
 */
const char *airy_strerror(int error);

/* ==================== 系统信息类型 (UNI-01: 唯一定义) ==================== */

#ifndef AIRY_SYSINFO_T_DEFINED
#define AIRY_SYSINFO_T_DEFINED
typedef struct {
    char os_name[64];
    char os_version[64];
    char hostname[64];
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} airy_sysinfo_t;
#endif

int airy_get_sysinfo(airy_sysinfo_t *info);

/* ==================== 原子操作类型 (UNI-01: 唯一定义) ==================== */

#ifndef AIRY_ATOMIC_INT_T_DEFINED
#define AIRY_ATOMIC_INT_T_DEFINED
#include "atomic_compat.h"
typedef atomic_int airy_atomic_int_t;
#endif

int airy_atomic_load(airy_atomic_int_t *atomic);
void airy_atomic_store(airy_atomic_int_t *atomic, int value);
int airy_atomic_fetch_add(airy_atomic_int_t *atomic, int value);
int airy_atomic_fetch_sub(airy_atomic_int_t *atomic, int value);

/* ==================== P0.18.3: AIRY_MUTEX_LOCK_GUARD RAII 自动解锁 ==================== */
/* d8 清理：从 sync_compat.h 迁移到 platform.h（RAII 守卫依赖 airy_mtx_lock/unlock，
 * 逻辑上属于 platform.h API 的辅助工具）。消除 sync_compat.h 的兼容层定位。 */

/**
 * @defgroup mutex_guard RAII 互斥锁守卫（P0.18.3）
 * @{
 *
 * AIRY_MUTEX_LOCK_GUARD 将 mutex 加锁 + 作用域退出自动解锁二合一，
 * 消除手工 lock/unlock 配对样板。
 *
 * 基于 GCC/Clang 的 __attribute__((cleanup)) 实现 RAII 语义。
 * guard 跟踪锁状态，仅当加锁成功时才在 cleanup 中解锁，避免对未锁定的
 * 互斥锁调用 unlock。MSVC 回退到仅加锁（需手动解锁）。
 *
 * 用法：
 *   AIRY_MUTEX_LOCK_GUARD(m);   // 加锁，函数返回时自动解锁
 *   // 临界区操作...
 *
 * 注意：宏不检查加锁是否成功。若加锁可能失败（如死锁检测），请使用
 * airy_mtx_lock + 手动 if 检查 + airy_mtx_unlock。
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief 互斥锁守卫类型 — 跟踪锁状态
 *
 * 保存互斥锁指针和加锁状态，用于 cleanup 时判断是否需要解锁。
 */
typedef struct {
    airy_mtx_t *mutex;      /**< 指向 airy_mtx_t 变量 */
    bool        acquired;   /**< 是否已成功获取锁 */
} airy_mtx_guard_t;

/**
 * @brief 互斥锁守卫 cleanup 函数（由 cleanup 属性自动调用）
 *
 * 当 AIRY_MUTEX_LOCK_GUARD 标记的变量离开作用域时自动调用。
 * 仅当 acquired 为 true 且 mutex 非空时才解锁，避免对未锁定的互斥锁调用 unlock。
 *
 * @param g 指向 guard 变量的指针
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
 * @brief RAII 互斥锁守卫：加锁 + 作用域退出自动解锁
 *
 * @param m 互斥锁变量（airy_mtx_t 类型，宏内部取地址 &m）
 *
 * 使用示例：
 *   static airy_mtx_t my_lock;
 *   airy_mtx_init(&my_lock);
 *   {
 *       AIRY_MUTEX_LOCK_GUARD(my_lock);
 *       // 临界区操作...
 *   }  // 离开作用域时自动 airy_mtx_unlock(&my_lock)
 *
 * @note 使用 __COUNTER__ 生成唯一变量名，同一作用域可多次使用
 * @note 加锁失败时 acquired=false，cleanup 不会解锁；后续代码在未加锁状态下执行
 */
#define AIRY_MUTEX_LOCK_GUARD_(m, counter) \
    airy_mtx_guard_t __attribute__((cleanup(airy_mtx_guard_cleanup))) \
    __guard_##counter = { .mutex = &(m), .acquired = (airy_mtx_lock(&(m)) == 0) }

/* 两层间接：强制 __COUNTER__ 在 ## 拼接前展开为数字，避免变量名冲突。
 * 直接 AIRY_MUTEX_LOCK_GUARD_(m, __COUNTER__) 会拼接成 __guard___COUNTER__
 * （字面值，不展开），导致同作用域多次使用时变量名冲突。 */
#define AIRY_MUTEX_LOCK_GUARD_EXPAND(m, counter) AIRY_MUTEX_LOCK_GUARD_(m, counter)
#define AIRY_MUTEX_LOCK_GUARD(m) AIRY_MUTEX_LOCK_GUARD_EXPAND(m, __COUNTER__)

#elif defined(_MSC_VER)

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII 互斥锁守卫（MSVC — 回退到仅加锁，需手动解锁）
 *
 * MSVC 不支持 __attribute__((cleanup))，此宏仅执行加锁。
 * 使用 MSVC 时需在函数返回前手动调用 airy_mtx_unlock。
 */
#define AIRY_MUTEX_LOCK_GUARD(m) ((void)airy_mtx_lock(&(m)))

#else

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII 互斥锁守卫（未知编译器 — 回退到仅加锁，需手动解锁）
 */
#define AIRY_MUTEX_LOCK_GUARD(m) ((void)airy_mtx_lock(&(m)))

#endif

/** @} */  // end of mutex_guard

/* ==================== AIRY_MUTEX_* 兼容宏 ==================== */
/* d8 清理：sync_compat.h 已迁移，但部分代码仍使用 AIRY_MUTEX_* 宏形式。
 * 这里提供与 airy_mtx_* 函数的兼容映射，避免调用方逐一改写。
 * 调用约定：调用方传入指针（如 AIRY_MUTEX_LOCK(&ctx->mutex)），
 * 宏直接转发指针给 airy_mtx_* 函数。
 * 新代码应直接使用 airy_mtx_init/lock/unlock/destroy 函数形式。 */

/**
 * @def AIRY_MUTEX_INIT(m, attr)
 * @brief 初始化互斥锁（兼容宏 — 转发至 airy_mtx_init）
 * @param m airy_mtx_t* 指针
 * @param attr 未使用（保留兼容 pthread_mutex_init 签名）
 * @return 0 成功，非 0 失败
 */
#define AIRY_MUTEX_INIT(m, attr) airy_mtx_init(m)

/**
 * @def AIRY_MUTEX_DESTROY(m)
 * @brief 销毁互斥锁（兼容宏 — 转发至 airy_mtx_destroy）
 * @param m airy_mtx_t* 指针
 */
#define AIRY_MUTEX_DESTROY(m) airy_mtx_destroy(m)

/**
 * @def AIRY_MUTEX_LOCK(m)
 * @brief 加锁（兼容宏 — 转发至 airy_mtx_lock）
 * @param m airy_mtx_t* 指针
 * @return 0 成功，非 0 失败
 */
#define AIRY_MUTEX_LOCK(m) airy_mtx_lock(m)

/**
 * @def AIRY_MUTEX_UNLOCK(m)
 * @brief 解锁（兼容宏 — 转发至 airy_mtx_unlock）
 * @param m airy_mtx_t* 指针
 * @return 0 成功，非 0 失败
 */
#define AIRY_MUTEX_UNLOCK(m) airy_mtx_unlock(m)

/**
 * @def AIRY_MUTEX_TRYLOCK(m)
 * @brief 尝试加锁（兼容宏 — 转发至 airy_mtx_trylock）
 * @param m airy_mtx_t* 指针
 * @return 0 成功，非 0 失败（EBUSY 表示已锁定）
 */
#define AIRY_MUTEX_TRYLOCK(m) airy_mtx_trylock(m)

/* ==================== AIRY_HOME 路径体系（生产就绪） ====================
 *
 * 统一安装根目录：默认 $HOME/.airymaxrt，可用环境变量 AIRY_HOME 覆盖。
 * 全部运行时产物（socket/pid/日志/配置/数据）收敛于其下，非 root 部署、
 * 容器化与卸载均干净，替代散布的 FHS 路径（/tmp、/var/log、/etc...）。
 *
 * 目录布局：
 *   $AIRY_HOME/bin      — 可执行文件（agentrt、agent_d、llm_d、mem_d、airy_cli）
 *   $AIRY_HOME/lib      — 运行时依赖（airymax_agents、openlab、sdk-python）
 *   $AIRY_HOME/run      — Unix socket、PID 文件
 *   $AIRY_HOME/logs     — daemon 日志、审计日志、agent 子进程日志
 *   $AIRY_HOME/config   — agentrt.yaml、model.yaml、secrets.env
 *   $AIRY_HOME/data     — 持久化数据（记忆等）
 *   $AIRY_HOME/tmp      — 临时文件
 *   $AIRY_HOME/cache    — 缓存
 */

#define AIRY_DEFAULT_HOME_DIR ".airymaxrt"
#define AIRY_HOME_SUBDIR_BIN    "bin"
#define AIRY_HOME_SUBDIR_LIB    "lib"
#define AIRY_HOME_SUBDIR_RUN    "run"
#define AIRY_HOME_SUBDIR_LOG    "logs"
#define AIRY_HOME_SUBDIR_CONFIG "config"
#define AIRY_HOME_SUBDIR_DATA   "data"
#define AIRY_HOME_SUBDIR_TMP    "tmp"
#define AIRY_HOME_SUBDIR_CACHE  "cache"

/** @brief AIRY_HOME 根目录（$AIRY_HOME 或 $HOME/.airymaxrt），线程安全 */
const char *airy_home_dir(void);
/** @brief 可执行文件目录，线程安全 */
const char *airy_bin_dir(void);
/** @brief 运行时依赖目录（Python SDK/agents），线程安全 */
const char *airy_lib_dir(void);
/** @brief 运行时目录（socket/pid），线程安全 */
const char *airy_runtime_dir(void);
/** @brief 日志目录，线程安全 */
const char *airy_log_dir(void);
/** @brief 配置目录，线程安全 */
const char *airy_config_dir(void);
/** @brief 数据目录，线程安全 */
const char *airy_data_dir(void);
/** @brief 临时目录，线程安全 */
const char *airy_tmp_dir(void);
/** @brief 缓存目录，线程安全 */
const char *airy_cache_dir(void);

/**
 * @brief 初始化 AIRY_HOME 路径体系
 *
 * 解析各目录路径并创建（mkdir -p），同时向环境变量 setenv
 * AIRY_RUNTIME_DIR/AIRY_LOG_DIR/AIRY_CONFIG_DIR/AIRY_DATA_DIR/
 * AIRY_TMP_DIR/AIRY_CACHE_DIR，使既有 getenv 型消费点立即生效。
 * 各 daemon 在 main 早期（日志初始化前）调用一次。
 *
 * @return AIRY_SUCCESS 成功；AIRY_ERR_SYS_FILE 目录创建失败
 */
int airy_paths_init(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_H */
