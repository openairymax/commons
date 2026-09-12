# cognition — 认知模块公共定义

**模块路径**: `commons/utils/cognition/` · **版本**: 0.1.15

认知层（规划、调度、协调）共享的**公共数据结构与基础操作**：统一定义 Agent 信息、任务信息与三类结果对象，并提供一组简单、确定性的启发式默认实现，供上层 cognition 组件复用而无需重复造轮子。

## 概述

- **纯值语义结构体**：`agent_info_t` / `airy_task_info_t` 与三个结果类型均为公开字段结构，以成对的 `*_init` / `*_cleanup` 管理生命周期；内部字符串经安全分配器复制，由对应的 `cleanup` 统一释放。
- **权重模型**：Agent 权重 = `success_rate × 0.7 + 1/(1 + avg_latency_ms/1000) × 0.3`，`agent_info_update_stats` 在更新统计后自动重算。
- **简单启发式默认实现**：最优选择按权重取最大；计划生成为固定模板文本；结果协调取首个结果的副本。上层如需更复杂的策略可自行实现，本模块只提供最小可用默认。
- **错误契约**：所有 `int` 返回接口在参数非法或内存分配失败时返回 `AIRY_EINVAL`（-22）；全部 `cleanup` 与查询函数对 NULL 安全（返回 0 / 直接返回）。
- **非并发安全**：结构体与函数均无锁，对同一 Agent/结果对象的更新须由调用方串行化。

## 目录结构

```
commons/utils/cognition/
├── README.md
├── cognition_common.h    # 公共类型与接口声明（213 行）
└── cognition_common.c    # 启发式默认实现（393 行）
```

## 数据结构

### `agent_info_t` — Agent 信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `agent_id` | `char *` | Agent 标识（init 时复制，归结构体所有） |
| `weight` | `double` | 当前权重，初值 1.0，随统计更新重算 |
| `success_rate` | `double` | 历史成功率（成功数 / 总任务数） |
| `total_tasks` / `successful_tasks` | `uint64_t` | 总任务数 / 成功任务数 |
| `avg_latency` | `double` | 平均延迟（毫秒），增量算术平均 |
| `last_used` | `uint64_t` | 最后使用时间（毫秒时钟） |

### `airy_task_info_t` — 任务信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `task_id` / `task_type` / `task_content` | `char *` | 任务标识 / 类型 / 内容（均复制持有） |
| `priority` | `uint64_t` | 优先级，init 后为 0 |
| `deadline` | `uint64_t` | 截止时间（毫秒时钟），0 表示无截止 |

### 结果类型

| 类型 | 载荷字段 | 附带字段 |
|------|----------|----------|
| `plan_result_t` | `plan` / `plan_size` | `success`、`error` / `error_size` |
| `dispatch_result_t` | `selected_agent`、`confidence` | `success`、`error` / `error_size` |
| `coordination_result_t` | `decision` / `decision_size` | `success`、`error` / `error_size` |

## 接口

### 对象生命周期

| 函数 | 语义 |
|------|------|
| `agent_info_init(agent, agent_id)` | 复制 ID，`weight=1.0`、`last_used` 置当前毫秒时钟；NULL 或分配失败返回 `AIRY_EINVAL`。 |
| `agent_info_cleanup(agent)` | 释放 ID 并清零全部字段，接受 NULL。 |
| `agent_info_update_stats(agent, success, latency)` | 累加计数、重算成功率与平均延迟、刷新 `last_used`，并自动重算 `weight`；接受 NULL（无操作）。 |
| `agent_info_calculate_weight(agent)` | 返回按上式计算的权重；NULL 返回 0.0。 |
| `task_info_init(task, id, type, content)` | 复制三个字符串（部分失败自动回滚），`priority`/`deadline` 置 0；任一参数 NULL 或分配失败返回 `AIRY_EINVAL`。 |
| `task_info_cleanup(task)` | 释放三个字符串并清零，接受 NULL。 |
| `{plan,dispatch,coordination}_result_init` | 将结果置为全空初始态（`success=false`、指针 NULL）；NULL 返回 `AIRY_EINVAL`。 |
| `{plan,dispatch,coordination}_result_cleanup` | 释放载荷与错误字符串并清零，接受 NULL。 |

### 调度、计划与协调

| 函数 | 语义 |
|------|------|
| `cognition_select_best_agent(agents, count, task, result)` | 按 `weight` 取最大者（并列取序号靠前者），`selected_agent` 为其 ID 副本，`confidence` 即该权重。`task` 仅参与入参校验；分配失败时写入 `error` 并返回 `AIRY_EINVAL`。 |
| `cognition_generate_plan(task, result)` | 生成模板文本 `"Execute task: <task_content>"`，`plan_size` 为字符数；`plan` 由 `plan_result_cleanup` 释放。 |
| `cognition_coordinate_results(agent_results, count, result)` | 以 `agent_results[0]` 的副本作为 `decision`；`count` 为 0 或参数 NULL 返回 `AIRY_EINVAL`。 |
| `cognition_calculate_task_priority(task)` | 按类型映射 `emergency`=100、`urgent`=80、`normal`=50、`low`=20（其余 0）；`deadline>0` 且剩余时间不足 1 小时加 30、不足 24 小时加 15。剩余时间按无符号毫秒差计算，截止时间已过不会触发加分。NULL 返回 0。 |
| `cognition_evaluate_plan_quality(plan, task)` | 基础 50 分，计划文本包含任务内容加 30、长度超过 10 字符再加 20，封顶 100；任一参数 NULL 返回 0。 |

所有动态字符串（ID、`plan`、`selected_agent`、`decision`、`error`）均由本模块分配、由对应 `cleanup` 释放；调用方不应对其单独 `free`。

## 用法

```c
#include <cognition_common.h>

bool pick_agent(agent_info_t *agents, size_t count,
                const airy_task_info_t *task,
                dispatch_result_t *picked)
{
    bool ok = false;

    agent_info_update_stats(&agents[0], true, 120); /* 记录一次成功执行 */

    if (dispatch_result_init(picked) != 0) {
        return false;
    }
    if (cognition_select_best_agent(agents, count, task, picked) == 0) {
        ok = picked->success; /* picked->selected_agent 由 cleanup 释放 */
    }
    if (!ok) {
        dispatch_result_cleanup(picked);
    }
    return ok;
}
```

## 构建与依赖

`cognition_common.c` 编译进静态库 `airy_common`；`utils/cognition/` 作为公共 include 目录（PUBLIC 导出），头文件随库安装（`*.h`，排除 `*_internal.h`）。

| 依赖 | 类型 | 用途 |
|------|------|------|
| memory（`memory_common.h`） | 同库模块 | 字符串与载荷的安全分配 / 复制 / 释放 |
| platform（`platform.h`） | 同库模块 | `airy_time_ns()` 毫秒时钟（`last_used`、截止时间计算） |
| C 标准库 | 外部 | `string.h` / `stdio.h` / `math.h` |

本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
