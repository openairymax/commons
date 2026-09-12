# Memory — 内存管理模块

**模块路径**: `commons/utils/memory/`
**版本**: 0.1.15

## 概述

Memory 模块提供 commons 统一的内存管理基础设施：带标签的安全分配/释放、
内存池、调试能力（泄漏检测、边界检查、释放后使用与双重释放检查）、全局与
扩展统计、内存水位监控与 OOM 响应，以及面向 compliance 封禁策略的全套
`AIRY_*` 替代宏。公共入口是聚合头 `airy_memory.h`。

## 目录结构

```
memory/
├── airy_memory.h               # 聚合公共头（引入下面 4 个 airy_memory_*.h）
├── airy_memory_api.h           # 核心 API 声明 + MEMORY_FREE_SAFE
├── airy_memory_types.h         # 子系统类型：选项、统计、类别/水位/OOM 枚举
├── airy_memory_inline.h        # AIRY_* 兼容宏、SAFE_* 宏、AUTO_FREE、SECURE_FREE
├── airy_memory_guard.h         # AIRY_MALLOC_GUARD / AIRY_CALLOC_GUARD 分配守卫
├── airy_memory_stats_ext.h     # 扩展统计跟踪器（inline 实现）
│
├── memory_core.c               # 核心分配/释放/统计实现
├── memory_internal.h           # 核心内部锁与调试表访问
├── memory_stats.c              # 全局统计实现
│
├── memory_common.h / memory_common.c        # 安全分配原语与分配策略切换
│
├── memory_pool.h                            # 内存池 API
├── memory_pool.c / memory_pool_alloc.c / memory_pool_stats.c
├── memory_pool_internal.h                   # 池内部结构
├── memory_prealloc.h / memory_prealloc.c    # 低内存关键路径预分配缓冲
│
├── memory_debug.h                           # 调试 API
├── memory_debug.c / memory_debug_core.c / memory_debug_leak.c
│   / memory_debug_stats.c / memory_debug_track.c / memory_debug_validate.c
├── memory_debug_internal.h                  # 调试子系统内部结构
│
├── memory_stats_reporter.h / memory_stats_reporter.c  # 统计周期上报
└── README.md
```

## 核心数据结构

### memory_stats_t — 全局统计

`total_allocated` / `total_freed` / `current_allocated` / `peak_allocated` /
`allocation_count` / `free_count` / `leak_count`（均为 `size_t`）。

### memory_options_t — 分配选项

| 字段 | 类型 | 说明 |
|------|------|------|
| `alignment` | `size_t` | 对齐要求 |
| `zero_memory` | `bool` | 是否零初始化 |
| `tag` | `const char *` | 分配标签（调试与统计） |
| `fail_strategy` | `memory_fail_strategy_t` | 失败策略：`RETURN_NULL` / `ABORT` / `CALLBACK` / `RETRY` |
| `fail_callback` / `fail_callback_user_data` | 回调 | `CALLBACK` 策略下触发 |

### memory_pool_options_t — 内存池选项

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `block_size` | `size_t` | — | 块大小（字节） |
| `initial_blocks` | `size_t` | 16 | 创建时预分配块数 |
| `max_blocks` | `size_t` | 0（无限制） | 最大块数 |
| `expansion_size` | `size_t` | 8 | 池满时扩展块数 |
| `thread_safe` | `bool` | true | 是否线程安全 |
| `name` | `const char *` | NULL | 池名称（调试用） |

`memory_pool_stats_t` 提供块水位（`total/allocated/free_blocks`）、字节用量
（`total/used_memory`）、`allocation_count` / `free_count` 与命中率
（`hit_count` / `miss_count`）。

### memory_debug_options_t — 调试选项

`enable_leak_check`、`enable_boundary_check`、`enable_use_after_free_check`、
`enable_double_free_check`、`enable_invalid_free_check`、`track_allocations`、
`fill_pattern_on_alloc`、`fill_pattern_on_free`、`redzone_size`、
`verbosity_level`（0-3）。

### 扩展统计与水位监控

- `alloc_category_t`：`ALLOC_SHORT_LIVED=0` / `ALLOC_LONG_LIVED=1` /
  `ALLOC_CRITICAL=2`；
- `watermark_level_t`：`NORMAL=0` / `WARNING=1` / `HIGH=2` / `CRITICAL=3`；
- `oom_response_level_t`：`WARNING=0` / `DEGRADED=1` / `CRITICAL=2` /
  `FATAL=3`；
- `memory_stats_extended_t`：在全局统计之上叠加疑似泄漏字节、短生命周期高
  水位、按类别计数、OOM 事件、系统总内存、环形分配跟踪器与最多
  `MAX_WATERMARK_CALLBACKS`（8）个水位回调槽。

## 接口说明

### 核心 API（`airy_memory_api.h`）

| 函数 | 说明 |
|------|------|
| `memory_init(options)` / `memory_cleanup()` | 初始化 / 清理 |
| `memory_alloc(size, tag)` / `memory_calloc(size, tag)` | 带标签分配 / 清零分配 |
| `memory_aligned_alloc(alignment, size, tag)` | 对齐分配 |
| `memory_realloc(ptr, new_size, tag)` | 重分配 |
| `memory_free(ptr)` | 释放 |
| `memory_get_stats(stats)` / `memory_reset_stats()` | 全局统计 |
| `memory_get_current_usage()` / `memory_get_peak_usage()` | 当前 / 峰值用量 |
| `memory_check_leaks(dump_to_stderr)` | 泄漏检查 |
| `memory_dump_debug_info(file)` / `memory_validate(ptr)` | 调试信息转储 / 块完整性验证 |
| `memory_debug_enable(enable)` | 开/关调试追踪 |
| `memory_set_fail_callback(cb, user_data)` | 分配失败回调 |

### 安全分配与策略（`memory_common.h`）

`memory_safe_alloc` / `memory_safe_realloc` / `memory_safe_free` /
`memory_safe_strdup`；`memory_get_global_stats` / `memory_reset_global_stats`；
策略切换 `memory_set_strategy` / `memory_get_strategy`
（`MEMORY_STRATEGY_DEFAULT` / `PERFORMANCE` / `SAFETY` / `LOW_LATENCY`）。

### 内存池 API（`memory_pool.h`）

`memory_pool_create` / `create_default` / `destroy`、`alloc` / `calloc` /
`free`、`get_stats` / `reset_stats`、`prealloc` / `clear` / `expand` /
`shrink`、`is_empty` / `is_full` / `validate`、`iterate`、`get_name` /
`set_name`。

### 预分配缓冲（`memory_prealloc.h`）

为信号处理等低内存关键路径预留缓冲：`airy_prealloc_init` /
`airy_prealloc_shutdown` / `airy_prealloc_acquire(category)` /
`airy_prealloc_release(category)` / `airy_prealloc_is_initialized`。

### 调试 API（`memory_debug.h`）

初始化与开关（`memory_debug_init` / `enable` / `is_enabled` /
`set_feature` / `set_callback` / `set_log_level`）、泄漏与校验
（`memory_debug_check_leaks` / `validate` / `validate_all`）、分配信息查询
（`get_allocation_info` / `set_tag` / `get_stats` / `reset_stats`）、堆栈跟踪
（`enable_stack_trace` / `get_stack_trace`）、检查点对比
（`checkpoint` / `compare_checkpoints`）、操作日志
（`log_operation` / `dump_info`）。

### 扩展统计跟踪（`airy_memory_stats_ext.h`）

`airy_memory_stats_extended_init` / `destroy`、`airy_memory_track_alloc` /
`track_free`、`airy_check_leaks_scheduled`、`airy_memory_calc_watermark` /
`airy_memory_check_watermark`、`airy_register_watermark_callback` /
`airy_unregister_watermark_callback`、`airy_oom_determine_response`、
`airy_memory_stats_report`，以及轻量包装 `airy_check_memory_leaks` /
`airy_get_memory_stats`。

### 统计上报器（`memory_stats_reporter.h`）

`airy_mem_stats_reporter_init` / `airy_msrep_shutdown`、
`airy_mem_stats_get` / `record_alloc` / `record_dealloc` / `record_oom`、
`airy_mem_stats_set_interval`。

### 便捷宏（`airy_memory_inline.h` / `airy_memory_api.h` / `airy_memory_guard.h`）

| 宏 | 说明 |
|----|------|
| `AIRY_MALLOC` / `AIRY_CALLOC` / `AIRY_REALLOC` / `AIRY_FREE` | 兼容 libc 语义的安全分配族 |
| `AIRY_STRDUP` / `AIRY_STRNDUP` / `AIRY_STRNCPY_TERM` | 字符串安全复制 |
| `AIRY_MEMCPY` / `AIRY_MEMMOVE` / `AIRY_MEMSET` / `AIRY_MEMCPY_SAFE` | 带容量校验的内存块操作 |
| `AIRY_SECURE_FREE(ptr, size)` | 先擦除后释放（敏感数据） |
| `AUTO_FREE` | `__attribute__((cleanup))` 作用域自动释放；不支持的编译器回退为手动 |
| `AIRY_MALLOC_GUARD` / `AIRY_CALLOC_GUARD` | 分配 + NULL 检查 + 失败跳转三步合一 |
| `SAFE_MALLOC` / `SAFE_CALLOC` / `SAFE_MALLOC_ARRAY` / `SAFE_CALLOC_ARRAY` | 失败置 NULL，数组版本带溢出检查 |
| `MEMORY_FREE_SAFE(&ptr)` | 释放并置 NULL |

> 本模块是 compliance 封禁的 `malloc` / `memcpy` / `strncpy` 等函数的官方
> 替代实现来源；模块自身的实现层通过 `AIRY_COMPLIANCE_IMPL` 豁免裸 libc
> 调用，详见 `commons/utils/compliance/README.md`。

## 使用示例

```c
#include "airy_memory.h"
#include "memory_pool.h"

memory_init(NULL);

/* 基本分配与自动释放 */
AUTO_FREE char *buf = AIRY_MALLOC(1024);   /* 离开作用域自动 airy_free */

void *data = AIRY_CALLOC(64, 64);
data = AIRY_REALLOC(data, 4096);
AIRY_FREE(data);

/* 内存池 */
memory_pool_options_t opts = {
    .block_size = 256,
    .initial_blocks = 32,
    .max_blocks = 1024,
    .expansion_size = 16,
    .thread_safe = true,
    .name = "request_pool"
};
memory_pool_t *pool = memory_pool_create(&opts);
if (pool != NULL) {
    void *block = memory_pool_alloc(pool);
    memory_pool_free(pool, block);
    memory_pool_destroy(pool);
}

/* 全局统计 */
memory_stats_t stats;
if (memory_get_stats(&stats)) {
    /* stats.current_allocated / stats.peak_allocated ... */
}
memory_cleanup();
```

## 依赖

| 依赖 | 说明 |
|------|------|
| `platform`（`commons/platform/`） | 原子与锁原语、系统内存信息查询 |
| `error`（`commons/utils/error/`） | 错误码 |
| 标准库 | `stdbool.h` / `stddef.h` / `stdint.h` 等 |

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
