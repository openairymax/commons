# print — 运行时统一打印 API

`print` 提供**运行时统一打印宏**（P3.25），位于 `commons/utils/print`。命名与
构建期 `airy_print.cmake` 层精确对齐；底层委托核心日志系统 `log_write()`，
复用其全部能力（多输出目标、多格式、trace_id 传播、线程安全、运行期配置热重载）。

## 宏一览

| 运行期宏 | 日志级别 | 用途 |
|----------|----------|------|
| `airy_print_ok()` | INFO | 状态成功，自动加 `[OK]` 前缀 |
| `airy_print_no()` | ERROR | 状态失败，自动加 `[NO]` 前缀 |
| `airy_print_info()` | INFO | 常规信息 |
| `airy_print_warn()` | WARN | 警告 |
| `airy_print_error()` | ERROR | 错误 |
| `airy_print_fatal()` | FATAL | 致命 |
| `airy_print_debug()` | DEBUG | 调试 |
| `airy_print_section()` | INFO | 章节/阶段标题 |

## 使用示例

```c
#include "airy_print.h"

airy_print_section("Daemon Bootstrap");
airy_print_info("agentrt v0.1.1 starting (pid=%d)", getpid());
airy_print_ok("config loaded: %s", config_path);
airy_print_error("init failed: %s (errno=%d)", what, errno);
airy_print_no("health check failed: %s", check_name);
```

## 状态

- **实现**：`include/airy_print.h`（宏，委托 `log_write`）。
- **规范**：生产代码禁止直接 `fprintf/printf`，统一使用本宏或 `LOG_*` 宏。
