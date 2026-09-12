# Observability — 可观测性模块

**模块路径**: `commons/utils/observability/`
**版本**: 0.1.15

## 概述

Observability 模块提供 AgentRT 运行时的可观测性基础组件，包含五个部分：

- **日志（logger）**：`AIRY_LOG_*` 便捷宏的权威定义源，底层委托
  [`utils/logging`](../logging/README.md) 统一日志引擎，自动附加文件、行号与追踪
  ID，进程加载时经构造函数一次性自动初始化；
- **指标（metrics）**：轻量级收集器，支持 Counter / Gauge / Timing 三种类型，
  可导出 JSON 或 Prometheus 文本格式；
- **链路追踪（trace）**：Span 生命周期管理与事件注解，支持 JSON 导出；
- **统一指标（unified_metrics）**：进程级全局注册表（`um_*` API），按模块/实例
  聚合多个守护进程的指标，统一导出一个 Prometheus 端点；
- **告警管理（alert_manager）**：规则引擎（`am_*` API），支持阈值/趋势/复合/
  异常规则、告警抑制与去重、多通道通知（日志/回调/Webhook/文件）与升级策略。

`observability.h` 为聚合入口头：重导出 `logger.h`，声明指标与追踪的核心 API，
并提供 `static inline` 的单调时钟读取 `airy_get_monotonic_time_ns()`。

## 目录结构

```
observability/
├── README.md
├── observability.h            # 聚合入口（日志宏重导出 + 指标/追踪声明 + 单调时钟）
├── logger.h                   # AIRY_LOG_* 宏权威定义 + airy_log_* 函数声明
├── logger.c                   # airy_log_* 实现（委托 utils/logging 引擎）
├── metrics.h                  # 指标收集接口
├── metrics.c                  # 指标实现（JSON 导出经 cJSON，Prometheus 手工格式化）
├── trace.h                    # 链路追踪接口（Span 句柄 + 只读 getter）
├── trace.c                    # 追踪实现（全局 Span 链表 + 每 Span 互斥锁）
├── unified_metrics.h          # 统一指标注册表接口（um_*）
├── unified_metrics.c          # 统一指标实现（含默认系统指标采集）
├── alert_manager.h            # 告警管理接口（am_*）
└── alert_manager.c            # 告警规则引擎与通知通道实现
```

## 日志（logger.h）

### 日志级别

级别常量与 [`airymax/log_types.h`](../../include/airymax/log_types.h) 的
`enum airy_log_level` 及 `utils/logging` 的 `log_level_t` 数值严格一致，
数值越大越严重（syslog/Linux 内核约定）：

| 常量 | 值 | 说明 |
|------|-----|------|
| `AIRY_LOG_LEVEL_DEBUG` | 0 | 调试信息 |
| `AIRY_LOG_LEVEL_INFO` | 1 | 一般信息 |
| `AIRY_LOG_LEVEL_WARN` | 2 | 警告信息 |
| `AIRY_LOG_LEVEL_ERROR` | 3 | 错误信息 |
| `AIRY_LOG_LEVEL_FATAL` | 4 | 致命错误（记录后调用 `abort()`） |

编译时宏 `AIRY_LOG_LEVEL` 默认为 `AIRY_LOG_LEVEL_INFO`，可在集成侧覆盖。

### 接口

| 函数/宏 | 说明 |
|------|------|
| `airy_log_set_trace_id(trace_id)` | 设置当前线程追踪 ID（NULL 则自动生成），返回生效值 |
| `airy_log_get_trace_id()` | 获取当前线程追踪 ID |
| `airy_log_write(level, file, line, fmt, ...)` | 底层写入函数，通常经宏调用 |
| `AIRY_LOG_DEBUG(fmt, ...)` | 调试日志；未定义 `AIRY_DEBUG` 时编译为空操作 |
| `AIRY_LOG_INFO(fmt, ...)` | 信息日志 |
| `AIRY_LOG_WARN(fmt, ...)` | 警告日志 |
| `AIRY_LOG_ERROR(fmt, ...)` | 错误日志 |
| `AIRY_LOG_FATAL(fmt, ...)` | 致命日志，记录后 `abort()` |

便捷宏自动展开 `__FILE__` / `__LINE__`。首次调用任一 `airy_log_*` 函数时经
原子 CAS 完成一次性 `log_init`；在 GCC/Clang（`constructor` 属性）与 MSVC
（`.CRT$XCU`）下还会于加载时预初始化。运行时级别过滤、输出目标、异步写盘等
行为由底层日志引擎决定，见 [`utils/logging`](../logging/README.md)。

> 注意：本模块 `airy_log_write()` 以**文件名**定位来源；`utils/logging` 的
> `log_write()` 以**模块名字符串**定位来源，两者签名不同、用途互补。

## 指标收集（metrics.h）

### 指标类型

| 类型 | 写入 API | 导出形态 |
|------|----------|----------|
| Counter | `airy_metrics_increment()`（`uint64_t` 增量） | 单调累计值 |
| Gauge | `airy_metrics_gauge()`（`double`） | 瞬时值 |
| Timing | `airy_metrics_timing()`（毫秒 `double`） | 总和 + 次数，导出平均值 |

指标名不存在时写入 API 自动创建对应表项（内部为按类型分组的链表）。

### 接口

| 函数 | 说明 |
|------|------|
| `airy_metrics_create()` | 创建收集器，返回句柄（失败返回 NULL） |
| `airy_metrics_destroy(metrics)` | 销毁收集器并释放全部指标 |
| `airy_metrics_increment(metrics, name, value)` | 计数器增加 `value` |
| `airy_metrics_gauge(metrics, name, value)` | 设置仪表值 |
| `airy_metrics_timing(metrics, name, duration_ms)` | 记录一次耗时（毫秒） |
| `airy_metrics_export(metrics)` | 导出 JSON 字符串（调用方释放）；未启用 cJSON 时返回 NULL |
| `airy_metrics_export_prometheus(metrics)` | 导出 Prometheus 文本格式（调用方释放） |
| `airy_metrics_export_prometheus_filtered(metrics, prefix)` | 按名称前缀过滤导出 |

JSON 导出结构为三个对象分组 `counters` / `gauges` / `timings`，其中 timing 项
含 `avg` 与 `count` 字段。Prometheus 文本示例：

```
# TYPE requests_total counter
requests_total 1523
# TYPE memory_usage_mb gauge
memory_usage_mb 128.5
# TYPE request_duration_ms summary
request_duration_ms_sum 42.3
request_duration_ms_count 1
```

## 链路追踪（trace.h）

### 接口

| 函数 | 说明 |
|------|------|
| `airy_trace_begin(name, parent_id)` | 开启 Span（`parent_id` 可为 NULL），返回句柄 |
| `airy_trace_end(span)` | 结束 Span，记录结束时间并将状态置为完成 |
| `airy_trace_add_event(span, name, attributes)` | 添加事件（`attributes` 为 JSON 文本，可为 NULL） |
| `airy_trace_export()` | 导出全部 Span 的 JSON 字符串（调用方释放） |
| `airy_trace_cleanup()` | 清理全部追踪数据 |
| `airy_trace_get_span_count()` | 获取当前记录的 Span 数量 |

`airy_trace_span_t` 为不透明句柄，字段经只读 getter 访问：

| Getter | 返回 |
|------|------|
| `airy_trace_span_get_trace_id(span)` | 追踪 ID 字符串（Span 生命周期内有效） |
| `airy_trace_span_get_span_id(span)` | Span ID 字符串 |
| `airy_trace_span_get_parent_id(span)` | 父 Span ID（无父时为空串） |
| `airy_trace_span_get_name(span)` | Span 名称 |
| `airy_trace_span_get_start_time_us(span)` | 开始时间（微秒） |
| `airy_trace_span_get_end_time_us(span)` | 结束时间（微秒），0 表示运行中 |
| `airy_trace_span_get_status(span)` | 状态：0=运行中，1=完成，2=错误 |

### 容量限制与 ID 格式

以下上限定义于 `trace.c`：

| 参数 | 值 |
|------|-----|
| `MAX_SPANS` | 1024 |
| `MAX_EVENTS_PER_SPAN` | 64 |
| `MAX_TRACE_ID_LEN` | 64 |
| `MAX_SPAN_ID_LEN` | 32 |

ID 生成为 `前缀-<unix 秒时间戳 16 位十六进制>-<原子计数 8 位十六进制>`，
例如 `tr-00000065f3a2b1c0-00000001`；Span ID 使用 `sp-` 前缀，同格式。
父子关系通过 `airy_trace_begin()` 的 `parent_id` 参数显式传入。

JSON 导出为 Span 对象数组，每项含 `trace_id`、`span_id`、可选 `parent_id`、
`name`、`start_time`、（已结束时）`end_time` 与 `duration_us`、
`status`（`"running"` / `"ok"` / `"error"`）、（有事件时）`events` 数组，
事件含 `name`、`timestamp` 与可选 `attributes`（原样嵌入的 JSON 文本）。

## 统一指标（unified_metrics.h）

全局单例注册表，将多个模块/守护进程的指标聚合到统一端点。容量常量：
`UM_MAX_MODULES` = 32、`UM_MAX_METRICS_PER_MOD` = 256。

| 类型 | 说明 |
|------|------|
| `um_metric_type_t` | `UM_TYPE_COUNTER` / `UM_TYPE_GAUGE` / `UM_TYPE_HISTOGRAM` / `UM_TYPE_SUMMARY` |
| `um_metric_entry_t` | 指标条目（名称、help 文本、类型、标签、值/总和/次数、时间戳） |
| `um_module_metrics_t` | 模块指标组（模块名 + 实例 ID + 条目数组） |
| `um_config_t` | 配置（服务名、抓取间隔、保留时长、是否启用默认指标） |
| `um_stats_t` | 运行统计（注册数、导出数、增量/更新次数、活跃模块与指标总数） |

| 函数组 | API |
|------|------|
| 生命周期 | `um_init(config)` / `um_shutdown()` / `um_is_initialized()` |
| 模块注册 | `um_register_module(module_name, instance_id)` / `um_unregister_module(module_name)` |
| 指标操作 | `um_register_metric(...)` / `um_increment(...)` / `um_gauge_set(...)` / `um_observe(...)` |
| 导出 | `um_export_prometheus()` / `um_export_prometheus_module(name)` / `um_export_json()`（均返回调用方释放的字符串） |
| 默认系统指标 | `um_register_default_metrics()` / `um_update_default_metrics()`（CPU/内存/线程等） |
| 辅助 | `um_get_stats(&stats)` / `um_create_default_config()` |

## 告警管理（alert_manager.h）

| 类型 | 取值 |
|------|------|
| `am_level_t` | `INFO` / `WARNING` / `CRITICAL` / `EMERGENCY` |
| `am_state_t` | `PENDING` / `FIRING` / `RESOLVED` / `SUPPRESSED` / `ACKNOWLEDGED` |
| `am_rule_type_t` | `THRESHOLD` / `TREND` / `COMPOSITE` / `ANOMALY` |
| `am_comparison_t` | `GT` / `GTE` / `LT` / `LTE` / `EQ` / `NEQ` |
| `am_channel_type_t` | `LOG` / `CALLBACK` / `WEBHOOK` / `FILE` |

核心结构：`am_rule_t`（规则名、类型、指标名、比较符、阈值、持续/冷却秒数、
复合表达式、启用位）、`am_alert_t`（告警实例：级别、状态、消息、来源、标签、
触发/解决/通知时间、通知与触发计数、确认位）、`am_channel_t`（通道类型、
配置串、最低告警级别）、`am_config_t`（评估间隔、默认冷却、单告警最大通知数、
升级超时、去重/抑制开关）。容量常量：`AM_MAX_RULES` = 64、
`AM_MAX_ACTIVE_ALERTS` = 256、`AM_MAX_CHANNELS` = 8。

| 函数组 | API |
|------|------|
| 生命周期 | `am_init(config)` / `am_shutdown()` |
| 规则管理 | `am_add_rule(rule)` / `am_remove_rule(name)` / `am_set_rule_enabled(name, enabled)` |
| 告警生命周期 | `am_fire(name, level, message, source, labels)` / `am_resolve(name)` / `am_acknowledge(name)` |
| 评估 | `am_record_metric(metric_name, value)` / `am_evaluate(metric_name, value)` / `am_evaluate_all()`（返回触发数） |
| 通知 | `am_register_channel(channel)` / `am_register_callback(cb, user_data, min_level)` |
| 查询 | `am_get_active_alerts(...)` / `am_get_alerts_by_level(...)` / `am_active_alert_count()` |
| 辅助 | `am_level_to_string(level)` / `am_state_to_string(state)` / `am_create_default_config()` |

## 使用示例

```c
#include "observability.h"

/* 日志：追踪 ID + 分级记录（AIRY_LOG_DEBUG 仅在 -DAIRY_DEBUG 时输出） */
airy_log_set_trace_id(NULL);
AIRY_LOG_INFO("agent init start");
AIRY_LOG_ERROR("connect failed: %s", reason);

/* 指标：命名收集器 */
airy_metrics_t *m = airy_metrics_create();
airy_metrics_increment(m, "requests_total", 1);
airy_metrics_gauge(m, "memory_usage_mb", 128.5);
airy_metrics_timing(m, "request_duration_ms", 42.3);

char *prom = airy_metrics_export_prometheus(m);
/* ... 将 prom 文本写入 HTTP /metrics 响应 ... */
AIRY_FREE(prom);
airy_metrics_destroy(m);

/* 追踪：父子 Span 与事件注解 */
airy_trace_span_t *root = airy_trace_begin("handle_request", NULL);
airy_trace_add_event(root, "request_received", "{\"method\":\"GET\"}");
airy_trace_span_t *child = airy_trace_begin("db_query", airy_trace_span_get_span_id(root));
airy_trace_end(child);
airy_trace_end(root);

char *traces = airy_trace_export();
/* ... 输出/上传 traces JSON ... */
AIRY_FREE(traces);
airy_trace_cleanup();

/* 统一指标：注册模块并导出 */
um_init(NULL);
um_register_module("gateway_d", NULL);
um_register_metric("gateway_d", "gateway_requests", UM_TYPE_COUNTER,
                   "Total requests", "method=\"GET\"");
um_increment("gateway_d", "gateway_requests", 1);
char *all = um_export_prometheus();
AIRY_FREE(all);
um_shutdown();
```

## 构建与依赖

本模块全部源文件编译进 commons 单一静态库 target `airy_common`，目录经
`target_include_directories(... PUBLIC ...)` 导出并由 install 规则安装。

| 依赖 | 用途 | 所在模块 |
|------|------|----------|
| `logging.h` | 日志引擎（`log_init` / `log_write_va` / 线程本地追踪 ID） | `utils/logging` |
| `svc_logger.h` | 服务层日志宏（unified_metrics / alert_manager 实现内使用） | `utils/logging` |
| `airy_memory.h` | 分配/释放宏（`AIRY_MALLOC` / `AIRY_FREE` 等） | `utils/memory` |
| `error.h` | 统一错误码 | `utils/error` |
| `platform.h` | 跨平台时间戳（`airy_time_ns`） | `platform` |
| `atomic_compat.h` | 跨平台原子操作与互斥量 | `utils/include` |
| `string_compat.h` / `safe_string_utils.h` | 安全字符串操作 | `utils/string` |
| `cjson/cJSON.h` | JSON 序列化（可选；构建未启用 cjson 时自动注入 `AIRY_NO_CJSON`，`airy_metrics_export()` 返回 NULL） | `utils/cjson`（第三方） |

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
