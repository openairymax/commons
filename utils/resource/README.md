# resource — API 错误恢复与资源守卫

**模块路径**: `commons/utils/resource/` · **版本**: 0.1.15

三个独立组件的资源工具集：**API 错误恢复池**（多凭证轮换 + 指数退避重试 + 模型降级链）、**作用域守卫**（RAII 自动清理 + 可选分配追踪）、**资源配额账本**（内存 / I/O 记账与超限标志位）。三个源文件均编入静态库 `airy_common`。

## 概述

- **凭证池自愈**：至多 8 个 API 密钥轮转发放，按成功/失败动态维护健康分，认证失败立即禁用、连续失败 5 次自动失效、全员失效时自动复活最优凭证。
- **降级重试**：服务端错误持续时沿 fallback 模型链逐级降级，fallback 耗尽进入缓存级；恢复成功后逐级回升。
- **作用域守卫**：`AIRY_SCOPE_GUARD` / `AIRY_SCOPE_EXIT` 借助编译器 `cleanup` 属性在作用域结束自动调用清理函数。
- **配额账本**：申请前检查、分配/释放记账、峰值统计与超限位标志，供上层做资源准入控制。

## 目录结构

```
utils/resource/
├── api_recovery.h       # API 错误恢复池接口（144 行）
├── api_recovery.c       # 实现（536 行）
├── resource_guard.h     # 作用域守卫 + 可选资源追踪
├── resource_guard.c     # 实现（含追踪链表）
├── resource_quota.h     # 资源配额接口
├── resource_quota.c     # 实现
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

## 作用域守卫与资源追踪（resource_guard）

### 守卫 API 与宏

| 接口 | 说明 |
|---|---|
| `airy_resource_guard_init(guard, resource, cleanup, file, line, name)` | 手工初始化守卫（置 `active = 1`） |
| `airy_resource_guard_cleanup(guard)` | 立即执行清理并复位守卫 |
| `airy_resource_guard_dismiss(guard)` | 取消自动清理，所有权移交调用方 |
| `AIRY_SCOPE_GUARD(resource, cleanup)` | 声明式守卫（变量名 `_guard<行号>`），作用域结束自动 cleanup |
| `AIRY_SCOPE_EXIT(resource, cleanup)` | 同上（变量名 `_scope_exit<行号>`） |
| `AIRY_SCOPE_DISMISS(resource)` | 撤销作用域守卫 |

清理回调类型为 `airy_resource_cleanup_t`（`void (*)(void *)`），签名不符的函数需经包装或强转。

### 资源追踪（定义 `AIRY_RESOURCE_TRACKING` 后启用，默认关闭）

| 接口 | 说明 |
|---|---|
| `airy_resource_track_alloc(resource, type, file, line)` | 登记一次分配（链表 + 互斥锁，时间戳取 `airy_time_ns()`） |
| `airy_resource_track_free(resource)` | 按指针注销登记 |
| `airy_resource_track_report(out_report)` | 返回未释放条目数；`*out_report` 为新建报告字符串（调用方释放），4096 字节上限，最多列出 100 条，超出汇总为 "... and N more" |
| `airy_resource_track_clear()` | 清空全部登记 |
| `AIRY_TRACKED_MALLOC(size)` / `AIRY_TRACKED_FREE(ptr)` | 追踪版分配/释放；未启用追踪时分别退化为 `AIRY_MALLOC` / `AIRY_FREE` |

未启用 `AIRY_RESOURCE_TRACKING` 时，`AIRY_TRACK_ALLOC` / `AIRY_TRACK_FREE` 为空操作宏。

## 资源配额（resource_quota）

### 数据结构

| 结构 | 字段 |
|---|---|
| `airy_resource_quota_t` | `max_memory_bytes`、`max_cpu_time_ms`、`max_io_ops`、`max_network_bytes`、`timeout_ms` |
| `airy_resource_usage_t` | `current_memory_bytes`、`peak_usage`、`total_cpu_time_ms`、`total_io_ops`、`total_network_bytes`、`start_time`、`last_update`、`operation_count` |
| `airy_resource_manager_t` | 配额 + 用量 + `resource_id`（strdup 副本）+ `lock` + `enabled` + `exceeded_flags` |

超限标志位：MEMORY `0x01`、CPU `0x02`、IO `0x04`、NETWORK `0x08`。

### 接口（按头文件声明）

| 函数 | 说明 |
|---|---|
| `airy_resource_manager_create(quota, resource_id, out_manager)` | 创建管理器（记录 `start_time`） |
| `airy_resource_manager_destroy(manager)` | 销毁（释放 id 与本体） |
| `airy_resource_check_memory(manager, requested_bytes)` | 预计用量超 `max_memory_bytes` 时置 MEMORY 位、WARN 并返回 `AIRY_ENOMEM`；0 字节请求返回 `AIRY_EINVAL` |
| `airy_resource_record_allocation(manager, bytes)` | 记录分配，更新峰值与计数（实现导出符号为 `airy_resource_rec_alloc`，见语义与约束） |
| `airy_resource_record_free(manager, bytes)` | 记录释放；释放超量时钳到 0 并 WARN |
| `airy_resource_record_io(manager)` | 累计 I/O 次数；`total_io_ops ≥ max_io_ops` 时置 IO 位、WARN 并返回 `AIRY_EBUSY` |
| `airy_resource_is_exceeded(manager)` | 任一超限位置位则返回 1 |
| `airy_resource_get_usage(manager, out_usage)` | 拷贝当前用量快照 |
| `airy_resource_get_exceeded_info(manager)` | 超限类型描述字符串（实现导出符号为 `airy_resource_get_exc_info`） |

`enabled = 0` 或 manager 为 NULL 时各检查按「未超限」放行。

## 语义与约束

- **配额函数名错配**：头文件声明的 `airy_resource_record_allocation` / `airy_resource_get_exceeded_info` 与实现导出的 `airy_resource_rec_alloc` / `airy_resource_get_exc_info` 不一致，库内无对应别名——按头声明调用会导致未定义符号；消费方当前均直接使用实现符号名。
- **未生效的配额字段**：`max_cpu_time_ms`、`max_network_bytes`、`timeout_ms` 不参与任何检查逻辑，CPU / NETWORK 超限位在当前实现中永不置位；`usage` 中对应的累计字段也无写入路径。
- **配额非线程安全**：`manager->lock` 恒为 NULL，所有记录/检查操作无锁；`get_exceeded_info` 返回内部 `static` 512 字节缓冲，不可重入。
- **守卫宏的平台性**：`AIRY_SCOPE_GUARD` / `AIRY_SCOPE_EXIT` 依赖 `__attribute__((cleanup))`；MSVC 下 `AIRY_ATTRIBUTE` 展开为空，守卫变量不会自动清理，需显式调用 `airy_resource_guard_cleanup`。`AIRY_SCOPE_DISMISS` 以自身所在行拼接守卫变量名，与定义处行号不一致时将引用失败——跨行移交所有权请改用 `airy_resource_guard_dismiss(&guard)` 配合手工初始化的守卫。
- **熔断器仅存储**：`api_rec_bind_circuit_breaker` 只记录指针，执行路径不调用任何熔断逻辑；`user_context` 字段同理预留未用。
- **恢复池非线程安全**：池结构字段公开且轮转索引为普通变量，多线程共享同一池需外部加锁。
- `api_rec_cred_health` 以负值（`AIRY_EINVAL`）表示参数错误，正常值域为 `[0.0, 1.0]`。

## 用法示例

```c
#include "airy_memory.h"
#include "api_recovery.h"
#include "resource_guard.h"

static void pool_reclaim(void *pool)
{
    api_rec_pool_destroy((api_rec_pool_t *)pool);
}

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
    AIRY_SCOPE_EXIT(pool, pool_reclaim); /* 作用域结束自动销毁 */

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
    return result.rec_code == API_REC_ERR_NONE ? 0 : 1;
}
```

## 构建与依赖

三个 `.c` 均在 `airy_common` 源列表中；`utils/resource/` 已注册 PUBLIC include 路径并整体安装头文件（`include/agentrt/utils/resource/`，排除 `*_internal.h`）。

| 依赖 | 使用方 | 用途 |
|---|---|---|
| commons `utils/error` | api_recovery / quota | `airy_err_t` 与错误码宏 |
| commons `utils/memory` | 三者 | `AIRY_MALLOC/CALLOC/FREE`、strdup 封装 |
| commons `utils/string` | resource_guard | 追踪登记的字符串拷贝 |
| commons `utils/sync`、`atomic_compat.h` | resource_guard | 追踪链表的互斥锁与 once-init |
| commons `platform`（`platform_misc.h`） | api_recovery / guard | `airy_time_ms` / `airy_time_ns` / `airy_random_*` |
| commons `utils/logging`（`svc_logger.h`） | api_recovery | `SVC_LOG_*` 事件日志 |
| commons `utils/observability`（`logger.h`） | resource_quota | `AIRY_LOG_WARN` 超限告警 |
| commons `include/airy_defaults.h` | api_recovery | 健康分/退避默认常量 |
| agentrt `atoms/corekern`（`airy_rt.h`） | resource_quota | 以仓内相对路径 `../../../atoms/...` 引入，使 commons 反向依赖上层原子模块 |

commons 测试套件中本模块仅有 `tests/unit/test_resource_guard.c`（以 mock 资源覆盖守卫生命周期与追踪报告）；api_recovery 与 resource_quota 无专项测试。除 `resource_quota.c` 对 `atoms/corekern` 的头文件引用外，本模块其余组件无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
