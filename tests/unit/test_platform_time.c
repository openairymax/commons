// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_platform_time.c
 * @brief 时间服务（逻辑墙钟 / 时区偏移 / SNTP 校对 / 周期同步）单元测试。
 *
 * 网络相关用例在 CI 离线环境自动降级：airy_time_sync_once 失败时仅
 * 验证"不崩溃 + 逻辑墙钟仍可用"（离线回退语义），不强制要求外网。
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "platform.h"

#include <assert.h>
#include <string.h>

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "FAIL: %s\n", message);  \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("Running %s...\n", #test_func);                 \
        if (test_func() != 0) {                                \
            fprintf(stderr, "Test failed: %s\n", #test_func);  \
            failed_tests++;                                    \
        } else {                                               \
            printf("PASS: %s\n", #test_func);                  \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* Hinnant days_from_civil（与实现同算法，独立验证时区偏移） */
static int64_t test_days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2 ? 1 : 0;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static int64_t test_tm_epoch(const struct tm *t)
{
    int64_t day = test_days_from_civil((int64_t)t->tm_year + 1900, (unsigned)t->tm_mon + 1,
                                       (unsigned)t->tm_mday);
    return day * 86400 + (int64_t)t->tm_hour * 3600 + (int64_t)t->tm_min * 60 + t->tm_sec;
}

/* 独立算法计算的当前时区偏移 */
static int test_expected_tz_offset(void)
{
    time_t now = time(NULL);
    struct tm local_tm;
    airy_localtime_r(&now, &local_tm);
    return (int)(test_tm_epoch(&local_tm) - (int64_t)now);
}

/**
 * @brief 时区偏移：与独立算法计算结果一致，且落在合法范围
 */
static int test_tz_offset(void)
{
    int off = airy_time_tz_offset();
    TEST_ASSERT(off >= -43200 && off <= 50400, "tz offset in legal range");

    int expect = test_expected_tz_offset();
    TEST_ASSERT(off == expect, "tz offset matches localtime-based computation");

    printf("  tz offset = %d s\n", off);
    return 0;
}

/**
 * @brief 逻辑墙钟单调性：连续调用不减小，短睡后严格递增
 */
static int test_wall_monotonic(void)
{
    uint64_t t0 = airy_time_wall_ms();
    uint64_t t1 = airy_time_wall_ms();
    TEST_ASSERT(t1 >= t0, "wall clock non-decreasing");

    airy_sleep_ms(20);
    uint64_t t2 = airy_time_wall_ms();
    TEST_ASSERT(t2 > t0, "wall clock advances after sleep");
    TEST_ASSERT((t2 - t0) >= 10, "wall clock advances by at least 10ms");

    printf("  wall: t0=%llu t2=%llu delta=%llu ms\n", (unsigned long long)t0,
           (unsigned long long)t2, (unsigned long long)(t2 - t0));
    return 0;
}

/**
 * @brief 离线回退：未联网同步时逻辑墙钟与系统时间基本一致
 */
static int test_wall_system(void)
{
    uint64_t wall_sec = airy_time_wall_sec();
    uint64_t sys_sec = (uint64_t)time(NULL);
    int64_t diff = (int64_t)wall_sec - (int64_t)sys_sec;
    TEST_ASSERT(diff >= -5 && diff <= 5, "offline wall clock within 5s of system time");
    printf("  wall=%llu sys=%llu diff=%lld s\n", (unsigned long long)wall_sec,
           (unsigned long long)sys_sec, (long long)diff);
    return 0;
}

/**
 * @brief SNTP 单次校对：成功则 ready=1 且偏移记录；失败则离线回退不崩溃
 */
static int test_sync_once(void)
{
    int rc = airy_time_sync_once(1000);
    printf("  sync_once rc=%d\n", rc);

    if (rc == 0) {
        TEST_ASSERT(airy_time_sync_ready() == 1, "ready after successful sync");

        /* 成功路径：校正后逻辑墙钟（当前时区标准时间）与网络 UTC+时区
         * 的差应在合理容差内（同步时刻已过，容差取 10s） */
        uint64_t wall = airy_time_wall_sec();
        uint64_t sys = (uint64_t)time(NULL);
        int64_t expected_diff = airy_time_sync_delta() / 1000;
        int64_t actual_diff = (int64_t)wall - (int64_t)sys;
        TEST_ASSERT(actual_diff >= expected_diff - 10 && actual_diff <= expected_diff + 10,
                    "wall clock tracks synced delta");
        printf("  synced: wall=%llu sys=%llu delta=%lld ms\n", (unsigned long long)wall,
               (unsigned long long)sys, (long long)airy_time_sync_delta());
    } else {
        TEST_ASSERT(rc == -110 /* AIRY_ETIMEDOUT */, "offline sync returns timeout");
        TEST_ASSERT(airy_time_sync_ready() == 0, "offline: not ready");

        /* 离线回退：逻辑墙钟仍可用且贴近系统时间 */
        uint64_t wall = airy_time_wall_sec();
        uint64_t sys = (uint64_t)time(NULL);
        int64_t diff = (int64_t)wall - (int64_t)sys;
        TEST_ASSERT(diff >= -5 && diff <= 5, "offline fallback keeps wall clock usable");
        printf("  offline fallback: wall=%llu sys=%llu diff=%lld s\n",
               (unsigned long long)wall, (unsigned long long)sys, (long long)diff);
    }
    return 0;
}

/**
 * @brief 周期校对线程启停：start/stop 幂等，不崩溃
 */
static int test_sync_loop(void)
{
    int rc = airy_time_sync_start(30);
    TEST_ASSERT(rc == 0, "sync loop start succeeds");

    airy_sleep_ms(1200);
    airy_time_sync_stop();
    airy_time_sync_stop(); /* 幂等 */

    /* 再次启动（线程已退出）后立即停止 */
    rc = airy_time_sync_start(30);
    TEST_ASSERT(rc == 0, "sync loop restart succeeds");
    airy_time_sync_stop();

    printf("  sync loop start/stop ok\n");
    return 0;
}

int main(void)
{
    airy_network_init();

    printf("===========================================\n");
    printf("  agentrt/commons/platform/time 单元测试\n");
    printf("===========================================\n\n");

    TEST_RUN(test_tz_offset);
    TEST_RUN(test_wall_monotonic);
    TEST_RUN(test_wall_system);
    TEST_RUN(test_sync_once);
    TEST_RUN(test_sync_loop);

    airy_network_cleanup();

    printf("\n===========================================\n");
    printf("  测试结果: %d 通过, %d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");

    return failed_tests > 0 ? 1 : 0;
}
