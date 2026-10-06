# String — 字符串工具模块

**模块路径**: `commons/utils/string/`
**版本**: 0.1.19

## 概述

本模块是 AgentRT 全树字符串与 UTF-8 处理的**唯一真相源（SSoT）**，保留三个
能力面与一个跨平台兼容头：

- **安全字符串层 `safe_string_utils.h`**（14 个函数）：替代 `strcpy`/`strcat`/
  `sprintf` 等不安全 libc 函数的带界校验版本，附敏感缓冲区清理与输入校验；
- **UTF-8 安全层 `safe_utf8.h`**（2 个函数）：RFC 3629 合规性校验与非法序列净化；
- **JSON 顶层键定位件 `json_key.h`**（`airy_json_key`，header-only）：无 cJSON
  依赖的顶层键值定位机制件，收敛全树各模块自研的 `strstr` 键提取副本；
- **兼容头 `string_compat.h`**：`ssize_t`（MSVC）与 `snprintf` 映射的 OS 屏蔽垫片。

> **收敛历史（0.1.19）**：原 `airy_string.h` 核心层（57 函数：`string_buffer_t` /
> `string_view_t` / `string_list_t` / 多编码转换）与 `string_common.*` 公共层
> （`strlcpy` 风格与 JSON 转义等 20 函数）经全树消费者审计确认**模块外零消费**后
> 整体移除，其被消费的 UTF-8 能力迁入 `safe_utf8.*`。全树字符串处理一律委托本模块，
> **不得自研重复轮子**。
>
> **收敛历史（0.1.19 §214）**：`checkpoint_persist.c`（`find_cp_key`）、
> `checkpoint_adapter_json.c`（`find_json_value`）、`orchestrator_llm.c`
> （`find_field`）三处近同构的本地 JSON 键定位 helper 收敛至 `json_key.h` 的
> `airy_json_key`。`daemons/supervisor_d/src/ctrl.c` 的同名 `sup_find_key`
> 因 supervisor_d 是「零项目内库依赖自持层」的隔离边界（仅自持 JSON/日志，
> include 面不含本模块）而**刻意保留**，非重复缺陷。

所有返回堆内存的接口统一经 [`utils/memory`](../memory/README.md) 的分配器分配，
**必须使用 `AIRY_FREE()` 释放**（不要使用 libc `free()`）。头文件声明公共接口
线程安全。

## 目录结构

```
string/
├── README.md
├── safe_string_utils.h/.c         # 安全字符串层 API 与实现
├── safe_utf8.h/.c                 # UTF-8 校验与净化
├── json_key.h                     # JSON 顶层键定位机制件（header-only，无 .c）
└── string_compat.h                # 跨平台兼容定义
```

## 安全字符串层 API（`safe_string_utils.h`）

| 函数 | 说明 |
|------|------|
| `safe_strcpy(dest, src, dest_size)` / `safe_strcat` / `safe_sprintf(dest, size, fmt, ...)` | 带界写入，NULL 校验，溢出返回负值（已截断并保证 NUL 终止） |
| `safe_strlen(str, max_len)` | 带界求长 |
| `safe_strcmp(str1, str2, max_len)` | 带界比较（大小写敏感） |
| `safe_strdup_with_limit(str, max_copy_len)` | 限长堆复制（`AIRY_FREE` 释放） |
| `secure_clear(buf, size)` | 防编译器优化的敏感数据清理 |
| `validate_string_input(str, max_len)` / `validate_pointer(ptr)` / `validate_range(value, min, max)` | 输入校验 |
| `is_valid_ascii(str, len)` | ASCII 合法性 |
| `safe_malloc / safe_calloc / safe_realloc(size, purpose)` | 带用途标签的安全分配包装 |

## UTF-8 安全层 API（`safe_utf8.h`）

| 函数 | 说明 |
|------|------|
| `utf8_validate(str, len)` | 判定字节流是否合规 RFC 3629：拒绝 overlong 编码、代理对（U+D800–U+DFFF）、超出 U+10FFFF 的值、截断尾字节与非法续字节；遇 NUL 提前结束 |
| `utf8_sanitize(in, len, out, out_cap)` | 将非法序列替换为 `U+FFFD`（`EF BF BD`）后写入 `out`，容量不足时安全截断；返回写入字节数，参数非法或容量为 0 时返回 0 |

## JSON 顶层键定位件（`json_key.h`）

`static inline const char *airy_json_key(const char *json, const char *key)`：在
未强制依赖 cJSON 的模块中定位 JSON 对象成员值的起始位置，供解码外部或不可信
JSON 文本使用（强制依赖 cJSON 的模块应优先用
[`utils/cjson/cjson_helpers.h`](../cjson/cjson_helpers.h) 的结构化解析）。

| 行为 | 语义 |
|------|------|
| 键匹配 | 以带引号完整键 `"key"` 精确匹配，`"turn"` 不会命中 `"current_turn"` |
| 键判定 | 只认「跳过空白后紧随 `:`」的匹配，据此跳过值域内嵌的同名字面量 |
| 返回值 | 越过 `:` 与空白后的值首字符指针；未找到或参数非法返回 `NULL` |
| 依赖 | 仅 `<ctype.h>`/`<stddef.h>`/`<string.h>`，**无 cJSON 依赖**；非完整 JSON 解析器 |

## 兼容头（`string_compat.h`）

- `_WIN32`：定义 `ssize_t`（若 MSVC 尚未定义）与 `snprintf → _snprintf` 映射；
- 其他平台：仅引入 `<stddef.h>`/`<stdint.h>`/`<string.h>`。

## 用法示例

```c
#include "safe_string_utils.h"
#include "safe_utf8.h"
#include "airy_memory.h"

/* 带界复制：溢出时返回负值，dest 已截断且保证 NUL 终止 */
char dest[64];
if (safe_strcpy(dest, "Hello, AgentRT!", sizeof(dest)) < 0) {
    /* 目标缓冲区不足 */
}

/* LLM 流式增量对齐：先判定，再决定是否交付 */
if (!utf8_validate(delta, delta_len)) {
    char fixed[256];
    size_t n = utf8_sanitize(delta, delta_len, fixed, sizeof(fixed));
    (void)n; /* 使用 fixed 中已净化的 n 字节 */
}
```

## 构建与依赖

本模块 2 个 `.c` 文件编入静态库 `airy_common`；头文件目录经 PUBLIC 导出，导出面
为 `safe_string_utils.h` / `safe_utf8.h` / `json_key.h` / `string_compat.h`。

| 依赖 | 来源 | 用途 |
|------|------|------|
| `airy_memory.h` | [`utils/memory`](../memory/README.md) | 堆分配（`AIRY_MALLOC`/`AIRY_FREE` 等） |
| `error.h` | [`utils/error`](../error/README.md) | 参数校验失败时的错误返回宏 |

`safe_utf8.c` 仅依赖标准库（`<stdint.h>`），无 commons 内部依赖。

## 相关资源

- 内存释放规范见 [`utils/memory`](../memory/README.md)
- 编码禁用的不安全函数替代宏见 [`utils/compliance`](../compliance/README.md)

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
