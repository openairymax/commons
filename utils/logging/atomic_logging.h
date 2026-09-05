/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file atomic_logging.h
 * @brief Unified layered logging system: atomic-layer API.
 *
 * @details
 * This module provides the unified layered logging system atomic-layer
 * interface, focusing on:
 * - High performance: lock-free queue, zero copy, batch commit
 * - Thread safety: multi-producer single-consumer, memory barriers, atomic
 *   operations
 * - Low latency: async flush, thread-local buffering, minimal syscalls
 *
 * Atomic-layer design principles:
 * 1. Lock-free design: CAS (Compare-And-Swap) operations avoid lock
 *    contention
 * 2. Zero copy: log formatting writes directly into the target buffer,
 *    avoiding memory copies
 * 3. Batch commit: multiple log records are committed in batches, reducing
 *    syscall overhead
 * 4. Separation of concerns: write threads are separated from flush
 *    threads, avoiding I/O blocking
 *
 * Architecture role:
 * - Receives log records from the core layer
 * - Provides thread-safe buffer and queue management
 * - Passes formatted logs to the service layer for output
 *
 * Note: the atomic layer is an internal implementation detail; most users
 * should use the core-layer API. Only components needing extreme
 * performance or special thread-safety requirements should use the atomic
 * layer directly.
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
 * @brief Atomic-layer configuration structure
 *
 * Configures the atomic layer's behavior, optimizing performance and
 * resource usage.
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
 * @brief Atomic log record node
 *
 * A node in the ring queue containing a log record and the necessary
 * metadata. Designed cache-line aligned (typically 64 bytes) to avoid
 * false sharing.
 */
typedef struct _AtomicLogRecordNode {

    log_record_t record;


    _Atomic uint32_t state;


    _Atomic uint64_t sequence;


    uint8_t _padding[1];
} AtomicLogRecordNode;

/**
 * @brief Lock-free ring buffer queue
 *
 * Multi-producer single-consumer (MPSC) lock-free ring queue. Producers
 * may write concurrently; the consumer reads sequentially.
 */
typedef struct {

    AtomicLogRecordNode *nodes;


    size_t capacity;


    _Atomic size_t head;


    _Atomic size_t tail;


    _Atomic size_t barrier;
} LockFreeRingBuffer;

/**
 * @brief Thread-local buffer
 *
 * A per-thread buffer reducing contention on the global queue. When the
 * local buffer is full, records are batch-committed to the global queue.
 */
typedef struct {

    log_record_t *buffer;


    size_t capacity;


    size_t position;


    uint64_t thread_id;
} ThreadLocalBuffer;


/**
 * @brief Initialize the atomic layer
 *
 * Initializes the atomic-layer internal data structures, including the
 * ring queue and memory pool. Must be called before any atomic-layer
 * function.
 *
 * @param manager Atomic-layer configuration, NULL for defaults
 * @return 0 on success, negative on error
 */
int atomic_logging_init(const atomic_logging_config_t *manager);

/**
 * @brief Submit a log record to the atomic layer (lock-free version)
 *
 * Submits a log record to the lock-free ring queue, safe for concurrent
 * multi-threaded calls. If the queue is full, the function blocks until
 * space is available (unless non-blocking mode is configured).
 *
 * @param record Log record
 * @param non_blocking Whether non-blocking; if true, returns failure
 *                     immediately when the queue is full
 * @return 0 on success, negative on error
 */
int atomic_logging_submit_lockfree(const log_record_t *record, bool non_blocking);

/**
 * @brief Submit a log record to the atomic layer (mutex version)
 *
 * Submits a log record to the mutex-protected queue. Used in
 * environments without lock-free operations or for debugging.
 *
 * @param record Log record
 * @return 0 on success, negative on error
 */
int atomic_logging_submit_mutex(const log_record_t *record);

/**
 * @brief Batch-submit log records
 *
 * Batch-submits multiple log records to the atomic layer, reducing
 * function-call overhead.
 *
 * @param records Log record array
 * @param count Number of records
 * @return Number of records submitted, negative on error
 */
int atomic_logging_submit_batch(const log_record_t *records, size_t count);

/**
 * @brief Acquire a log record from the atomic layer
 *
 * Acquires the next available log record from the atomic layer. Normally
 * called by the service layer's flush thread.
 *
 * @param record Output parameter receiving the log record
 * @param timeout_ms Timeout in ms; 0 does not block, -1 waits forever
 * @return 0 on success, 1 if the queue is empty, negative on error
 */
int atomic_logging_acquire(log_record_t *record, int timeout_ms);

/**
 * @brief Batch-acquire log records
 *
 * Batch-acquires multiple log records from the atomic layer.
 *
 * @param records Output array receiving the records
 * @param max_count Maximum number to acquire
 * @param timeout_ms Timeout in milliseconds
 * @return Number of records actually acquired, negative on error
 */
int atomic_logging_acquire_batch(log_record_t *records, size_t max_count, int timeout_ms);


/**
 * @brief Atomic-layer statistics
 *
 * Runtime performance statistics of the atomic layer.
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
 * @brief Get atomic-layer statistics
 *
 * Gets the atomic layer's runtime statistics for monitoring and debugging.
 *
 * @param out_stats Output parameter receiving the statistics
 * @return 0 on success, negative on error
 */
int atomic_logging_get_stats(atomic_logging_stats_t *out_stats);

/**
 * @brief Get the thread-local buffer
 *
 * Gets or creates the current thread's local buffer. A new buffer is
 * created on the thread's first call.
 *
 * @return Thread-local buffer pointer, NULL on failure
 */
ThreadLocalBuffer *atomic_logging_get_thread_local_buffer(void);

/**
 * @brief Submit the thread-local buffer
 *
 * Batch-commits all records in the thread-local buffer to the global
 * queue.
 *
 * @param buffer Thread-local buffer
 * @return Number of records submitted, negative on error
 */
int atomic_logging_flush_thread_local_buffer(ThreadLocalBuffer *buffer);

/**
 * @brief Flush the atomic layer
 *
 * Force-flushes all buffered records, ensuring all submitted records are
 * available.
 *
 * @return 0 on success, negative on error
 */
int atomic_logging_flush(void);

/**
 * @brief Clean up the atomic layer
 *
 * Releases atomic-layer resources and frees all allocated memory. Must be
 * called before program exit.
 */
void atomic_logging_cleanup(void);


/**
 * @brief Memory barrier: write barrier
 *
 * Ensures all writes before the barrier are visible to subsequent reads.
 * Call after writing shared data.
 */
static inline void atomic_write_barrier(void)
{
    atomic_thread_fence(memory_order_release);
}

/**
 * @brief Memory barrier: read barrier
 *
 * Ensures all reads after the barrier observe the latest data. Call before
 * reading shared data.
 */
static inline void atomic_read_barrier(void)
{
    atomic_thread_fence(memory_order_acquire);
}

/**
 * @brief Atomic compare-and-swap (CAS) operation
 *
 * Compares the value at a memory location with the expected value; if
 * equal, updates it to the new value.
 *
 * @param ptr Target memory location
 * @param expected Expected value pointer (input/output)
 * @param desired New value
 * @return true on success, false on failure
 */
static inline bool airy_atomic_cas_weak(volatile uint64_t *ptr, uint64_t *expected,
                                        uint64_t desired)
{
    return atomic_compare_exchange_strong_64((_Atomic int64_t *)ptr, (int64_t *)expected,
                                             (int64_t)desired, memory_order_acq_rel,
                                             memory_order_acquire);
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMON_ATOMIC_LOGGING_H */
