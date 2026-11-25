# execution — 任务检查点（checkpoint）

**模块路径**: `commons/utils/execution/` · **版本**: 0.1.21

将任务执行现场（状态 JSON + 元数据）按「任务 ID + 序号」写入文件，供外部工具查看与过期清理；运行期重建任务现场由 CoreLoopThree 自身的会话状态承担，本模块不再提供读回路径。

## 概述

- 检查点以**进程级全局单例**形式管理：一个存储目录、一份统计、一把全局锁；不支持多实例。
- 每个检查点文件对应一个 `(task_id, sequence_num)` 组合，文件名为 `checkpoint_<task_id>_<seq>.json`，同任务不同序号互不覆盖。
- **落盘内容**为标量字段 + `state_json` 字符串；内存中的已完成/待办**节点名数组不持久化**。
- 写入采用 **临时文件 + rename** 方式，避免读到半截文件。
- 未调用 `airy_checkpoint_init` 前，其余接口一律返回 `AIRY_ENOTINIT`。

## 目录结构

```
execution/
├── checkpoint.h            公开接口（AIRY_API 导出）
├── checkpoint_internal.h   内部共享定义（不安装）
├── checkpoint.c            生命周期、创建/销毁、全局状态与内部助手
├── checkpoint_persist.c    保存：路径/序号解析、目录扫描、JSON 序列化
└── checkpoint_session.c    任务 ID 列举、过期清理、自动检查点钩子
```

## 数据结构

| 类型 | 说明 |
|------|------|
| `airy_checkpoint_state_t` | `PENDING` / `COMPLETED` / `FAILED` / `INVALID` 四态枚举（以字符串形式随文件持久化） |
| `airy_task_checkpoint_t` | 公开 POD：task_id/session_id（各 128 字节内联）、sequence_num、timestamp（纳秒）、`state_json`（堆持有）与 state_size、completed/pending 节点数组（仅内存）、checksum、metadata（512 字节内联） |
| `airy_checkpoint_stats_t` | 累计检查点数、成功/失败数、最近检查点时间、平均大小（增量均值） |
| `airy_checkpoint_hook_fn` | 自动检查点回调 `(task_id, state_json, user_data)` |

## 接口

| 函数 | 语义 |
|------|------|
| `airy_checkpoint_init(storage_path)` | 初始化存储目录；`NULL` 时用数据目录下 `checkpoints/`；重复调用幂等返回成功 |
| `airy_checkpoint_shutdown(void)` | 复位全局状态并清除已注册钩子 |
| `airy_checkpoint_create(task_id, session_id, seq, state_json, completed, c_cnt, pending, p_cnt, &out)` | 构造内存检查点：复制字符串与节点数组，初始状态 `PENDING`，按 `state_json` 计算 checksum；失败不产出句柄 |
| `airy_checkpoint_save(cp)` | 写入文件并将 `cp->state` 置 `COMPLETED`、更新统计；I/O 失败返回 `AIRY_EIO` |
| `airy_checkpoint_ids(&out_ids, &out_count)` | 目录扫描列出存在检查点的 distinct task_id；结果由调用方逐个释放后释放数组 |
| `airy_checkpoint_get_stats(&stats)` | 快照式读取全局统计 |
| `airy_checkpoint_cleanup(max_age_seconds, max_count)` | 按文件修改时间删除过期 `*.json`；数量上限仅钳制统计计数器，不额外删除文件 |
| `airy_checkpoint_destroy(cp)` | 释放句柄及其持有的字符串/节点数组（`NULL` 安全） |
| `airy_checkpoint_set_auto_hook(hook, user_data, interval_ms)` | 注册/清除（`hook` 传 `NULL`）自动检查点回调；本模块不含定时器，`interval_ms` 仅为调用方调度参考 |
| `airy_checkpoint_trigger_auto(task_id)` | 由调用方择机触发已注册回调（`state_json` 实参为 `NULL`） |

## 语义与约束

- **写入即终点**：本模块只负责落盘与目录可见性；恢复由调用方以自己的会话状态机制承担，避免同族接口出现「只存不取」的悬空承诺。
- **文件名解析单一真相**：`checkpoint_{task_id}_{seq}.json` 中序号为最后一个 `_` 之后的纯数字段，task_id 可含 `_`；所有目录扫描共用同一解析器。
- **时间基准**：文件时间戳用墙钟秒比较；结构体 `timestamp` 为 `airy_time_ns()` 纳秒值。
- **并发**：统计与清理等临界区由全局锁保护；目录扫描不加锁。

## 用法示例

```c
#include <checkpoint.h>

/* 初始化 → 创建一个任务检查点并落盘 */
airy_err_t save_once(const char *task_id)
{
    airy_err_t err = airy_checkpoint_init(NULL);
    if (err != AIRY_SUCCESS) {
        return err;
    }

    airy_task_checkpoint_t *cp = NULL;
    err = airy_checkpoint_create(task_id, "s-01", 1, "{\"step\": 12}",
                                 NULL, 0, NULL, 0, &cp);
    if (err != AIRY_SUCCESS) {
        return err;
    }
    err = airy_checkpoint_save(cp);
    airy_checkpoint_destroy(cp);
    return err;
}
```

## 构建与依赖

三个 `.c` 随 `airy_common` 静态库编译；`checkpoint.h` 以 PUBLIC 方式导出（`include/agentrt/utils/execution/`，安装规则排除 `*_internal.h`）。

| 依赖 | 用途 |
|------|------|
| `platform` | `AIRY_API` 导出宏、`airy_time_ns`、`airy_mtx_*`、`airy_data_dir` 数据目录 |
| `error` | `airy_err_t` 与 `AIRY_SUCCESS` / `AIRY_EINVAL` / `AIRY_ENOMEM` / `AIRY_ENOTINIT` / `AIRY_EIO` 等错误契约 |
| `logging` | 运行日志 |
| `memory` / `sync` | `AIRY_MALLOC` 族、`airy_mtx_*`、`atomic_*` |
| `compat`（`airy_dirent.h`） | Windows 目录遍历垫片 |
| 标准库 | `stdio` / `stdlib` / `string` / `time` |

跨 Linux/macOS/Windows。本模块无 agentrt 内部上游依赖，无第三方 JSON/YAML 依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*

*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
