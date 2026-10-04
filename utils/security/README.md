# security — 安全工具

**模块路径**: `commons/utils/security/` · **版本**: 0.1.19

提供日志敏感信息脱敏组件 `log_sanitizer`，编译进 commons 静态库 `airy_common`。

## 概述

- **log_sanitizer**：维护一张全局敏感字段模式表（内置 15 个默认模式，如 `api_key`、`password`、`token`、`authorization`），对任意日志文本做大小写不敏感的模式匹配，将字段值替换为掩码（默认 `***`）。内部以互斥锁保护全局表，接口线程安全。

## 目录结构

```
security/
├── log_sanitizer.h          # 日志脱敏接口（7 个函数）
├── log_sanitizer.c          # 日志脱敏实现
└── README.md
```

## log_sanitizer — 日志脱敏

| 函数 | 说明 |
|------|------|
| `log_sanitizer_init(max_fields)` | 重建模式表（容量 0 → 默认 32），预置内置模式 |
| `log_sanitizer_destroy(void)` | 释放模式表并复位，之后可再次 `init` |
| `log_sanitizer_add_pattern(pattern, replacement)` | 追加模式；`replacement` 为 NULL 时用 `***`；表满返回 `false` |
| `log_sanitize(message, buffer, buffer_size)` | 脱敏写入调用方缓冲 |
| `log_sanitize_dup(message)` | 动态版本，返回调用方释放的副本 |
| `log_contains_sensitive(message)` | 是否命中任一模式 |
| `log_get_default_patterns(count)` | 取内置模式表（15 项）与数量 |

脱敏行为：模式匹配大小写不敏感，且要求字段名前后为边界字符（前：串首/空白/引号/逗号；后：`=` `:` 空格/引号/换行/`&`/串尾）。命中后从字段名结束到值结束整段消费，重写为 `字段名=替换串`——即原分隔符（`:`、`=`、空白）被归一为 `=`，例如 `password: hunter2` 输出 `password=***`。

## 语义与约束

- 日志脱敏的全局模式表由内部互斥锁保护；`pattern`/`replacement` 字符串按指针保存，调用方须保证其生命周期覆盖 sanitizer 使用期。
- `log_sanitize` 成功返回输出长度；失败返回负错误码（参数非法 `-36`、缓冲不足 `-60`），调用方按 `< 0` 判断即可。`log_sanitize_dup` 分配量不小于 4096 字节。
- 脱敏输出始终为完整拷贝，不存在零拷贝路径。

## 用法示例

```c
#include "log_sanitizer.h"

#include <stddef.h>

/* 写日志前脱敏 */
void prepare_log_line(const char *raw, char *out, size_t out_size)
{
    if (log_sanitize(raw, out, out_size) < 0) {
        out[0] = '\0'; /* 溢出或参数非法时宁可不输出 */
    }
}
```

## 构建与依赖

`log_sanitizer.c` 由 `commons/CMakeLists.txt` 列入 `airy_common` 源列表，`utils/security` 注册为 PUBLIC include 目录，头文件随 `install` 导出到 `include/agentrt/utils/security/`。链接 `airy_common` 即可使用，无需额外 target。

| 依赖 | 用途 |
|------|------|
| `utils/error`（`error.h`、`error_codes.h`） | `AIRY_EINVAL`、`AIRY_EOVERFLOW` 等错误码 |
| `utils/memory`（`airy_memory.h`） | 模式表的分配/释放（`AIRY_MALLOC`/`AIRY_FREE`） |
| `utils/logging`（`svc_logger.h`） | `log_sanitizer` 的事件日志（`SVC_LOG_*` 转发到 `AIRY_LOG_*`） |
| `utils/include`（`atomic_compat.h`） | `log_sanitizer` 的互斥锁与原子原语 |

本模块仅依赖 commons 内部其他工具模块，无 agentrt 内部上游依赖。

`log_sanitizer` 在 commons 测试套件中暂无专项用例。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
