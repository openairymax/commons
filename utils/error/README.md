# Error — 错误处理模块

**模块路径**: `commons/utils/error/`
**版本**: 0.1.15

## 概述

Error 模块提供 AgentRT 统一的错误处理框架，涵盖用户态扩展错误码、错误链
追踪、多语言错误描述和便捷的检查宏。该模块是所有组件错误报告的基础设施，
遵循"错误可追溯"原则，支持分层错误码体系。

跨边界的错误码契约（`airy_err_t` 与 `AIRY_E*` 正幅值码）定义在全局头
[`include/airymax/error.h`](../../include/airymax/error.h)；本模块提供的是
用户态扩展码 `AIRY_ERR_*` 与错误链/本地化运行时设施。

## 设计目标

- **统一错误码**：错误码为负值，成功为 0（`AIRY_EOK`），分段管理避免冲突
- **错误链追踪**：支持最多 16 层（`AIRY_ERROR_CONTEXT_MAX_DEPTH`）深度的
  错误上下文记录，包含源文件、行号、函数名和时间戳
- **线程安全**：每线程独立错误链
- **多语言支持**：内建 8 种语言的错误描述（英/简中/繁中/日/韩/德/法/西）
- **便捷宏**：提供 `AIRY_ERROR`、`AIRY_CHECK`、`AIRY_PROPAGATE` 等宏减少样板代码

## 目录结构

```
error/
├── error.h          # 错误处理框架核心接口（错误链、宏、多语言）
├── error_codes.h    # 用户态扩展错误码 AIRY_ERR_*（分段规划）
├── handler.c        # 错误处理实现
└── README.md        # 本文档
```

## 核心数据结构

### 错误码分段（`error_codes.h`）

| 范围 | 说明 |
|------|------|
| `-1 ~ -99` | 通用基础错误（`AIRY_ERR_INVALID_PARAM`、`AIRY_ERR_OUT_OF_MEMORY` 等） |
| `-100 ~ -999` | 系统与平台错误 |
| `-1000 ~ -1999` | 内核层错误 |
| `-2000 ~ -2999` | 服务层错误 |
| `-3000 ~ -3999` | LLM/AI 服务错误 |
| `-4000 ~ -4999` | 执行/工具错误 |
| `-5000 ~ -5999` | 调度错误 |
| `-6000 ~ -6999` | 记忆/存储错误 |
| `-7000 ~ -7999` | 安全/沙箱错误 |

### 常用错误码（实际值）

| 宏 | 值 | 说明 |
|------|------|------|
| `AIRY_EOK` | `0` | 操作成功（`AIRY_OK` / `AIRY_SUCCESS` 为兼容别名） |
| `AIRY_ERR_NULL_POINTER` | `-3` | 空指针 |
| `AIRY_ERR_NOT_FOUND` | `-6` | 未找到 |
| `AIRY_ERR_TIMEOUT` | `-8` | 超时 |
| `AIRY_ERR_NOT_SUPPORTED` | `-9` | 不支持 |
| `AIRY_ERR_UNDERFLOW` | `-15` | 下溢 |
| `AIRY_ERR_UNKNOWN` | `-99` | 未知错误 |
| `AIRY_ERR_INVALID_PARAM` | `-36` | 无效参数 |
| `AIRY_ERR_BUFFER_TOO_SMALL` | `-37` | 缓冲区过小 |
| `AIRY_ERR_ALREADY_EXISTS` | `-38` | 已存在 |
| `AIRY_ERR_PERMISSION_DENIED` | `-39` | 权限不足 |
| `AIRY_ERR_IO` | `-40` | I/O 错误 |
| `AIRY_ERR_PARSE_ERROR` | `-55` | 解析错误 |
| `AIRY_ERR_STATE_ERROR` | `-56` | 状态错误 |
| `AIRY_ERR_CANCELED` | `-57` | 已取消 |
| `AIRY_ERR_OUT_OF_MEMORY` | `-59` | 内存不足 |
| `AIRY_ERR_OVERFLOW` | `-60` | 上溢 |

### `airy_err_severity_t` — 错误严重程度

| 枚举值 | 说明 |
|------|------|
| `AIRY_ERR_SEVERITY_INFO` | 信息 |
| `AIRY_ERR_SEVERITY_WARNING` | 警告 |
| `AIRY_ERR_SEVERITY_ERROR` | 错误 |
| `AIRY_ERR_SEVERITY_CRITICAL` | 严重 |

### `airy_err_chain_t` — 错误链

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | `airy_err_t` | 错误码 |
| `depth` | `int` | 当前链深度 |
| `contexts` | `airy_err_context_entry_t[16]` | 错误上下文条目数组 |

### `airy_err_context_entry_t` — 错误上下文条目

记录源文件名、行号、函数名、错误消息、错误码与纳秒时间戳。

## 接口说明

### 基础错误处理（`error.h`）

| 函数 | 说明 |
|------|------|
| `airy_err_str(code)` | 获取错误码的可读描述 |
| `airy_err_get_severity(code)` | 获取错误码的严重程度 |
| `airy_err_get_chain()` | 获取当前线程的错误链 |
| `airy_err_clear()` | 清除当前线程的错误链 |
| `airy_err_thread_cleanup()` | 线程退出时释放线程局部错误资源 |
| `airy_err_push_ex(code, file, line, func, fmt, ...)` | 推送错误上下文到错误链 |
| `airy_err_print_chain(chain)` | 打印错误链（调试用） |
| `airy_err_chain_to_json(chain)` | 将错误链转为 JSON 字符串（调用方释放） |
| `airy_err_set_handler(handler)` | 设置全局错误回调 |

### 错误链迭代与查询

| 函数 | 说明 |
|------|------|
| `airy_err_chain_iter_init(chain, iter)` | 初始化错误链迭代器 |
| `airy_err_chain_iter_next(iter)` | 获取下一个错误上下文条目 |
| `airy_err_chain_iter_reset(iter)` | 重置迭代器 |
| `airy_err_chain_get_depth(chain)` | 获取错误链深度 |
| `airy_err_chain_get_root_error(chain)` | 获取最早的错误码 |
| `airy_err_chain_get_latest_error(chain)` | 获取最新的错误码 |
| `airy_err_chain_format(chain, lang)` | 格式化错误链为可读字符串 |

### 多语言支持（`airy_language_t`）

| 函数 | 说明 |
|------|------|
| `airy_err_set_language(lang)` | 设置当前语言环境 |
| `airy_err_get_language()` | 获取当前语言环境 |
| `airy_err_str_i18n(code, lang)` | 获取错误码的本地化描述 |
| `airy_err_register_i18n(entries, count)` | 注册自定义错误码的本地化描述 |
| `airy_err_chain_to_json_i18n(chain, lang)` | 按语言将错误链转为 JSON |

语言枚举包含 `AIRY_LANG_EN_US`、`AIRY_LANG_ZH_CN`、`AIRY_LANG_ZH_TW`、
`AIRY_LANG_JA_JP`、`AIRY_LANG_KO_KR`、`AIRY_LANG_DE_DE`、`AIRY_LANG_FR_FR`、
`AIRY_LANG_ES_ES` 共 8 种。

### 便捷宏

| 宏 | 说明 |
|------|------|
| `AIRY_ERROR(code, msg)` | 记录错误上下文并返回负值错误 |
| `AIRY_ERROR_FMT(code, fmt, ...)` | 记录格式化错误并返回 |
| `AIRY_CHECK(cond, code, msg)` | 条件检查，失败时记录并返回错误 |
| `AIRY_CHECK_NULL(ptr, name)` | 空指针检查 |
| `AIRY_CHECK_ALLOC(ptr)` | 内存分配检查 |
| `AIRY_PROPAGATE(expr)` | 错误传播（自动记录传播路径） |
| `AIRY_TRY(expr)` | 错误检查并返回（不记录额外上下文） |
| `AIRY_ERR_NEG(code)` | 正幅值契约码取负（跨边界返回值语义） |
| `AIRY_ERR_EQ(err, code)` | 错误码与正幅值契约码比较 |

### 错误统计

| 函数 | 说明 |
|------|------|
| `airy_err_get_stats(stats)` | 获取全局错误统计 |
| `airy_err_reset_stats()` | 重置全局错误统计 |

## 使用示例

```c
#include "error.h"

airy_err_t do_something(void *ptr) {
    AIRY_CHECK_NULL(ptr, "ptr");

    if (some_condition_fails) {
        AIRY_ERROR(AIRY_ERR_INVALID_PARAM, "invalid parameter");
    }
    return AIRY_EOK;
}

airy_err_t caller(void) {
    AIRY_PROPAGATE(do_something(NULL));  /* 自动记录传播路径 */
    return AIRY_EOK;
}

/* 错误链调试 */
void dump_error_chain(void) {
    airy_err_chain_t *chain = airy_err_get_chain();
    if (chain->depth > 0) {
        airy_err_print_chain(chain);
        char *json = airy_err_chain_to_json(chain);
        if (json) {
            /* json 为错误链的 JSON 文本，交给日志/上报出口后释放 */
            free(json);
        }
        airy_err_clear();
    }
}
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `include/airymax/error.h` | `airy_err_t` 与跨边界契约错误码 |
| `include/airy_types.h` | 基础类型定义 |

## 许可

Copyright (c) 2025-2026 SPHARX Ltd.

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`（双许可，任选其一遵守，
完整文本见仓库根 [LICENSE](../../LICENSE)）。
