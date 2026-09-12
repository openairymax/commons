# strategy — 策略通用工具

**模块路径**: `commons/utils/strategy/` · **版本**: 0.1.15

为「多候选加权打分选最优」这一常见决策形态提供最小共享实现：三维（成本/性能/信任）加权评分、最优候选选择、权重校验与归一化、策略命名拼接。编译进 commons 静态库 `airy_common`。

## 概述

模块只有一对文件、七个函数，定位是消除各调用方自行实现打分循环的重复代码，而非完整的策略引擎：评分维度固定为三个，无插件式评分函数，也不提供调度或规划框架本身。

## 目录结构

```
strategy/
├── strategy_common.h      # 数据结构与 7 个函数声明
├── strategy_common.c      # 实现
└── README.md
```

## 数据结构

| 结构 | 字段 | 说明 |
|------|------|------|
| `weighted_config_t` | `cost_weight` / `perf_weight` / `trust_weight`（`float`） | 三维权重 |
| `strategy_agent_info_t` | `cost_estimate`、`success_rate`、`trust_score`（`float`），`name`（`const char *`），`user_data`（`void *`） | 单个候选的输入指标 |
| `strategy_result_t` | `selected_index`（`int`）、`best_score`（`float`）、`success`（`bool`） | 选择结果 |

## 接口

| 函数 | 说明 |
|------|------|
| `strategy_compute_weighted_score(agent, config)` | 计算单个候选的加权分；任一入参为 NULL 时返回 `0.0f` |
| `strategy_select_best_agent(agents, count, config, result)` | 遍历候选取最高分；参数非法或 `count == 0` 返回 `AIRY_EINVAL`，成功返回 0 |
| `strategy_create_default_weighted_config()` | 返回默认权重 `0.33 / 0.34 / 0.33` |
| `strategy_validate_weighted_config(config)` | 各权重是否都落在 `[0.0, 1.0]` |
| `strategy_normalize_weights(config)` | 按三权重之和缩放；和 ≤ 0 时返回默认配置 |
| `strategy_cleanup_data(data, free_func)` | 两者非 NULL 时调用 `free_func(data)` |
| `strategy_generate_name(base, suffix)` | 生成 `base_suffix`（`suffix` 为 NULL/空时仅 `base`），返回调用方以 `AIRY_FREE` 释放的副本；`base` 为 NULL 返回 NULL |

## 语义与约束

- 评分公式为固定三维：

  ```
  score = cost_weight × 1/(cost_estimate + 1) + perf_weight × success_rate + trust_weight × trust_score
  ```

  成本以倒数形式参与（值越大得分越低），不做上下界裁剪：`cost_estimate` 接近 `-1` 或为负数会产生异常得分，权重亦未强制归一，需调用方自行保证输入合理。
- 选择时以严格大于比较分数，平局取索引最小者。
- 归一化是显式调用 `strategy_normalize_weights` 完成的，`strategy_select_best_agent` 不会自动归一化；该函数对 NULL 入参无防护（解引用后行为未定义），结果也不保证权重和恰为 1.0（浮点）。
- 无内部状态，所有函数可并发调用。

## 用法示例

```c
#include "strategy_common.h"
#include "airy_memory.h"

/* 从三个候选节点中选出综合得分最高者 */
int pick_node(const char *preferred_suffix)
{
    strategy_agent_info_t nodes[] = {
        { .cost_estimate = 0.3f, .success_rate = 0.95f, .trust_score = 0.8f, .name = "node-1" },
        { .cost_estimate = 0.5f, .success_rate = 0.90f, .trust_score = 0.9f, .name = "node-2" },
        { .cost_estimate = 0.2f, .success_rate = 0.85f, .trust_score = 0.7f, .name = "node-3" },
    };
    weighted_config_t cfg = { .cost_weight = 0.3f, .perf_weight = 0.4f, .trust_weight = 0.3f };
    strategy_result_t res;
    char *label = NULL;
    int ret = 0;

    if (!strategy_validate_weighted_config(&cfg)) {
        return -1;
    }

    if (strategy_select_best_agent(nodes, sizeof(nodes) / sizeof(nodes[0]), &cfg, &res) != 0) {
        return -1;
    }

    label = strategy_generate_name(nodes[res.selected_index].name, preferred_suffix);
    if (label) {
        AIRY_FREE(label); /* 名称交调用方决定去向 */
    }
    return res.success ? ret : -1;
}
```

## 构建与依赖

`strategy_common.c` 由 `commons/CMakeLists.txt` 列入 `airy_common` 源列表，`utils/strategy` 注册为 PUBLIC include 目录，头文件随 `install` 导出。链接 `airy_common` 即可使用。

| 依赖 | 用途 |
|------|------|
| `utils/memory`（`airy_memory.h`） | `strategy_generate_name` 的分配（`AIRY_MALLOC`/`AIRY_FREE`） |
| `utils/string`（`string_compat.h`） | 头文件引入的字符串兼容层 |
| `utils/error`（`error.h`） | `AIRY_EINVAL`、`AIRY_ERROR_NULL` 错误契约 |

本模块无 agentrt 内部上游依赖。commons 中当前没有其他模块调用本模块，也未纳入 commons 单元测试套件。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
