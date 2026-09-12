# String — 字符串工具模块

**模块路径**: `commons/utils/string/`
**版本**: 0.1.15

## 概述

String 模块提供安全、统一的字符串处理基础设施，包含三个公共 API 层与一个兼容头：

- **核心层 `airy_string.h`**（57 个函数）：带边界检查的复制/连接/比较/查找/修剪/
  替换/分割/格式化，动态字符串缓冲区（`string_buffer_t`）、零拷贝字符串视图
  （`string_view_t`）、字符串列表（`string_list_t`）、多编码转换（ASCII / UTF-8 /
  UTF-16 / UTF-32 / Latin-1 / Windows-1252）；
- **公共层 `string_common.h`**（20 个函数）：`strlcpy`/`strlcat`/`strdup` 风格的基础
  操作、数值互转（`itoa`/`ftoa`/`strtoint` 等）、JSON 转义；
- **安全工具层 `safe_string_utils.h`**（14 个函数）：用于替代 `strcpy`/`strcat`/
  `sprintf` 等不安全函数的带界校验版本，附敏感缓冲区清理与输入校验；
- **兼容头 `string_compat.h`**：`ssize_t` 定义（MSVC）与 `snprintf` 映射。

所有返回堆内存的接口统一经 [`utils/memory`](../memory/README.md) 的分配器分配，
**必须使用 `AIRY_FREE()` 释放**（不要使用 libc `free()`）。头文件声明公共接口
线程安全。

## 目录结构

```
string/
├── README.md
├── airy_string.h                  # 核心层 API（数据结构 + 57 函数）
├── string.c                       # 复制/连接/比较基础实现
├── string_find.c                  # 查找与修剪
├── string_split.c                 # 分割与连接
├── string_classify.c              # 字符分类（digit/alpha/alnum/blank）
├── string_format.c                # 格式化与堆分配复制
├── string_buffer.c                # 字符串缓冲区容器
├── string_view.c                  # 视图与列表容器
├── string_encode.c                # 编码转换与 JSON 转义
├── string_internal.h              # 内部共享声明
├── string_common.h/.c             # 公共层 API 与实现
├── safe_string_utils.h/.c         # 安全工具层 API 与实现
└── string_compat.h                # 跨平台兼容定义
```

## 数据结构与枚举（`airy_string.h`）

### `string_buffer_t` — 动态字符串缓冲区

| 字段 | 类型 | 说明 |
|------|------|------|
| `data` | `char *` | 缓冲区数据 |
| `capacity` | `size_t` | 容量（含终止符） |
| `length` | `size_t` | 当前长度（不含终止符） |
| `encoding` | `string_encoding_t` | 编码 |
| `gateway` | `bool` | 是否动态分配 |

### `string_view_t` — 字符串视图（零拷贝，不持有所有权）

字段：`data`（`const char *`）、`length`、`encoding`。

### `string_list_t` — 字符串列表

字段：`items`（`string_view_t *`）、`count`、`capacity`。

### 枚举

| 枚举 | 取值 |
|------|------|
| `string_encoding_t` | `ASCII`、`UTF8`、`UTF16_LE`、`UTF16_BE`、`UTF32_LE`、`UTF32_BE`、`LATIN1`、`WINDOWS_1252` |
| `string_compare_option_t` | `CASE_SENSITIVE`(0)、`CASE_INSENSITIVE`(1)、`NATURAL`(2，自然排序)、`LOCALE_AWARE`(4) |
| `string_split_option_t` | `KEEP_EMPTY`(1)、`TRIM_WHITESPACE`(2)、`LIMIT_COUNT`(4) |
| `string_format_options_t` | `initial_buffer_size`、`max_buffer_size`、`locale_aware`、`null_string`、`error_string` |

## 核心层 API（`airy_string.h`）

### 安全复制 / 连接 / 长度

| 函数 | 说明 |
|------|------|
| `string_copy(dest, src, dest_size)` / `string_copy_n` | 带界复制，保证 NUL 终止；截断时返回 -1 |
| `string_concat(dest, src, dest_size)` / `string_concat_n` | 带界连接 |
| `string_length(str, max_len)` | 带界求长 |

### 比较与查找

| 函数 | 说明 |
|------|------|
| `string_compare(s1, s2, options)` / `string_compare_n` | 按选项比较（大小写/自然排序） |
| `string_find(haystack, needle, options)` / `string_find_last` | 子串查找，返回位置指针 |
| `string_find_char(str, ch)` / `string_find_char_last` | 字符查找 |

### 修剪 / 大小写 / 替换

| 函数 | 说明 |
|------|------|
| `string_trim` / `string_trim_start` / `string_trim_end` | 原地修剪（返回移动后的指针） |
| `string_to_lower` / `string_to_upper` | 原地转换 |
| `string_replace(str, old, new, result, result_size)` | 替换到输出缓冲区 |

### 分割与连接

| 函数 | 说明 |
|------|------|
| `string_split(str, delimiter, options, limit)` | 返回 `string_list_t`（视图项，`string_list_destroy` 释放底层存储） |
| `string_join(list, delimiter, result, result_size)` | 列表连接进缓冲区 |

### 检查

`string_starts_with(str, prefix, options)`、`string_ends_with`、`string_is_blank`、
`string_is_digit`、`string_is_alpha`、`string_is_alnum`。

### 格式化与堆分配

| 函数 | 说明 |
|------|------|
| `string_format(buf, size, fmt, ...)` / `string_format_v` | 带界格式化 |
| `string_alloc_format(fmt, ...)` / `string_alloc_format_v` | 自动分配完整结果（`AIRY_FREE` 释放） |
| `string_alloc_copy(str)` / `string_alloc_copy_n` / `string_alloc_concat` | 堆上复制/连接（`AIRY_FREE` 释放） |

### 缓冲区 / 视图 / 列表容器

| 组 | 函数 |
|----|------|
| 缓冲区 | `string_buffer_create(capacity, encoding)`、`destroy`、`append`、`append_n`、`append_format`、`append_char`、`clear`、`cstr`、`length` |
| 视图 | `string_view_create(str, encoding)`、`create_n`、`view_compare`、`view_find`（返回 `ssize_t` 偏移）、`view_to_cstr`（堆分配） |
| 列表 | `string_list_create(capacity)`、`destroy`、`add`、`add_cstr`、`clear`、`size`、`get` |

### 编码转换

| 函数 | 说明 |
|------|------|
| `string_convert_encoding(src, src_enc, dest, dest_size, dest_enc)` | 编码转换 |
| `string_utf8_char_count(str, max_len)` | UTF-8 码点数 |
| `string_utf8_next_char(str, &ch)` | 解码下一码点（`uint32_t *`），返回字节宽度 |
| `string_utf8_validate(str, len)` | UTF-8 合法性检查 |

## 公共层 API（`string_common.h`）

| 组 | 函数 |
|----|------|
| 复制/连接 | `string_common_strlcpy(dest, size, src)`、`strlcat`（BSD 语义，返回总长度） |
| 堆复制 | `string_common_strdup` / `strndup`（`AIRY_FREE` 释放） |
| 比较/查找 | `strcasecmp` / `strncasecmp` / `strstr` |
| 分割 | `string_common_strsplit(str, delim)`（返回 `char **`，`strsplit_free` 释放）、`strsplit_free` |
| 数值互转 | `strtoint` / `strtouint` / `strtod`（`bool` + 出参）；`itoa` / `utoa` / `ftoa`（写入调用方缓冲区，返回长度） |
| 修剪/大小写 | `strtrim` / `strtolower` / `strtoupper`（原地） |
| JSON 转义 | `string_common_json_escape(src, &out)`（堆分配输出）、`json_escape_buf(src, dst, size)`（固定缓冲区） |

## 安全工具层 API（`safe_string_utils.h`）

| 函数 | 说明 |
|------|------|
| `safe_strcpy(dest, src, dest_size)` / `safe_strcat` / `safe_sprintf(dest, size, fmt, ...)` | 带界写入，NULL 校验，返回写入长度或负值 |
| `safe_strlen(str, max_len)` | 带界求长 |
| `safe_strcmp(s1, s2, max_len)` | 带界比较 |
| `safe_strdup_with_limit(str, max_copy_len)` | 限长堆复制（`AIRY_FREE` 释放） |
| `secure_clear(buf, size)` | 防编译器优化的敏感数据清理 |
| `validate_string_input(str, max_len)` / `validate_pointer(ptr)` / `validate_range(value, min, max)` | 输入校验 |
| `is_valid_ascii(str, len)` | ASCII 合法性 |
| `safe_malloc / safe_calloc / safe_realloc(size, purpose)` | 带用途标签的安全分配包装 |

## 用法示例

```c
#include "airy_string.h"
#include "airy_memory.h"

/* 带界复制与格式化 */
char dest[64];
int n = string_copy(dest, "Hello, AgentRT!", sizeof(dest));
if (n < 0) {
    /* 目标缓冲区不足，已截断但保证 NUL 终止 */
}

/* 动态缓冲区拼接 */
string_buffer_t *buf = string_buffer_create(64, STRING_ENCODING_UTF8);
string_buffer_append(buf, "task:");
string_buffer_append_format(buf, "%d-%s", 42, "done");
const char *view = string_buffer_cstr(buf);
string_buffer_destroy(buf);

/* 分割与遍历 */
string_list_t parts = string_split("a,b,,c", ",", STRING_SPLIT_TRIM_WHITESPACE, 0);
for (size_t i = 0; i < string_list_size(&parts); i++) {
    string_view_t item = string_list_get(&parts, i);
    (void)item; /* 使用 item.data / item.length，注意视图不持有内存 */
}
string_list_destroy(&parts);

/* 堆分配格式化结果：AIRY_FREE 释放 */
char *msg = string_alloc_format("Task #%d completed in %.2f ms", 42, 123.45);
AIRY_FREE(msg);
```

## 构建与依赖

本模块 10 个 `.c` 全部编入静态库 `airy_common`，头文件目录经 PUBLIC 导出。

| 依赖 | 来源 | 用途 |
|------|------|------|
| `airy_memory.h` | [`utils/memory`](../memory/README.md) | 所有堆分配（`AIRY_MALLOC`/`AIRY_FREE` 等） |
| `error.h` | [`utils/error`](../error/README.md) | 参数校验失败时的错误返回宏 |
| `string_compat.h` | 本目录 | MSVC `ssize_t`/`snprintf` 兼容 |

除上述基础库外不依赖其他 commons 模块；反之，`utils/memory`、`utils/config_unified`、
`utils/observability`、`utils/ipc` 等广泛消费本模块。

## 相关资源

- 内存释放规范见 [`utils/memory`](../memory/README.md)
- 编码禁用的不安全函数替代宏见 [`utils/compliance`](../compliance/README.md)

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
