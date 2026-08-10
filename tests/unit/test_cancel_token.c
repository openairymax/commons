// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file test_cancel_token.c
 * @brief 取消令牌（改进1：异步可中断）单元测试
 *
 * 覆盖：生命周期 / 取消标志 / wait 超时与唤醒 / 回调唤醒链 / 回调链上限 /
 * reset 复位复用 / 跨线程取消唤醒。
 */

#include "cancel_token.h"
#include "airy_memory.h"
#include "platform.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- 回调计数 ---- */
static int g_cb_hits;
static void test_cb(void *ctx)
{
    (void)ctx;
    g_cb_hits++;
}

/* ---- 跨线程测试：线程阻塞在 wait(0)，主线程 cancel 唤醒 ---- */
typedef struct {
    airy_cancel_token_t *token;
    int wait_rc;
} wait_thread_ctx_t;

static void *cancel_wait_thread(void *arg)
{
    wait_thread_ctx_t *ctx = (wait_thread_ctx_t *)arg;
    ctx->wait_rc = airy_cancel_token_wait(ctx->token, 0); /* 无限等待取消 */
    return NULL;
}

static void test_lifecycle(void)
{
    printf("  test_lifecycle...\n");
    airy_cancel_token_t t;
    assert(airy_cancel_token_init(&t) == 0);
    assert(!airy_cancel_token_is_canceled(&t));
    airy_cancel_token_cancel(&t);
    assert(airy_cancel_token_is_canceled(&t));
    /* 幂等：重复 cancel 不崩溃 */
    airy_cancel_token_cancel(&t);
    assert(airy_cancel_token_is_canceled(&t));
    airy_cancel_token_destroy(&t);
    /* destroy 后调用安全 */
    airy_cancel_token_destroy(&t);
    airy_cancel_token_cancel(&t);
    assert(!airy_cancel_token_is_canceled(&t));
    printf("    PASSED\n");
}

static void test_wait_timeout(void)
{
    printf("  test_wait_timeout...\n");
    airy_cancel_token_t t;
    assert(airy_cancel_token_init(&t) == 0);
    /* 未取消 → wait 超时返回 1 */
    assert(airy_cancel_token_wait(&t, 50) == 1);
    /* 已取消 → wait 立即返回 0 */
    airy_cancel_token_cancel(&t);
    assert(airy_cancel_token_wait(&t, 1000) == 0);
    airy_cancel_token_destroy(&t);
    printf("    PASSED\n");
}

static void test_callback_chain(void)
{
    printf("  test_callback_chain...\n");
    airy_cancel_token_t t;
    assert(airy_cancel_token_init(&t) == 0);

    g_cb_hits = 0;
    assert(airy_cancel_token_register(&t, test_cb, NULL) == 0);
    assert(airy_cancel_token_register(&t, test_cb, NULL) == 0);
    /* cancel 触发全部回调 */
    airy_cancel_token_cancel(&t);
    assert(g_cb_hits == 2);

    /* 重复 cancel 不重复触发（幂等） */
    airy_cancel_token_cancel(&t);
    assert(g_cb_hits == 2);

    airy_cancel_token_destroy(&t);
    printf("    PASSED\n");
}

static void test_callback_capacity(void)
{
    printf("  test_callback_capacity...\n");
    airy_cancel_token_t t;
    assert(airy_cancel_token_init(&t) == 0);

    int rc = 0;
    for (int i = 0; i < AIRY_CANCEL_TOKEN_MAX_CBS + 1; i++) {
        rc = airy_cancel_token_register(&t, test_cb, NULL);
    }
    /* 第 (MAX+1) 个注册必须返回 OVERFLOW */
    assert(rc == AIRY_ERR_OVERFLOW);

    airy_cancel_token_destroy(&t);
    printf("    PASSED\n");
}

static void test_reset_reuse(void)
{
    printf("  test_reset_reuse...\n");
    airy_cancel_token_t t;
    assert(airy_cancel_token_init(&t) == 0);

    g_cb_hits = 0; /* 隔离：重置全局回调计数 */
    airy_cancel_token_register(&t, test_cb, NULL);
    airy_cancel_token_cancel(&t);
    assert(g_cb_hits == 1);

    /* reset 复位：标志清 0，回调链保留，可再次取消触发 */
    airy_cancel_token_reset(&t);
    assert(!airy_cancel_token_is_canceled(&t));
    assert(airy_cancel_token_wait(&t, 20) == 1); /* 未取消 → 超时 */

    g_cb_hits = 0;
    airy_cancel_token_cancel(&t);
    assert(g_cb_hits == 1); /* 回调链保留 */
    assert(airy_cancel_token_is_canceled(&t));

    airy_cancel_token_destroy(&t);
    printf("    PASSED\n");
}

static void test_cross_thread_wakeup(void)
{
    printf("  test_cross_thread_wakeup...\n");
    airy_cancel_token_t t;
    assert(airy_cancel_token_init(&t) == 0);

    wait_thread_ctx_t ctx = {.token = &t, .wait_rc = -1};
    airy_thread_t th;
    assert(airy_platform_thread_create(&th, cancel_wait_thread, &ctx) == 0);

    /* 给线程一点时间进入 wait */
    airy_sleep_ms(50);
    airy_cancel_token_cancel(&t);

    void *retval = NULL;
    assert(airy_platform_thread_join(th, &retval) == 0);
    assert(ctx.wait_rc == 0); /* 被取消唤醒，非超时 */

    airy_cancel_token_destroy(&t);
    printf("    PASSED\n");
}

int main(void)
{
    printf("=== test_cancel_token ===\n");
    test_lifecycle();
    test_wait_timeout();
    test_callback_chain();
    test_callback_capacity();
    test_reset_reuse();
    test_cross_thread_wakeup();
    printf("cancel_token tests: ALL PASSED\n");
    return 0;
}
