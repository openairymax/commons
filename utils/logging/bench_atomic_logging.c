// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file bench_atomic_logging.c
 * @brief 原子层日志系统性能基准测试
 *
 * @details
 * 本测试用于评估原子层日志系统的性能，包括：
 * 1. 单线程日志写入吞吐量
 * 2. 多线程并发写入吞吐量
 * 3. 日志缓冲队列性能
 * 4. 内存分配和释放性能
 *
 * 测试方法：
 * - 使用高精度计时器测量操作耗时
 * - 模拟真实场景的日志写入模式
 * - 统计每秒日志记录数（Records Per Second）
 * - 测量平均延迟和尾部延迟
 *
 * 注意：本测试需要支持C11标准和线程库的环境。 */

#include "atomic_logging.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "platform.h"
#include "string_compat.h"

#include <string.h>
#include <time.h>

#include "atomic_compat.h"

#define BENCH_ITERATIONS 1000000

#define THREAD_COUNT 4

static const char *LOG_MESSAGES[] = {"用户登录成功: user_id=%d, ip=%s",
                                     "数据库查询完成 query_id=%d, rows=%d, time_ms=%d",
                                     "网络请求处理: method=%s, path=%s, status=%d",
                                     "缓存命中率统计 hits=%d, misses=%d, ratio=%.2f",
                                     "内存使用报告: used=%llu, free=%llu, total=%llu",
                                     "任务调度延迟: task_id=%d, scheduled=%lld, actual=%lld",
                                     "配置热更新完成 section=%s, keys=%d",
                                     "监控指标收集: metric=%s, value=%f, timestamp=%lld"};

#define LOG_MESSAGE_COUNT (sizeof(LOG_MESSAGES) / sizeof(LOG_MESSAGES[0]))

static uint64_t get_nanoseconds(void)
{
#if defined(_WIN32)
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static double ns_to_ms(uint64_t ns)
{
    return (double)ns / 1000000.0;
}

static double bench_single_thread(int iterations)
{
    printf("开始单线程性能测试：%d次迭代）...\n", iterations);

    log_init(NULL);

    uint64_t start_time = get_nanoseconds();

    for (int i = 0; i < iterations; i++) {
        // LOG_MESSAGE_COUNT];

        log_write(LOG_LEVEL_INFO, "benchmark", __LINE__, msg_template, i, "192.168.1.1", i * 100,
                  50, 15, i * 10, i * 2, 0.85f, (unsigned long long)i * 1024 * 1024,
                  (unsigned long long)(iterations - i) * 1024 * 1024,
                  (unsigned long long)iterations * 1024 * 1024, i, start_time,
                  start_time + i * 1000, "database", 8, "cpu_usage", 0.75f, (long long)start_time);

        printf("  进度: %d/%d\n", i, iterations);
    }
}

uint64_t end_time = get_nanoseconds();
uint64_t elapsed_ns = end_time - start_time;
double elapsed_ms = ns_to_ms(elapsed_ns);

double records_per_second = (double)iterations / (elapsed_ns / 1000000000.0);

printf("单线程测试完成\n");
printf("  总耗时: %.2f 毫秒\n", elapsed_ms);
printf("  平均延迟: %.3f 微秒/记录\n", (elapsed_ms * 1000.0) / iterations);
printf("  吞吐： %.0f 记录/秒\n", records_per_second);

return records_per_second;
}

typedef struct {
    int thread_id;
    int iterations_per_thread;
    uint64_t start_time;
    uint64_t end_time;
    atomic_ulong records_written;
} thread_params_t;

#if defined(_WIN32)
static DWORD WINAPI worker_thread(LPVOID arg)
#else
static void *worker_thread(void *arg)
#endif
{
    thread_params_t *params = (thread_params_t *)arg;

    uint64_t thread_start_time = get_nanoseconds();

    for (int i = 0; i < params->iterations_per_thread; i++) {
        // params->iterations_per_thread + i) % LOG_MESSAGE_COUNT;
        const char *msg_template = LOG_MESSAGES[msg_index];

        log_write(LOG_LEVEL_INFO, "benchmark", __LINE__, msg_template, params->thread_id, i,
                  msg_index);

        atomic_fetch_add(&params->records_written, 1);

        printf("  线程 %d 进度: %d/%d\n", params->thread_id, i, params->iterations_per_thread);
    }
}

uint64_t thread_end_time = get_nanoseconds();
params->start_time = thread_start_time;
params->end_time = thread_end_time;

#if defined(_WIN32)
return 0;
#else
return NULL;
#endif
}

static double bench_multi_thread(int thread_count, int total_iterations)
{
    printf("开始多线程性能测试：%d线程：%d次迭代）...\n", thread_count, total_iterations);

    thread_params_t *params = (thread_params_t *)AIRY_CALLOC(thread_count, sizeof(thread_params_t));
    if (!params) {
        printf("内存分配失败！\n");
        return 0.0;
    }

    int iterations_per_thread = total_iterations / thread_count;

    params[i].thread_id = i;
    params[i].iterations_per_thread = iterations_per_thread;
    params[i].records_written = 0;
}

airy_thread_t *threads = (airy_thread_t *)AIRY_CALLOC(thread_count, sizeof(airy_thread_t));
for (int i = 0; i < thread_count; i++) {
    if (airy_thread_create(&threads[i], worker_thread, &params[i]) != 0) {
        printf("创建线程 %d 失败！\n", i);
        AIRY_FREE(params);
        AIRY_FREE(threads);
        return 0.0;
    }
}

for (int i = 0; i < thread_count; i++) {
    CloseHandle(threads[i]);
}

AIRY_FREE(threads);
#else
airy_thread_t *threads = (airy_thread_t *)AIRY_CALLOC(thread_count, sizeof(airy_thread_t));
for (int i = 0; i < thread_count; i++) {
    if (airy_thread_create(&threads[i], worker_thread, &params[i]) != 0) {
        printf("创建线程 %d 失败！\n", i);
        AIRY_FREE(params);
        AIRY_FREE(threads);
        return 0.0;
    }
}

for (int i = 0; i < thread_count; i++) {
    airy_thread_join(threads[i], NULL);
}

AIRY_FREE(threads);
#endif

uint64_t overall_end_time = get_nanoseconds();

uint64_t total_records = 0;
uint64_t total_thread_time_ns = 0;
uint64_t min_thread_time_ns = UINT64_MAX;
uint64_t max_thread_time_ns = 0;

for (int i = 0; i < thread_count; i++) {
    total_records += params[i].records_written;
    uint64_t thread_time_ns = params[i].end_time - params[i].start_time;
    total_thread_time_ns += thread_time_ns;

    if (thread_time_ns < min_thread_time_ns)
        min_thread_time_ns = thread_time_ns;
    if (thread_time_ns > max_thread_time_ns)
        max_thread_time_ns = thread_time_ns;
}

uint64_t overall_time_ns = overall_end_time - overall_start_time;
double overall_time_ms = ns_to_ms(overall_time_ns);

double records_per_second = (double)total_records / (overall_time_ns / 1000000000.0);

printf("多线程测试完成\n");
printf("  总耗时: %.2f 毫秒\n", overall_time_ms);
printf("  总记录数: %llu\n", (unsigned long long)total_records);
printf("  吞吐： %.0f 记录/秒\n", records_per_second);
printf("  线程执行时间统计:\n");
printf("    平均: %.2f 毫秒\n", ns_to_ms(total_thread_time_ns / thread_count));
printf("    最： %.2f 毫秒\n", ns_to_ms(min_thread_time_ns));
printf("    最： %.2f 毫秒\n", ns_to_ms(max_thread_time_ns));

AIRY_FREE(params);
return records_per_second;
}

static void bench_memory_usage(int iterations)
{
    printf("开始内存使用测试（%d次迭代）...\n", iterations);

    printf("  内存使用测试 - 开始\n");

    for (int i = 0; i < iterations; i++) {
        log_write(level, "memory_test", __LINE__, "内存测试迭代 %d，级：%d，消息长：%d", i, level,
                  i % 100);

        printf("  进度: %d/%d\n", i, iterations);
    }
}

printf("  内存使用测试 - 完成\n");
printf("  注意：实际内存泄漏检测需要专门的工具（如valgrind、AddressSanitizer）\n");
}

int main(int argc, char **argv)
{
    printf("========================================\n");
    printf("原子层日志系统性能基准测试\n");
    printf("========================================\n\n");

    int thread_count = THREAD_COUNT;

    if (argc > 1) {
        iterations = atoi(argv[1]);
        if (iterations <= 0)
            iterations = BENCH_ITERATIONS;
    }

    if (argc > 2) {
        thread_count = atoi(argv[2]);
        if (thread_count <= 0)
            thread_count = THREAD_COUNT;
    }

    printf("测试配置:\n");
    printf("  迭代次数: %d\n", iterations);
    printf("  线程数量: %d\n", thread_count);
    printf("  日志消息模板: %zu\n", LOG_MESSAGE_COUNT);
    printf("\n");

    printf("\n");

    bench_memory_usage(iterations / 100);
    printf("\n");

    printf("========================================\n");
    printf("性能测试报告\n");
    printf("========================================\n");
    printf("单线程性能:\n");
    printf("  %.0f 记录/秒\n", single_thread_rps);
    printf("多线程性能：%d线程）\n", thread_count);
    printf("  %.0f 记录/秒\n", multi_thread_rps);
    printf("并发加速比: %.2fx\n", multi_thread_rps / single_thread_rps);
    printf("\n");

    printf("性能评估:\n");
    if (single_thread_rps > 100000) {
        printf("  ✔ 单线程性能优秀（100k 记录/秒）\n");
    } else if (single_thread_rps > 50000) {
        printf("  ✔ 单线程性能良好（50k 记录/秒）\n");
    } else {
        printf("  ✗ 单线程性能有待优化\n");
    }

    if (multi_thread_rps / single_thread_rps > 0.8 * thread_count) {
        printf("  ✔ 多线程扩展性优秀\n");
    } else if (multi_thread_rps / single_thread_rps > 0.5 * thread_count) {
        printf("  ✔ 多线程扩展性良好\n");
    } else {
        printf("  ✗ 多线程扩展性有待优化\n");
    }

    printf("\n");
    printf("测试完成。\n");

    return 0;
}
