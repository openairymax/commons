# resource — API 错误恢复池

**模块路径**: `commons/utils/resource/` · **版本**: 0.1.16

**API 错误恢复池**：多凭证轮换 + 指数退避重试 + 模型降级链，为外部服务调用提供自愈能力。源文件编入静态库 `airy_common`。

## 概述

- **凭证池自愈**：至多 8 个 API 密钥轮转发放，按成功/失败动态维护健康分，认证失败立即禁用、连续失败 5 次自动失效、全员失效时自动复活最优凭证。
- **降级重试**：服务端错误持续时沿 fallback 模型链逐级降级，fallback 耗尽进入缓存级；恢复成功后逐级回升。

## 目录结构

```
utils/resource/
├── api_recovery.h       # 接口（144 行）
├── api_recovery.c       # 实现（528 行）
└── README.md
```

## API 错误恢复（api_recovery）

### 数据结构与常量

| 类型 | 说明 |
|---|---|
| `api_rec_pool_t` | 恢复池：名称、凭证数组、fallback 模型数组、重试配置、降级级别、统计计数 |
| `api_rec_credential_t` | 单个凭证：key、`is_valid`、成功/失败时间、连败计数、`health_score` |
| `api_rec_model_t` | fallback 模型：名称、成本权重、优先级、可用标志 |
| `api_rec_result_t` | 单次执行结果：HTTP 码、恢复错误码、可重试/应轮换/应降级标志、消息 |
| `api_rec_request_fn` | 请求回调 `int (*)(void *ctx, url, body, cred, char **resp_body, long *http_code)`，返回 0 且 `*http_code` 为 2xx 视为成功 |

| 常量 | 值 | 含义 |
|---|---|---|
| `API_REC_MAX_CREDENTIALS` | 8 | 池内凭证上限 |
| `API_REC_MAX_CRED_LEN` | 256 | 密钥长度上限（含结尾 NUL） |
| `API_REC_MAX_FALLBACK_MODELS` | 4 | fallback 模型上限 |
| `API_REC_MAX_MODEL_LEN` | 64 | 模型名长度上限 |
| `API_REC_MAX_RETRY` | 5 | 默认最大重试次数 |
| `API_REC_DEFAULT_BASE_DELAY_MS` | 200 | 默认退避基础延迟 |

健康分参数（`commons/include/airy_defaults.h`）：成功衰减系数 0.9（分数向 1.0 收敛）、失败惩罚 0.3（分数 ×0.7）、发放门槛 0.2、连败失效阈值 5。

### 接口

| 组 | 函数 |
|---|---|
| 生命周期 | `api_rec_pool_create(name)` / `api_rec_pool_destroy(pool)` |
| 凭证池 | `api_rec_add_credential(pool, key)` / `api_rec_remove_credential(pool, idx)` / `api_rec_next_credential(pool)` / `api_rec_mark_cred_success(pool)` / `api_rec_mark_cred_failure(pool, err)` / `api_rec_cred_health(pool, idx)` |
| 模型降级 | `api_rec_add_fallback_model(pool, model, cost_weight, priority)` / `api_rec_current_model(pool)` / `api_rec_degrade(pool)` / `api_rec_upgrade(pool)` / `api_rec_current_level(pool)` |
| 执行与配置 | `api_rec_execute_with_recovery(...)` / `api_rec_set_retry_config(pool, max_retries, base_delay_ms, backoff_factor, jitter_ratio)` / `api_rec_bind_circuit_breaker(pool, breaker)` / `api_rec_get_stats(pool, ...)` |
| 字符串化 | `api_rec_error_string(code)` / `api_rec_degradation_string(level)` |

### 行为语义

- **HTTP 码归类**：429 → `RATE_LIMIT`；401/403 → `AUTH`；500–599 → `SERVER`；0 或 ≥600 → `NETWORK`；其余 → `UNKNOWN`。`API_REC_ERR_TIMEOUT` 不由归类产生，保留给调用方自行标注请求超时。
- **凭证发放**：`next_credential` 轮转发放「有效且健康分 > 0.2」的密钥；全部不合格时选健康分最高者复活（重新置有效、清连败）并发放。`mark_cred_success/failure` 作用于最近一次发放的凭证。`AUTH` 失败立即禁用该凭证；`RATE_LIMIT` 失败直接轮换到下一个。
- **降级链**：`degrade` 沿 fallback 数组前进一级（`LOWER_TIER`），走完最后一级后进入 `CACHE`；`upgrade` 回退一级，回到起点即 `NONE`。未降级时 `current_model` 返回 `"primary"`。
- **重试循环**（`execute_with_recovery`）：每次重试前等待 `base_delay × factor^(n-1)` 并叠加 ±jitter 抖动；RATE_LIMIT 轮换凭证后重试；SERVER 且第 2 次尝试起自动 `degrade` 后重试；AUTH 轮换凭证后重试。成功（回调返回 0 且 2xx）时 `mark_cred_success`、若在降级中自动 `upgrade` 一级，并把响应体所有权转交 `*out_response`（调用方以 `AIRY_FREE` 释放）；失败返回 -1，中间响应缓冲区由内部释放，`out_result` 填错误码与 `"All retries exhausted"` 消息并置 `is_retriable`。
- `set_retry_config` 对非法定值归一为默认（retries/delay 取正、factor ≤1 归 2.0、jitter 为负归 0.1）。

## 语义与约束

- **熔断器仅存储**：`api_rec_bind_circuit_breaker` 只记录指针，执行路径不调用任何熔断逻辑；`user_context` 字段同理预留未用。
- **恢复池非线程安全**：池结构字段公开且轮转索引为普通变量，多线程共享同一池需外部加锁。
- `api_rec_cred_health` 以负值（`AIRY_EINVAL`）表示参数错误，正常值域为 `[0.0, 1.0]`。

## 用法示例

```c
#include "airy_memory.h"
#include "api_recovery.h"

static int chat_request(void *ctx, const char *url, const char *body, const char *cred,
                        char **resp_body, long *http_code)
{
    (void)ctx;
    /* 调用实际 HTTP 客户端：回填 *http_code，
     * 成功时用分配器写入 *resp_body 并返回 0 */
    *resp_body = NULL;
    *http_code = 500;
    return -1;
}

int demo(void)
{
    api_rec_pool_t *pool = api_rec_pool_create("provider-a");
    if (!pool)
        return 1;

    api_rec_add_credential(pool, "sk-first");
    api_rec_add_credential(pool, "sk-backup");
    api_rec_add_fallback_model(pool, "small-model", 0.4f, 1);

    char *resp = NULL;
    long code = 0;
    api_rec_result_t result;
    if (api_rec_execute_with_recovery(pool, chat_request, NULL,
                                      "https://api.example/v1/chat", "{}",
                                      &resp, &code, &result) == 0) {
        /* 成功时 resp 所有权归调用方 */
        AIRY_FREE(resp);
    }
    api_rec_pool_destroy(pool);
    return result.rec_code == API_REC_ERR_NONE ? 0 : 1;
}
```

## 构建与依赖

`api_recovery.c` 在 `airy_common` 源列表中；`utils/resource/` 已注册 PUBLIC include 路径并整体安装头文件（`include/agentrt/utils/resource/`，排除 `*_internal.h`）。

| 依赖 | 使用方 | 用途 |
|---|---|---|
| commons `utils/error` | api_recovery | `airy_err_t` 与错误码宏 |
| commons `utils/memory` | api_recovery | `AIRY_MALLOC/CALLOC/FREE`、strdup 封装 |
| commons `platform`（`platform_misc.h`） | api_recovery | `airy_time_ms` / `airy_time_ns` / `airy_random_*` |
| commons `utils/logging`（`svc_logger.h`） | api_recovery | `SVC_LOG_*` 事件日志 |
| commons `include/airy_defaults.h` | api_recovery | 健康分/退避默认常量 |

daemons 的恢复策略测试（`daemons/common/tests/test_strategies_recovery.c`）消费 `api_recovery.h`；本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
