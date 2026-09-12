# Logging — 日志系统

**模块路径**: `commons/utils/logging/`
**版本**: 0.1.15

## 概述

Logging 模块是 commons 的统一分层日志系统，按职责分为三层：

- **Core 层**（`logging.h` + 四个实现文件）：级别管理、格式化、控制台/文件输出、
  按大小轮转、追踪 ID、模块级过滤、运行时节流；
- **Atomic 层**（`atomic_logging.h/.c`）：无锁（MPSC 环形缓冲 + 线程本地缓冲）
  日志记录提交与批量消费；
- **Service 层**（`service_logging.h/.c`）：在 Core 之上叠加轮转/传输配置、
  输出器与过滤器链、监控统计与配置热重载（可选，简单应用可只用 Core 层）。

## 目录结构

```
logging/
├── logging.h                 # Core 层：公共 API、log_level_t/log_output_t/log_format_t、log_config_t
├── logging_internal.h        # Core 层四个实现文件共享的内部状态与访问器声明
├── logging_core.c            # 核心：全局状态、级别、写入路径、生命周期
├── logging_format.c          # 格式化：控制台行格式化、终端与 ANSI 颜色探测
├── logging_backend_file.c    # 文件后端：打开/写入与按大小轮转（backup 滑动窗口）
├── logging_control.c         # 运行时控制：节流（哈希桶 + 每秒上限）等
├── atomic_logging.h          # Atomic 层：无锁队列与批量写入 API
├── atomic_logging.c
├── service_logging.h         # Service 层：轮转/传输/过滤/监控 API
├── service_logging.c
├── svc_logger.h              # 服务层日志宏（SVC_LOG_*、带 trace 上下文的 AIRY_LOG_*_T 等）
├── bench_atomic_logging.c    # Atomic 层性能基准
└── README.md
```

日志单测位于 `commons/tests/unit/test_logger.c`。

## 级别与枚举

日志级别 `log_level_t`（与共享契约头 `airymax/log_types.h` 的
`airy_log_level` 数值严格一致）：

| 级别 | 枚举值 | 说明 |
|------|--------|------|
| `LOG_LEVEL_DEBUG` | 0 | 调试信息 |
| `LOG_LEVEL_INFO` | 1 | 正常运行状态 |
| `LOG_LEVEL_WARN` | 2 | 潜在问题 |
| `LOG_LEVEL_ERROR` | 3 | 功能错误 |
| `LOG_LEVEL_FATAL` | 4 | 致命错误 |

输出目标 `log_output_t`：`CONSOLE` / `FILE` / `SYSLOG` / `NETWORK` / `BUFFER`
（可多目标并存，以位掩码存于 `log_config_t.outputs`）。

输出格式 `log_format_t`：`TEXT` / `JSON` / `STRUCTURED` / `BINARY`。

## 接口说明

### Core 层 API（`logging.h`）

本头只提供函数层 API，**不定义 `LOG_*` 宏**；便捷宏体系见下节。

| 函数 | 说明 |
|------|------|
| `log_init(config)` | 初始化日志系统 |
| `log_set_default_config(config)` | 设置默认配置 |
| `log_write(level, module, line, fmt, ...)` | 记录日志（核心写入函数） |
| `log_write_va(level, module, line, fmt, ap)` | va_list 形式写入 |
| `log_set_trace_id(id)` / `log_get_trace_id()` | 当前线程追踪 ID |
| `log_set_span_id(id)` / `log_get_span_id()` | OpenTelemetry Span ID |
| `log_set_module_level(pattern, level)` | 模块级别设置（支持通配符） |
| `log_reload_config(path)` | 热重载日志配置 |
| `log_flush()` | 强制刷出全部缓冲 |
| `log_cleanup()` | 释放资源并刷出 |
| `log_set_throttle(enable, max_per_sec)` | 启用/配置相同消息节流 |
| `log_should_sample(level)` | 按级别采样率决定是否输出（ERROR/FATAL 恒为真） |
| `log_level_to_string(level)` / `log_level_from_string(str)` | 级别与字符串互转 |

### 便捷宏体系

- `AIRY_LOG_DEBUG/INFO/WARN/ERROR/FATAL(fmt, ...)`：面向应用层的级别宏，
  权威定义在 `commons/utils/observability/logger.h`（经 `airy_log_write` →
  `log_write_va` 落到本模块），并由 `commons/utils/include/logging_compat.h`
  提供包含兼容。
- `SVC_LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL(...)`：守护进程/服务层专用，
  由本目录 `svc_logger.h` 提供，映射到 `AIRY_LOG_*`。
- `AIRY_LOG_*_T(ctx, ...)`：带 trace 上下文版本（`svc_logger.h`）；
  另有 `AIRY_LOG_ERROR_RETURN`、`AIRY_LOG_CHECK` 辅助宏。

### Atomic 层 API（`atomic_logging.h`）

| 函数 | 说明 |
|------|------|
| `atomic_logging_init(config)` | 初始化原子层 |
| `atomic_logging_submit_lockfree(record, non_blocking)` | 无锁提交 |
| `atomic_logging_submit_mutex(record)` | 互斥锁提交 |
| `atomic_logging_submit_batch(records, count)` | 批量提交 |
| `atomic_logging_acquire(record, timeout_ms)` | 消费一条记录 |
| `atomic_logging_acquire_batch(records, max_count, timeout_ms)` | 批量消费 |
| `atomic_logging_flush_thread_local_buffer(buffer)` | 刷出指定线程本地缓冲 |
| `atomic_logging_flush()` | 刷出全部 |
| `atomic_logging_get_stats(out_stats)` | 获取队列统计 |
| `atomic_logging_cleanup()` | 清理 |

### Service 层 API（`service_logging.h`）

| 函数 | 说明 |
|------|------|
| `service_logging_init(config)` | 初始化服务层 |
| `service_logging_configure_rotation(config)` | 配置轮转 |
| `service_logging_configure_transport(config)` | 配置传输 |
| `service_logging_add_outputter(name, type, user_data)` | 注册输出器 |
| `service_logging_add_filter(name, type, user_data)` | 注册过滤器 |
| `service_logging_process_record(record)` | 处理一条日志记录 |
| `service_logging_get_stats(stats)` | 获取监控统计 |
| `service_logging_reload_config(path)` | 热重载服务层配置 |
| `service_logging_cleanup()` | 清理 |

## 使用示例

```c
#include "logging.h"

log_config_t config = {
    .level = LOG_LEVEL_INFO,
    .outputs = (1u << LOG_OUTPUT_CONSOLE) | (1u << LOG_OUTPUT_FILE),
    .format = LOG_FORMAT_JSON,
    .file_path = "/var/log/agentrt/agentrt.log",
    .max_file_size = 100 * 1024 * 1024,
    .max_backup_count = 10,
    .async_mode = true,
    .enable_statistics = true
};
log_init(&config);

log_set_trace_id("tr-0123456789abcdef");
log_write(LOG_LEVEL_INFO, "gateway", __LINE__, "服务启动完成, port: %d", 8080);
log_set_module_level("network.*", LOG_LEVEL_WARN);
log_set_throttle(true, 100);

log_flush();
log_cleanup();
```

应用/服务代码通常使用便捷宏：

```c
#include "svc_logger.h"

SVC_LOG_INFO("请求处理开始, trace: %s", trace_id);
```

## 运行时行为

- **ANSI 色彩**：终端自动启用（INFO=蓝/WARN=黄/ERROR=红/FATAL=品红/DEBUG=灰），
  重定向到管道/文件时自动禁用。
- **时间戳**：`log_write` 内部使用 `clock_gettime(CLOCK_REALTIME, ...)`（墙钟），
  格式 `[YYYY-MM-DD HH:MM:SS.sss]`。`CLOCK_MONOTONIC` 系 API
  （`airy_time_monotonic_ns/ms`，platform 模块）只用于超时、调度与性能测量，
  不应用于日志时间戳；墙钟查询用 `airy_time_realtime_ns`。
- **节流**：相同消息按哈希桶限速，避免日志洪水。

## 环境变量

| 变量 | 取值 | 说明 |
|------|------|------|
| `AIRY_LOG_LEVEL` | 级别名（大小写不敏感） | 启动时覆盖默认日志级别 |
| `AIRY_LOG_COLOR` | `0` / `1` | 强制禁用/启用 ANSI 色彩；未设置时按目标自动探测 |

```bash
AIRY_LOG_COLOR=1 ./your_daemon | cat   # 管道中也强制启用色彩
AIRY_LOG_COLOR=0 ./your_daemon         # 终端中强制禁用色彩
```

## 构建

本模块随 commons 单一静态库 target `airy_common` 一并编译，头文件以 PUBLIC
方式导出，无需单独构建。Atomic 层吞吐与延迟基准可用
`bench_atomic_logging.c` 在 agentrt 构建树中运行。

## 依赖

| 依赖 | 说明 |
|------|------|
| `error`（`commons/utils/error/`） | 错误码与错误处理 |
| `airy_memory.h`（`commons/utils/memory/`） | 统一内存管理 |
| `atomic_compat.h`（`commons/utils/include/`） | 跨平台原子操作（环形缓冲 CAS） |
| `platform.h`（`commons/platform/`） | 平台抽象（时间、线程等） |
| `string_compat.h`（`commons/utils/string/`） | 安全字符串操作 |

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
