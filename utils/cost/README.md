# cost — 成本估算与预算控制

**模块路径**: `commons/utils/cost/` · **版本**: 0.1.15

按模型单价估算 LLM 调用成本，并提供带周期窗口的预算控制器。两个组件均为不透明句柄 API，可独立使用。

## 概述

- **估算器（estimator）**：维护一张模型价表（内置 9 个模型的默认单价 + 未知模型兜底项），按 `(tokens / 1000) × 每 1K 单价` 计算单次调用成本（USD），并累计总成本、输入/输出 token 数与请求次数。
- **预算控制器（budget controller）**：以墙钟时间划分的周期窗口跟踪消耗，超过周期预算的请求被拒绝；另有一条总累计上限（周期预算的 100 倍）。
- 价表匹配对模型名做**归一化**（剔除空白并转小写），比对时大小写不敏感。
- 自定义价目通过**逐行逗号分隔文本**加载（`模型名,输入单价,输出单价,最大输入,最大输出`，支持 `#` 注释），**不依赖任何 YAML 解析库**。
- 两者的累计状态变更均由互斥锁保护；价表读取不加锁，避免在估算进行中并发增删价表条目。
- 内置单价为示例性默认值，不代表任何提供商的实时计费；实际计费请以供应商价目为准并通过 `add_model` 或配置文件录入。

## 目录结构

```
cost/
├── cost.h         (182 行)  公开接口：estimator 与 budget controller 两族 API
├── estimator.c    (348 行)  价表、配置文件解析、估算与统计累计
└── controller.c   (326 行)  周期预算、消耗判定与状态查询
```

## 数据结构

两类型均为不透明句柄（`typedef struct airy_cost_estimator airy_cost_estimator_t;` / `typedef struct airy_budget_controller airy_budget_controller_t;`），仅经 API 操作，由 `AIRY_MALLOC` 分配、对应 `*_destroy` 释放。

## 接口 — 成本估算器

| 函数 | 语义 |
|------|------|
| `airy_cost_estimator_create(config_path)` | 创建估算器并载入内置价表；`config_path` 非空时追加加载该文件条目；失败返回 `NULL` |
| `airy_cost_estimator_destroy(estimator)` | 销毁（`NULL` 安全） |
| `airy_cost_estimator_estimate(estimator, model, in_tokens, out_tokens)` | 计算单次成本**并累计进统计**；失败返回负错误码（`AIRY_EINVAL`） |
| `airy_cost_estimator_get_total(estimator)` | 累计总成本（USD）；`NULL` 返回 `0.0` |
| `airy_cost_estimator_get_input_tokens(estimator)` | 累计输入 token 数 |
| `airy_cost_estimator_get_output_tokens(estimator)` | 累计输出 token 数 |
| `airy_cost_estimator_get_request_count(estimator)` | 估算调用次数 |
| `airy_cost_estimator_reset(estimator)` | 清零全部统计（价表保留） |
| `airy_cost_estimator_add_model(estimator, model, in_per_1k, out_per_1k)` | 追加价表条目（该条目上下文长度默认 4096/4096）；成功 `0`，容量已满或参数非法返回 `AIRY_EINVAL` |

价表容量上限 16 条（内置 10 条 + 自定义 6 条余量）。查找为**首个匹配生效**：与内置模型同名的自定义条目不会改变其计价，仅能新增模型。配置文件读取失败不影响句柄返回。

## 接口 — 预算控制器

| 函数 | 语义 |
|------|------|
| `airy_budget_controller_create(max_cost_usd, period_seconds)` | 创建控制器；`max_cost_usd <= 0` 返回 `NULL` |
| `airy_budget_controller_destroy(controller)` | 销毁（`NULL` 安全） |
| `airy_budget_controller_consume(controller, cost_usd)` | 尝试消耗：成功返回 `0`；本周期消耗超过警告阈值（默认 0.8）返回 `1`；超预算或参数非法被拒绝，返回负错误码（`AIRY_EINVAL`），被拒请求不产生消耗 |
| `airy_budget_controller_remaining(controller)` | 本周期剩余预算，最小钳 `0.0` |
| `airy_budget_controller_consumed(controller)` | 自创建以来的累计消耗（跨周期） |
| `airy_budget_controller_period_consumed(controller)` | 当前周期已消耗 |
| `airy_budget_controller_requests(controller)` | 被接受的消耗次数 |
| `airy_budget_controller_denied(controller)` | 被拒绝的消耗次数 |
| `airy_budget_controller_set_warning(controller, threshold)` | 设置警告阈值，须落在 `(0, 1.0]`；成功 `0`，否则 `AIRY_EINVAL` |
| `airy_budget_controller_reset_period(controller)` | 立即清零周期消耗并重启周期窗口 |
| `airy_budget_controller_average(controller)` | 平均单次消耗 = 累计消耗 / 被接受请求数 |
| `airy_budget_controller_get_status(controller)` | 按本周期占比返回 `0` 正常 / `1` 达到警告阈值 / `2` 达到或超过上限；`NULL` 返回 `AIRY_EINVAL` |

周期滚动为**惰性检查**：在 `consume` 与查询时比较当前时间与窗口起点，到期即清零周期消耗并重开窗口；无后台定时器。除周期上限外另有一条宽松的全局上限（累计消耗不得超过周期预算的 100 倍），触发后同样拒绝并计入 denied。

## 内置价表（USD / 1K tokens）

| 模型 | 输入 | 输出 |
|------|------|------|
| `gpt-4o` | 0.005 | 0.015 |
| `gpt-4-turbo` | 0.01 | 0.03 |
| `gpt-4` | 0.03 | 0.06 |
| `gpt-3.5-turbo` | 0.0005 | 0.0015 |
| `claude-3-opus` | 0.015 | 0.075 |
| `claude-3-sonnet` | 0.003 | 0.015 |
| `claude-3-haiku` | 0.00025 | 0.00125 |
| `deepseek-chat` | 0.00014 | 0.00028 |
| `deepseek-coder` | 0.00014 | 0.00028 |
| 未匹配模型（兜底） | 0.001 | 0.002 |

## 用法示例

```c
#include <cost.h>

/* 估算一次调用并把结果作为预算门禁的输入 */
double quote_and_charge(airy_cost_estimator_t *est,
                        airy_budget_controller_t *bc,
                        const char *model, size_t in_tok, size_t out_tok)
{
    double cost = airy_cost_estimator_estimate(est, model, in_tok, out_tok);
    if (cost < 0.0) {
        return -1.0; /* 参数非法 */
    }

    int rc = airy_budget_controller_consume(bc, cost);
    if (rc < 0) {
        /* 超出周期预算：请求被拒，cost 未被消耗 */
        return -1.0;
    }
    /* rc == 1：已消耗，但本周期用量越过警告阈值 */

    return cost; /* airy_cost_estimator_estimate 已计入累计 */
}
```

统计数据经 `airy_cost_estimator_get_total` 等 getter 查询，`airy_cost_estimator_reset` 清零；预算侧被拒计数经 `airy_budget_controller_denied` 观测。

## 构建与依赖

`estimator.c` 与 `controller.c` 随 `airy_common` 静态库编译；`cost.h` 随库以 PUBLIC 方式导出（`include/agentrt/utils/cost/`，安装规则排除 `*_internal.h`）。

| 依赖 | 用途 |
|------|------|
| `platform` | `airy_mtx_*` 跨平台互斥锁 |
| `sync/atomic`（`atomic_compat.h`） | `atomic_double` / `atomic_uint64_t` 计数 |
| `memory` | `AIRY_MALLOC` / `AIRY_MEMSET` / `AIRY_STRNCPY_TERM` |
| `string`（`string_compat.h`） | `strcasecmp` / `strtok_r` 兼容 |
| `error` | `AIRY_EINVAL`、`AIRY_ERROR_NULL` 错误契约 |
| 标准库 | `stdio`（配置文件逐行解析）、`stdlib`、`string`、`ctype`、`time` |

本模块无 agentrt 内部上游依赖，无 YAML 等外部第三方依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*

*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
