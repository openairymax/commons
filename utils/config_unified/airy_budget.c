/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file airy_budget.c
 * @brief 超时预算树的唯一解析点（0.1.21 C-21）。
 *
 * 五个层号来自 airy_budget.h，配置档只描述其中四层；think 的缺省恒取
 * airy_defaults.h 的 AIRY_THINK_TIMEOUT_DEFAULT_MS，避免出现第二份真值。
 *
 * 这里不做缓存：解析只碰少量 getenv，代价远低于一次 IPC；缓存反而会在
 * 进程内制造「启动时打印一套、调用时用另一套」的分裂，正是 C-21 要消灭的。
 */

#include "airy_budget.h"

#include "airy_defaults.h"
#include "logging.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define BUDGET_MIN_MS 1000u
#define BUDGET_MAX_MS 86400000u

typedef struct {
    const char *name;
    unsigned cli;
    unsigned srv;
    unsigned prv;
    unsigned tool;
} budget_profile_t;

static const budget_profile_t g_profiles[] = {
    {"small", 300000u, 150000u, 60000u, 45000u},
    {"default", 600000u, 150000u, 70000u, 60000u},
    {"large", 1800000u, 600000u, 240000u, 180000u},
};

#define BUDGET_PROFILES (sizeof(g_profiles) / sizeof(g_profiles[0]))

static const budget_profile_t *budget_pick(void)
{
    const char *want = getenv("AIRY_TIMEOUT_PROFILE");
    if (want && want[0])
        for (size_t i = 0; i < BUDGET_PROFILES; i++)
            if (strcmp(want, g_profiles[i].name) == 0)
                return &g_profiles[i];
    return &g_profiles[1];
}

/* 严格整数解析：整串必须是十进制数字且在 [BUDGET_MIN_MS, BUDGET_MAX_MS]。 */
static int budget_parse(const char *s, unsigned *out)
{
    if (!s || !s[0])
        return -1;
    errno = 0;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (errno != 0 || !end || *end != '\0' || v < BUDGET_MIN_MS || v > BUDGET_MAX_MS)
        return -1;
    *out = (unsigned)v;
    return 0;
}

static unsigned budget_env(const char *name, unsigned dflt)
{
    unsigned v = 0;
    return budget_parse(getenv(name), &v) == 0 ? v : dflt;
}

/* 解析顺序见契约头：层内 env > think 专用 env > profile > 缺省。 */
static void budget_resolve(unsigned v[AIRY_BUDGET_LAYERS])
{
    const budget_profile_t *p = budget_pick();
    v[AIRY_BUDGET_CLIENT] = budget_env("AIRY_TIMEOUT_CLIENT_MS", p->cli);
    v[AIRY_BUDGET_SERVER] = budget_env("AIRY_TIMEOUT_SERVER_MS", p->srv);
    v[AIRY_BUDGET_THINK] =
        budget_env("AIRY_TIMEOUT_THINK_MS",
                   budget_env("AIRY_THINK_TIMEOUT_MS", AIRY_THINK_TIMEOUT_DEFAULT_MS));
    v[AIRY_BUDGET_PROVIDER] = budget_env("AIRY_TIMEOUT_PROVIDER_MS", p->prv);
    v[AIRY_BUDGET_TOOL] = budget_env("AIRY_TIMEOUT_TOOL_MS", p->tool);
}

/* 只向上钳制：内层预算是事实，外层必须覆盖它。 */
static int budget_clamp(unsigned v[AIRY_BUDGET_LAYERS])
{
    int fixed = 0;
    unsigned need = (v[AIRY_BUDGET_THINK] * 6u + 4u) / 5u;
    if (v[AIRY_BUDGET_SERVER] < need) {
        v[AIRY_BUDGET_SERVER] = need;
        fixed++;
    }
    if (v[AIRY_BUDGET_PROVIDER] < v[AIRY_BUDGET_TOOL]) {
        v[AIRY_BUDGET_PROVIDER] = v[AIRY_BUDGET_TOOL];
        fixed++;
    }
    if (v[AIRY_BUDGET_SERVER] < v[AIRY_BUDGET_PROVIDER]) {
        v[AIRY_BUDGET_SERVER] = v[AIRY_BUDGET_PROVIDER];
        fixed++;
    }
    if (v[AIRY_BUDGET_CLIENT] < v[AIRY_BUDGET_SERVER]) {
        v[AIRY_BUDGET_CLIENT] = v[AIRY_BUDGET_SERVER];
        fixed++;
    }
    return fixed;
}

unsigned airy_budget_ms(airy_budget_layer_t layer)
{
    if (layer < 0 || layer >= AIRY_BUDGET_LAYERS)
        return 0;
    unsigned v[AIRY_BUDGET_LAYERS];
    budget_resolve(v);
    (void)budget_clamp(v);
    return v[layer];
}

unsigned airy_budget_outer(unsigned inner_ms)
{
    if (inner_ms == 0)
        return 0;
    unsigned long long o = (unsigned long long)inner_ms + inner_ms / 4u;
    o = (o + 999ull) / 1000ull * 1000ull;
    return o > BUDGET_MAX_MS ? BUDGET_MAX_MS : (unsigned)o;
}

const char *airy_budget_profile(void)
{
    return budget_pick()->name;
}

int airy_budget_verify(void)
{
    unsigned v[AIRY_BUDGET_LAYERS];
    budget_resolve(v);
    return budget_clamp(v);
}

void airy_budget_log(const char *who)
{
    unsigned v[AIRY_BUDGET_LAYERS];
    budget_resolve(v);
    int fixed = budget_clamp(v);
    AIRY_LOG_INFO("budget[%s]: profile=%s client=%u server=%u think=%u provider=%u tool=%u clamp=%d",
                  who ? who : "?", airy_budget_profile(), v[AIRY_BUDGET_CLIENT],
                  v[AIRY_BUDGET_SERVER], v[AIRY_BUDGET_THINK], v[AIRY_BUDGET_PROVIDER],
                  v[AIRY_BUDGET_TOOL], fixed);
    if (fixed > 0)
        AIRY_LOG_WARN("budget[%s]: %d layer(s) raised to keep client >= server >= "
                      "provider >= tool",
                      who ? who : "?", fixed);
}
