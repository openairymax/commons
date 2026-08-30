// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file bench_platform_perf.c
 * @brief 平台性能微基准（0.1.6f 任务2 强化验证）。
 *
 * 量化原子操作不同 memory_order 语义的开销差异（seq_cst vs
 * acquire/release/relaxed），以及互斥锁/时间戳/随机数调用开销。
 * 独立可运行（不注册 ctest）：gcc bench + libairy_common 链接。
 *
 * 验证目标：ARM/RISC-V 上 seq_cst load/store 需 dmb 全屏障，降级
 * acquire/release 后可达 2x+；x86 上 seq_cst store（xchg/lock）vs
 * release store（普通 mov）同样显著。真实产物依赖此基准量化。
 */

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"

#define N 20000000

static volatile uint64_t g_sink = 0;

static double bench_load_seq(void)
{
    static _Atomic int v = 0;
    uint64_t t0 = airy_time_ns();
    int s = 0;
    for (int i = 0; i < N; i++)
        s += atomic_load_explicit(&v, memory_order_seq_cst);
    uint64_t t1 = airy_time_ns();
    g_sink += (uint64_t)s;
    return (double)(t1 - t0) / (double)N;
}

static double bench_load_acq(void)
{
    static _Atomic int v = 0;
    uint64_t t0 = airy_time_ns();
    int s = 0;
    for (int i = 0; i < N; i++)
        s += atomic_load_explicit(&v, memory_order_acquire);
    uint64_t t1 = airy_time_ns();
    g_sink += (uint64_t)s;
    return (double)(t1 - t0) / (double)N;
}

static double bench_load_rlx(void)
{
    static _Atomic int v = 0;
    uint64_t t0 = airy_time_ns();
    int s = 0;
    for (int i = 0; i < N; i++)
        s += atomic_load_explicit(&v, memory_order_relaxed);
    uint64_t t1 = airy_time_ns();
    g_sink += (uint64_t)s;
    return (double)(t1 - t0) / (double)N;
}

static double bench_store_seq(void)
{
    static _Atomic int v = 0;
    uint64_t t0 = airy_time_ns();
    for (int i = 0; i < N; i++)
        atomic_store_explicit(&v, i, memory_order_seq_cst);
    uint64_t t1 = airy_time_ns();
    g_sink += (uint64_t)atomic_load_explicit(&v, memory_order_relaxed);
    return (double)(t1 - t0) / (double)N;
}

static double bench_store_rel(void)
{
    static _Atomic int v = 0;
    uint64_t t0 = airy_time_ns();
    for (int i = 0; i < N; i++)
        atomic_store_explicit(&v, i, memory_order_release);
    uint64_t t1 = airy_time_ns();
    g_sink += (uint64_t)atomic_load_explicit(&v, memory_order_relaxed);
    return (double)(t1 - t0) / (double)N;
}

static double bench_fetch_seq(void)
{
    static _Atomic int v = 0;
    uint64_t t0 = airy_time_ns();
    for (int i = 0; i < N; i++)
        (void)atomic_fetch_add_explicit(&v, 1, memory_order_seq_cst);
    uint64_t t1 = airy_time_ns();
    g_sink += (uint64_t)atomic_load_explicit(&v, memory_order_relaxed);
    return (double)(t1 - t0) / (double)N;
}

static double bench_fetch_rlx(void)
{
    static _Atomic int v = 0;
    uint64_t t0 = airy_time_ns();
    for (int i = 0; i < N; i++)
        (void)atomic_fetch_add_explicit(&v, 1, memory_order_relaxed);
    uint64_t t1 = airy_time_ns();
    g_sink += (uint64_t)atomic_load_explicit(&v, memory_order_relaxed);
    return (double)(t1 - t0) / (double)N;
}

static double bench_mutex(void)
{
    static airy_mtx_t m = AIRY_INVALID_MUTEX;
    airy_mtx_init(&m);
    const int n = N / 2;
    uint64_t t0 = airy_time_ns();
    for (int i = 0; i < n; i++) {
        airy_mtx_lock(&m);
        airy_mtx_unlock(&m);
    }
    uint64_t t1 = airy_time_ns();
    airy_mtx_destroy(&m);
    return (double)(t1 - t0) / (double)n;
}

static double bench_time(void)
{
    uint64_t t0 = airy_time_ns();
    uint64_t s = 0;
    for (int i = 0; i < N; i++)
        s += airy_time_ns();
    uint64_t t1 = airy_time_ns();
    g_sink += s;
    return (double)(t1 - t0) / (double)N;
}

static double bench_random(void)
{
    uint64_t t0 = airy_time_ns();
    uint32_t s = 0;
    const int n = 100000;
    for (int i = 0; i < n; i++)
        s += airy_random_uint32(0, 1000000);
    uint64_t t1 = airy_time_ns();
    g_sink += s;
    return (double)(t1 - t0) / (double)n;
}

int main(void)
{
    printf("== platform perf benchmark (arch=%s) ==\n", airy_arch_name());
    printf("N=%d iterations per case\n\n", N);

    double l_seq = bench_load_seq();
    double l_acq = bench_load_acq();
    double l_rlx = bench_load_rlx();
    double s_seq = bench_store_seq();
    double s_rel = bench_store_rel();
    double f_seq = bench_fetch_seq();
    double f_rlx = bench_fetch_rlx();
    double mx = bench_mutex();
    double tm = bench_time();
    double rnd = bench_random();

    printf("%-22s %10.2f ns/op\n", "atomic load seq_cst", l_seq);
    printf("%-22s %10.2f ns/op   (%.2fx vs seq_cst)\n", "atomic load acquire", l_acq,
           l_seq / l_acq);
    printf("%-22s %10.2f ns/op   (%.2fx vs seq_cst)\n", "atomic load relaxed", l_rlx,
           l_seq / l_rlx);
    printf("%-22s %10.2f ns/op\n", "atomic store seq_cst", s_seq);
    printf("%-22s %10.2f ns/op   (%.2fx vs seq_cst)\n", "atomic store release", s_rel,
           s_seq / s_rel);
    printf("%-22s %10.2f ns/op\n", "atomic fetch_add seq_cst", f_seq);
    printf("%-22s %10.2f ns/op   (%.2fx vs seq_cst)\n", "atomic fetch_add relaxed", f_rlx,
           f_seq / f_rlx);
    printf("%-22s %10.2f ns/op\n", "airy_mtx lock+unlock", mx);
    printf("%-22s %10.2f ns/op\n", "airy_time_ns", tm);
    printf("%-22s %10.2f ns/op\n", "airy_random_uint32", rnd);

    (void)g_sink;
    printf("\ndone (sink=%llu)\n", (unsigned long long)g_sink);
    return 0;
}
