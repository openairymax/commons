# token — Token 计数与预算管理

**模块路径**: `commons/utils/token/` · **版本**: 0.1.15

为 LLM 应用提供轻量的 token 估算、文本截断与预算/配额控制。全部实现为字符级启发式算法，不引入任何第三方编码器或外部依赖。

## 概述

- **两类不透明对象**：`airy_token_counter_t`（按模型名估算与截断）与 `airy_token_budget_t`（输入/输出 token 预算记账与时间窗口滚动），对象内部加锁，操作线程安全。
- **标准化估算层**（`token_standard.h`）：`airy_token_standard_count()` 及批量/文本分析/配置校验/精度切换/配额检查接口，可独立于计数器对象使用。
- **算法为字符级启发式**：UTF-8 严格解码后统计 CJK 字符、拉丁字母与总码点数，按比例模型折算。它不是 BPE 编码器，结果用于预算控制与预检，不保证与任一模型服务端逐 token 一致；需要精确计数时应接入目标模型的专用编码器。
- **错误契约**：返回指针的接口失败时返回 `NULL` 并将负值错误码压入 error 栈（见 [error 模块](../error/README.md)）；返回 `size_t` 的计数接口以 `(size_t)-1` 表示参数或配置错误；返回 `int` 的接口以 `AIRY_EINVAL`（-22）等 POSIX 风格负码表示失败。
- 截断结果由 memory 模块的 `AIRY_*` 分配器分配，使用完毕须以 `AIRY_FREE()` 释放。

## 目录结构

```
commons/utils/token/
├── README.md
├── token.h             # 计数器与预算 API
├── token_standard.h    # 标准化估算 / 精度 / 配额 API
├── counter.c           # 计数、批量与截断实现
├── budget.c            # 预算记账与时间窗口实现
└── token_standard.c    # 标准化估算算法实现
```

## 类型与配置（token_standard.h）

| 类型 | 说明 |
|------|------|
| `airy_token_model_t` | 模型族：`GENERIC`(0) / `GPT4` / `GPT35` / `CLAUDE` / `LLAMA` / `CUSTOM` |
| `airy_token_config_t` | `{ model_type, model_name, cjk_ratio, alpha_ratio, flags }` |
| `airy_token_precision_t` | 精度档：`LOW` / `MEDIUM` / `HIGH` |
| `airy_token_quota_t` | 七项限额：单次请求、每分钟/时/日的 token 数与请求数 |
| `airy_token_usage_t` | 六项已用量：每分钟/时/日的 token 数与请求数 |
| `AIRY_TOKEN_ALGORITHM_VERSION` | 算法版本字符串 `"1.0"` |

配置标志（可组合，但 `ACCURATE` 与 `ESTIMATE` 互斥，同时置位视为非法配置）：

| 标志 | 值 | 含义 |
|------|----|------|
| `AIRY_TOKEN_FLAG_ACCURATE` | 0x01 | 按模型族固定折算率估算 |
| `AIRY_TOKEN_FLAG_ESTIMATE` | 0x02 | 按 CJK/字母占比阈值估算（默认） |
| `AIRY_TOKEN_FLAG_INCLUDE_BOM` | 0x04 | 预留标志，当前估算路径未对其做特殊处理 |

`AIRY_TOKEN_CONFIG_DEFAULT`：`GENERIC` + `ESTIMATE`，`cjk_ratio = 0.3`，`alpha_ratio = 0.5`。
`AIRY_TOKEN_QUOTA_DEFAULT`：单次 8000 token；每分钟/时/日 60000 / 360000 / 2000000 token；每分钟/时/日 60 / 3600 / 10000 次请求。

## 计数器 API（token.h）

| 函数 | 语义 |
|------|------|
| `airy_token_counter_create(model_name)` | 创建计数器。`model_name` 按关键字匹配模型族：含 `gpt-4`/`gpt-4o` → GPT-4 类，`gpt-35`/`gpt-3.5` → GPT-3.5 类，`claude` → Claude 类，`llama`/`vicuna`/`alpaca` → Llama 类，其余按通用模型。参数为空或分配失败返回 `NULL`（错误入栈） |
| `airy_token_counter_destroy(counter)` | 销毁，接受 `NULL` |
| `airy_token_counter_count(counter, text)` | 估算单条文本 token 数；空串返回 0；参数为 `NULL` 返回 `(size_t)-1` |
| `airy_token_counter_count_batch(counter, texts, count, out_counts)` | 逐项写入 `out_counts`（`NULL` 元素计 0）；成功返回 0，参数错误返回 `(size_t)-1` |
| `airy_token_counter_truncate(counter, text, max_tokens, side)` | 截断到约 `max_tokens`，返回**新分配**文本（`AIRY_FREE` 释放），失败返回 `NULL`。`side`：`"left"` 保留尾部、`"middle"` 保留首尾、其他值或 `NULL` 保留头部 |

截断按字节数折算目标长度（`原字节长 × max_tokens ÷ 当前估算`），在字节边界裁切并追加 `...`（中段为 `...[truncated]...`），不保证落在 UTF-8 字符边界上；文本本就不超时返回全文副本。

## 预算 API（token.h）

| 函数 | 语义 |
|------|------|
| `airy_token_budget_create(max_tokens)` | 创建预算；`max_tokens == 0` 或分配失败返回 `NULL`（错误入栈） |
| `airy_token_budget_destroy(budget)` | 销毁，接受 `NULL` |
| `airy_token_budget_add(budget, input, output)` | 记账一次请求。成功返回 0；余额不足返回 `AIRY_EINVAL` 且拒绝计数 +1；参数为 `NULL` 同样返回 `AIRY_EINVAL` |
| `airy_token_budget_remaining(budget)` | 剩余可用 token 数（耗尽时为 0） |
| `airy_token_budget_used` / `_input` / `_output` | 已用总量 / 输入量 / 输出量 |
| `airy_token_budget_requests` / `_denied` | 成功记账次数 / 被拒绝次数（`uint32_t`） |
| `airy_token_budget_reset(budget)` | 清零 `used/input/output` 三项用量；`requests`、`denied` 不受影响 |
| `airy_token_budget_set_window(budget, seconds)` | 设定时间窗口，起算点为调用时刻 |
| `airy_token_budget_check_window(budget)` | 惰性滚窗：当前时间越过窗口终点时清零用量并顺延一个窗口；未设置窗口时不做任何事。返回 0（`budget` 为 `NULL` 返回 `AIRY_EINVAL`） |

## 标准化估算 API（token_standard.h）

| 函数 | 语义 |
|------|------|
| `airy_token_standard_count(text, length, config)` | 估算 token 数。`length == 0` 时自动 `strlen`；`config == NULL` 用默认配置；空串返回 0；`text == NULL` 或配置非法返回 `(size_t)-1`；非空文本至少计 1 |
| `airy_token_standard_count_batch(texts, lengths, count, out_counts, config)` | 批量估算；`lengths` 可为 `NULL`（逐项自动求长），`NULL` 文本元素计 0；成功返回 0，参数、配置错误或单项计数失败返回 `AIRY_EINVAL`（`count == 0` 亦视为参数错误） |
| `airy_token_analyze_text(text, length, &cjk, &alpha, &total)` | 输出 CJK 字符数、拉丁字母数、总码点数；非法字节序列逐字节跳过、不计入总数 |
| `airy_token_get_algorithm_info()` | 返回算法说明字符串 |
| `airy_token_validate_config(config)` | 校验配置：`model_type` 在枚举范围内、两个 ratio 均为开区间 (0,1)、`ACCURATE` 与 `ESTIMATE` 不同时置位 |
| `airy_token_set_precision(precision, config)` | 按档位写入配置：`LOW` → ESTIMATE 0.3/0.5，`MEDIUM` → ESTIMATE 0.2/0.4，`HIGH` → ACCURATE 0.1/0.3 |
| `airy_token_check_quota(quota, requested, usage)` | 逐级检查限额，见返回值表 |

`airy_token_check_quota()` 返回值（`quota` 为 `NULL` 返回 `AIRY_EINVAL`；`usage` 为 `NULL` 时仅检查单次限额；限额为 0 表示该项不检查）：

| 值 | 含义 | 值 | 含义 |
|----|------|----|------|
| 0 | 限额充足 | 4 | 超出每日 token 限额 |
| 1 | 超出单次请求 token 限额 | 5 | 超出每分钟请求限额 |
| 2 | 超出每分钟 token 限额 | 6 | 超出每小时请求限额 |
| 3 | 超出每小时 token 限额 | 7 | 超出每日请求限额 |

## 估算算法

`airy_token_standard_count()` 先经 UTF-8 严格解码（拒绝过长编码、代理区与 U+10FFFF 之外的码点），按 16 个 Unicode 区段识别 CJK（含汉字扩展区、假名、谚文音节、CJK 标点与全角形式），再按配置的标志折算：

- **ESTIMATE 模式**（默认，亦是计数器的内部路径）：CJK 字符占比超过 `cjk_ratio` 时按 `总码点 ÷ 1.5`；否则拉丁字母占比超过 `alpha_ratio` 时按 `总码点 ÷ 4`；其余按 `总码点 ÷ 3`。
- **ACCURATE 模式**：GPT-4 / GPT-3.5 按 `CJK ÷ 1.5 + 非 CJK ÷ 4`；Claude 按 `总码点 ÷ 3.5`；Llama 按 `CJK ÷ 2 + 非 CJK ÷ 4`；通用模型回落到与 ESTIMATE 相同的阈值分支（CJK 主导时为 `CJK ÷ 1.5 + 非 CJK ÷ 4`）。

两种模式的折算率均为经验常数，仅用于量级估算。

## 用法示例

### 估算与截断

```c
#include "token.h"
#include "airy_memory.h" /* AIRY_FREE */

airy_token_counter_t *counter = airy_token_counter_create("gpt-4o");

if (counter) {
    size_t n = airy_token_counter_count(counter, "The quick brown fox");

    const char *texts[2] = {"hello world", "你好，世界"};
    size_t counts[2];
    if (airy_token_counter_count_batch(counter, texts, 2, counts) == 0) {
        /* counts[0]、counts[1] 为逐项估算值 */
    }

    if (n != (size_t)-1 && n > 64) {
        char *cut = airy_token_counter_truncate(counter,
                                                "The quick brown fox ...",
                                                64, "middle");
        if (cut) {
            /* 使用截断文本 ... */
            AIRY_FREE(cut);
        }
    }

    airy_token_counter_destroy(counter);
}
```

### 预算与时间窗口

```c
#include "token.h"

airy_token_budget_t *budget = airy_token_budget_create(100000);

if (budget) {
    airy_token_budget_set_window(budget, 3600); /* 每小时滚动 */

    airy_token_budget_check_window(budget);     /* 到期自动清零并顺延 */

    if (airy_token_budget_add(budget, 1200, 300) == 0) {
        /* 记账成功 */
    } else {
        /* 余额不足：返回 AIRY_EINVAL，denied 计数 +1 */
    }

    if (airy_token_budget_remaining(budget) > 0) {
        /* 仍有预算 */
    }

    airy_token_budget_destroy(budget);
}
```

## 构建与依赖

源文件 `counter.c`、`budget.c`、`token_standard.c` 随 commons 一并编入静态库 `airy_common`；`token.h` 与 `token_standard.h` 随库安装并作为 PUBLIC include 目录导出。

```cmake
target_link_libraries(<your_target> PRIVATE airy_common)
```

| 依赖 | 用途 |
|------|------|
| [memory](../memory/README.md) | `AIRY_MALLOC` / `AIRY_STRDUP` / `AIRY_FREE` 分配契约 |
| [error](../error/README.md) | `AIRY_EINVAL` 负值错误码与 error 栈（`AIRY_ERROR_NULL`） |
| [compat](../compat/README.md) | `string_compat.h` 安全字符串、`atomic_compat.h` 原子类型兜底 |
| [platform](../../platform/README.md) | `airy_mtx_t` 及锁原语（预算内部记账锁） |
| C 标准库 | `ctype` / `string` / `time` / `stdatomic` |

本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
