/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file atomic_logging.h
 * @brief 统一分层日志系统原子层API
 *
 * @details
 * 本模块提供统一的分层日志系统原子层接口，专注于：
 * - 高性能：无锁队列、零拷贝、批量提交
 * - 线程安全：多生产者单消费者、内存屏障、原子操作
 * - 低延迟：异步刷新、线程本地缓冲、最小化系统调用
 *
 * 原子层设计原则：
 * 1. **无锁设计**：使用CAS（Compare-And-Swap）操作避免锁竞争
 * 2. **零拷贝**：日志格式化直接写入目标缓冲，避免内存复制
 * 3. **批量提交**：多个日志记录批量提交，减少系统调用开销
 * 4. **分离关注点**：写入线程与刷新线程分离，避免I/O阻塞
 *
 * 架构角色：
 * - 接收来自核心层的日志记录
 * - 提供线程安全的缓冲和队列管理
 * - 将格式化后的日志传递给服务层输出
 *
 * 注意：原子层是内部实现细节，大多数用户应该使用核心层API。
 * 只有需要极致性能或特殊线程安全需求的组件才应直接使用原子层。
 */

#ifndef AIRY_RT_COMMON_ATOMIC_LOGGING_H
#define AIRY_RT_COMMON_ATOMIC_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "logging.h"

#include <stdbool.h>


#include "atomic_compat.h"
#define HAVE_STDATOMIC 1


/**
 * @brief 原子层配置结构体
 *
 * 配置原子层的行为，优化性能和资源使用。
 */
typedef struct {

    bool lock_free_mode;


    size_t thread_local_buffer_size;


    size_t ring_buffer_capacity;


    size_t batch_commit_threshold;


    size_t max_batch_size;


    uint32_t flush_thread_sleep_ms;


    bool enable_memory_pool;


    size_t memory_pool_initial_size;


    size_t memory_pool_block_size;
} atomic_logging_config_t;


/**
 * @brief 原子日志记录节点
 *
 * 环形队列中的节点，包含一条日志记录和必要的元数据。
 * 设计为缓存行对齐（通常64字节），避免伪共享。
 */
typedef struct _AtomicLogRecordNode {

    log_record_t record;


    _Atomic uint32_t state;


    _Atomic uint64_t sequence;


    uint8_t _padding[1];
} AtomicLogRecordNode;

/**
 * @brief 无锁环形缓冲队列
 *
 * 多生产者单消费者（MPSC）无锁环形队列。
 * 生产者可以并发写入，消费者顺序读取。
 */
typedef struct {

    AtomicLogRecordNode *nodes;


    size_t capacity;


    _Atomic size_t head;


    _Atomic size_t tail;


    _Atomic size_t barrier;
} LockFreeRingBuffer;

/**
 * @brief 线程本地缓冲
 *
 * 每个线程独立的缓冲，减少全局队列竞争。
 * 当本地缓冲满时，批量提交到全局队列。
 */
typedef struct {

    log_record_t *buffer;


    size_t capacity;


    size_t position;


    uint64_t thread_id;
} ThreadLocalBuffer;


/**
 * @brief 初始化原子层
 *
 * 初始化原子层内部数据结构，包括环形队列、内存池等。
 * 必须在使用任何原子层函数之前调用。
 *
 * @param manager 原子层配置，为NULL时使用默认配置
 * @return 0 成功，负值表示错误
 */
int atomic_logging_init(const atomic_logging_config_t *manager);

/**
 * @brief 提交日志记录到原子层（无锁版本）
 *
 * 将日志记录提交到无锁环形队列，支持多线程并发调用。
 * 如果队列已满，函数将阻塞直到有空间可用（除非配置了非阻塞模式）。
 *
 * @param record 日志记录
 * @param non_blocking 是否非阻塞，true时队列满则立即返回失败
 * @return 0 成功，负值表示错误
 */
int atomic_logging_submit_lockfree(const log_record_t *record, bool non_blocking);

/**
 * @brief 提交日志记录到原子层（互斥锁版本）
 *
 * 将日志记录提交到带互斥锁保护的队列。
 * 用于不支持无锁操作的环境或调试目的。
 *
 * @param record 日志记录
 * @return 0 成功，负值表示错误
 */
int atomic_logging_submit_mutex(const log_record_t *record);

/**
 * @brief 批量提交日志记录
 *
 * 批量提交多个日志记录到原子层，减少函数调用开销。
 *
 * @param records 日志记录数组
 * @param count 记录数量
 * @return 成功提交的记录数，负值表示错误
 */
int atomic_logging_submit_batch(const log_record_t *records, size_t count);

/**
 * @brief 从原子层获取日志记录
 *
 * 从原子层获取下一条可用的日志记录。
 * 通常由服务层的刷新线程调用。
 *
 * @param record 输出参数，接收日志记录
 * @param timeout_ms 超时时间（毫秒），0表示不阻塞，-1表示无限等待
 * @return 0 成功，1 队列为空，负值表示错误
 */
int atomic_logging_acquire(log_record_t *record, int timeout_ms);

/**
 * @brief 批量获取日志记录
 *
 * 从原子层批量获取多个日志记录。
 *
 * @param records 输出数组，接收日志记录
 * @param max_count 最大获取数量
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际获取的记录数，负值表示错误
 */
int atomic_logging_acquire_batch(log_record_t *records, size_t max_count, int timeout_ms);


/**
 * @brief 原子层统计信息
 *
 * 原子层的运行时性能统计信息。
 */
typedef struct atomic_logging_stats {
    uint64_t total_submitted;
    uint64_t total_acquired;
    size_t current_queue_size;
    float queue_max_usage;
    uint64_t submit_avg_latency_ns;
    uint64_t acquire_avg_latency_ns;
    uint64_t submit_collisions;
    uint64_t memory_pool_allocations;
    uint64_t memory_pool_frees;
    size_t thread_local_buffers;
    uint64_t batch_submits;
    uint64_t batch_acquires;
} atomic_logging_stats_t;

/**
 * @brief 获取原子层统计信息
 *
 * 获取原子层的运行时统计信息，用于监控和调试。
 *
 * @param out_stats 输出参数，接收统计信息
 * @return 0 成功，负值表示错误
 */
int atomic_logging_get_stats(atomic_logging_stats_t *out_stats);

/**
 * @brief 获取线程本地缓冲
 *
 * 获取或创建当前线程的本地缓冲。
 * 如果线程第一次调用，将创建新的本地缓冲。
 *
 * @return 线程本地缓冲指针，失败返回NULL
 */
ThreadLocalBuffer *atomic_logging_get_thread_local_buffer(void);

/**
 * @brief 提交线程本地缓冲
 *
 * 将线程本地缓冲中的所有记录批量提交到全局队列。
 *
 * @param buffer 线程本地缓冲
 * @return 成功提交的记录数，负值表示错误
 */
int atomic_logging_flush_thread_local_buffer(ThreadLocalBuffer *buffer);

/**
 * @brief 刷新原子层
 *
 * 强制刷新所有缓冲的记录，确保所有已提交的记录都可用。
 *
 * @return 0 成功，负值表示错误
 */
int atomic_logging_flush(void);

/**
 * @brief 清理原子层
 *
 * 清理原子层资源，释放所有分配的内存。
 * 必须在程序退出前调用。
 */
void atomic_logging_cleanup(void);


/**
 * @brief 内存屏障：写屏障
 *
 * 确保屏障之前的所有写操作对后续读操作可见。
 * 在写入共享数据后调用。
 */
static inline void atomic_write_barrier(void)
{
    atomic_thread_fence(memory_order_release);
}

/**
 * @brief 内存屏障：读屏障
 *
 * 确保屏障之后的所有读操作能获取到最新数据。
 * 在读取共享数据前调用。
 */
static inline void atomic_read_barrier(void)
{
    atomic_thread_fence(memory_order_acquire);
}

/**
 * @brief 原子比较并交换（CAS）操作
 *
 * 比较内存位置的值与期望值，如果相等则更新为新值。
 *
 * @param ptr 目标内存位置
 * @param expected 期望值指针（输入/输出）
 * @param desired 新值
 * @return true 操作成功，false 操作失败
 */
static inline bool airy_atomic_cas_weak(volatile uint64_t *ptr, uint64_t *expected,
                                        uint64_t desired)
{
    return atomic_compare_exchange_strong_64((volatile _Atomic int64_t *)ptr, (int64_t *)expected,
                                             (int64_t)desired, memory_order_acq_rel,
                                             memory_order_acquire);
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMON_ATOMIC_LOGGING_H */