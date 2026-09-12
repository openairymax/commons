# execution — 任务检查点（checkpoint）

**模块路径**: `commons/utils/execution/` · **版本**: 0.1.15

将任务执行现场（状态 JSON + 元数据）按「任务 ID + 序号」持久化为文件，支持恢复、列举、过期清理与快照导出，用于中断后重建任务现场。

## 概述

- 检查点以**进程级全局单例**形式管理：一个存储目录、一份统计、一把全局锁；不支持多实例。
- 每个检查点文件对应一个 `(task_id, sequence_num)` 组合，文件名为 `checkpoint_<task_id>_<seq>.json`，同任务不同序号互不覆盖。
- **落盘内容**为标量字段 + `state_json` 字符串（自定义的简化 JSON 文本，逐键提取解析，不依赖第三方 JSON 库）；内存中的已完成/待办**节点名数组不持久化**，恢复后不可用。
- 写入采用 **临时文件 + rename** 方式，避免读到半截文件。
- 未调用 `airy_checkpoint_init` 前，其余接口一律返回 `AIRY_ENOTINIT`。

## 目录结构

```
execution/
├── checkpoint.h             (94 行)  公开接口（AIRY_API 导出）
├── checkpoint_internal.h    (59 行)  内部共享定义（不安装）
├── checkpoint.c            (327 行)  生命周期、创建/校验/销毁、全局状态与内部助手
├── checkpoint_persist.c    (546 行)  保存与恢复：路径/序号解析、目录扫描、JSON 读写
├── checkpoint_session.c    (247 行)  删除、列举、过期清理、自动检查点钩子
└── checkpoint_snapshot.c   (134 行)  SNAPSHOT_V1 文本格式导出/回读
```

## 数据结构

| 类型 | 说明 |
|------|------|
| `airy_checkpoint_state_t` | `PENDING` / `COMPLETED` / `FAILED` / `INVALID` 四态枚举（以字符串形式随文件持久化） |
| `airy_task_checkpoint_t` | 公开 POD：task_id/session_id（各 128 字节内联）、sequence_num、timestamp（纳秒）、`state_json`（堆持有）与 state_size、completed/pending 节点数组（仅内存）、checksum、metadata（512 字节内联） |
| `airy_checkpoint_stats_t` | 累计检查点数、成功/失败数、恢复操作数、最近检查点时间、平均大小（增量均值） |
| `airy_checkpoint_hook_fn` | 自动检查点回调 `(task_id, state_json, user_data)` |

## 接口 — 生命周期与核心

| 函数 | 语义 |
|------|------|
| `airy_checkpoint_init(storage_path)` | 初始化存储目录；`NULL` 时用数据目录下 `checkpoints/`；重复调用幂等返回成功 |
| `airy_checkpoint_shutdown(void)` | 复位全局状态并清除已注册钩子 |
| `airy_checkpoint_create(task_id, session_id, seq, state_json, completed, c_cnt, pending, p_cnt, &out)` | 构造内存检查点：复制字符串与节点数组，初始状态 `PENDING`，按 `state_json` 计算 checksum；失败不产出句柄 |
| `airy_checkpoint_save(cp)` | 写入文件并将 `cp->state` 置 `COMPLETED`、更新统计；I/O 失败返回 `AIRY_EIO` |
| `airy_checkpoint_restore(task_id, seq, &out)` | 读取并解析指定序号；**`seq == 0` 表示恢复该任务最新序号**；无文件 `AIRY_ENOENT`；文件超过 10 MiB 拒绝 |
| `airy_checkpoint_delete(task_id, seq)` | `seq > 0` 删除单个；`seq == 0` 删除该任务全部；无可删对象返回 `AIRY_ENOENT` |
| `airy_checkpoint_list(task_id, &out_arr, &out_count)` | 按序号升序恢复该任务全部检查点；单个条目失败时跳过；结果数组由调用方逐个 `destroy` 后释放 |
| `airy_checkpoint_verify(cp, &is_valid)` | 重算 checksum 与结构体字段比对；`INVALID` 状态直接判无效 |
| `airy_checkpoint_destroy(cp)` | 释放句柄及其持有的字符串/节点数组（`NULL` 安全） |
| `airy_checkpoint_get_stats(&stats)` | 快照式读取全局统计 |
| `airy_checkpoint_cleanup(max_age_seconds, max_count)` | 按文件修改时间删除过期 `*.json`；数量上限仅钳制统计计数器，不额外删除文件 |

## 接口 — 快照与自动钩子

| 函数 | 语义 |
|------|------|
| `airy_snapshot_create(task_id, snapshot_path)` | 取该任务最新检查点，导出为 `SNAPSHOT_V1` 文本格式（元信息行 + `---DATA---` 原始 state_json） |
| `airy_snapshot_restore(snapshot_path, &task_id)` | 校验快照头并**仅回读任务 ID**（分配字符串，调用方释放）；不恢复状态数据 |
| `airy_checkpoint_set_auto_hook(hook, user_data, interval_ms)` | 注册/清除（`hook` 传 `NULL`）自动检查点回调；本模块不含定时器，`interval_ms` 仅为调用方调度参考 |
| `airy_checkpoint_trigger_auto(task_id)` | 由调用方择机触发已注册回调（`state_json` 实参为 `NULL`） |

## 语义与约束

- **序号约定**：调用方应以 `sequence_num > 0` 保存检查点；`0` 在恢复/删除接口中作「最新/全部」的选择子。
- **解析为逐键文本提取**：读取时按已知键定位字符串值，`restore` 以解析出的 `state_json` 重算 checksum，因此 `verify` 检验的是内存结构自洽，而非与磁盘存档的字节级一致性。
- **时间基准**：文件时间戳用墙钟秒比较；结构体 `timestamp` 为 `airy_time_ns()` 纳秒值。
- **并发**：统计与删除等临界区由全局锁保护；目录扫描不加锁，`list` 结果以实际可恢复集合为准。

## 用法示例

```c
#include <checkpoint.h>

/* 初始化 → 保存 → 恢复一个任务检查点 */
airy_err_t save_and_reload(const char *task_id)
{
    airy_err_t err = airy_checkpoint_init(NULL);
    if (err != AIRY_SUCCESS) {
        return err;
    }

    airy_task_checkpoint_t *cp = NULL;
    const char *state = "{\"step\": 12}";
    err = airy_checkpoint_create(task_id, "s-01", 1, state,
                                 NULL, 0, NULL, 0, &cp);
    if (err != AIRY_SUCCESS) {
        return err;
    }
    err = airy_checkpoint_save(cp);
    airy_checkpoint_destroy(cp);
    if (err != AIRY_SUCCESS) {
        return err;
    }

    airy_task_checkpoint_t *loaded = NULL;
    err = airy_checkpoint_restore(task_id, 0, &loaded); /* 0 = 最新序号 */
    if (err == AIRY_SUCCESS) {
        /* loaded->state_json 即恢复的执行现场 */
        airy_checkpoint_destroy(loaded);
    }
    return err;
}
```

## 构建与依赖

四个 `.c` 随 `airy_common` 静态库编译；`checkpoint.h` 以 PUBLIC 方式导出（`include/agentrt/utils/execution/`，安装规则排除 `*_internal.h`）。

| 依赖 | 用途 |
|------|------|
| `platform` | `AIRY_API` 导出宏、`airy_time_ns`、`airy_mtx_*`、`airy_data_dir` 数据目录 |
| `error` | `airy_err_t` 与 `AIRY_SUCCESS` / `AIRY_EINVAL` / `AIRY_ENOMEM` / `AIRY_ENOTINIT` / `AIRY_ENOENT` / `AIRY_EIO` 等错误契约 |
| `logging` | 运行日志 |
| `memory` / `sync` | `AIRY_MALLOC` 族、`airy_mtx_*`、`atomic_*` |
| `compat`（`airy_dirent.h`） | Windows 目录遍历垫片 |
| 标准库 | `stdio` / `stdlib` / `string` / `ctype` / `time` |

跨 Linux/macOS/Windows。本模块无 agentrt 内部上游依赖，无第三方 JSON/YAML 依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*

*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
