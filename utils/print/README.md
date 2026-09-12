# print — 运行时统一打印宏

**模块路径**: `commons/utils/print/` · **版本**: 0.1.15

header-only 的运行时打印宏集：8 个 `airy_print_*` 宏统一委托 commons 日志系统的 `log_write()`，宏名与构建期 `cmake/airy_print.cmake` 的函数一一对应。

## 概述

- **复用日志能力**：底层走 `log_write()`，自动获得多输出目标、多格式、trace_id 传播、线程安全与运行期配置等全部日志能力。
- **命名对齐构建期**：与 `airy_print.cmake` 中的构建期函数同名，构建输出与运行输出风格一致。
- **状态标签**：`airy_print_ok` / `airy_print_no` 自动添加 `[OK]` / `[NO]` 前缀，用于健康检查、自检报告一类的判定输出。
- **生产代码约定**：生产代码不直接调用 `printf` / `fprintf`，统一使用本模块宏或 `LOG_*` 宏。

## 目录结构

```
utils/print/
├── airy_print.h   # 8 个打印宏（header-only，无 .c）
└── README.md
```

## 宏一览

| 宏 | 映射级别 | 输出形式 |
|---|---|---|
| `airy_print_ok(fmt, ...)` | INFO | 自动前缀 `[OK] ` |
| `airy_print_no(fmt, ...)` | ERROR | 自动前缀 `[NO] `（仅运行期，构建期无对应函数） |
| `airy_print_info(fmt, ...)` | INFO | 原样 |
| `airy_print_warn(fmt, ...)` | WARN | 原样 |
| `airy_print_error(fmt, ...)` | ERROR | 原样 |
| `airy_print_fatal(fmt, ...)` | FATAL | 原样 |
| `airy_print_debug(fmt, ...)` | DEBUG | 原样 |
| `airy_print_section(fmt, ...)` | INFO | 包裹 `=== ` / ` ===` 边界，用于启动横幅与阶段分隔 |

所有宏均以 `printf` 风格格式化，并自动携带 `__FILE__` / `__LINE__` 交给 `log_write()`。

## 语义与约束

- 颜色由 `log_write()` 按级别内部处理，宏本身不注入 ANSI 序列。
- `airy_print_fatal` 只是按 FATAL 级别写日志，**不主动终止进程**；是否中止由日志系统配置决定。需要立即终止时，调用方在宏之后显式 `abort()` 或 `exit(EXIT_FAILURE)`。
- 日志系统未初始化时，`log_write()` 按默认配置输出到 stderr，宏可安全调用。
- 本模块为纯宏头文件，不产生任何目标文件；`utils/print/` 目录已注册进 `airy_common` 的 PUBLIC include 路径（树内以 `#include "airy_print.h"` 消费），但当前未列入头文件安装规则。

## 用法示例

```c
#include "airy_print.h"

void bootstrap_banner(int pid, const char *config_path)
{
    airy_print_section("Daemon Bootstrap");
    airy_print_info("agentrt starting (pid=%d)", pid);
    airy_print_ok("config loaded: %s", config_path);
    airy_print_warn("deprecated option ignored");
}
```

## 构建与依赖

无编译单元。宏经 `log_write()` 落到 commons 日志实现。

| 依赖 | 用途 |
|---|---|
| commons `utils/logging`（`logging.h`） | `log_write()` 与 `LOG_LEVEL_*` 定义 |

单元测试见 `tests/unit/test_print.c`（验证 8 个宏可编译、可调用、未初始化日志时不崩溃）。本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
