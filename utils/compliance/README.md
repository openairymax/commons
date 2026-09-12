# 合规模块（compliance）

**模块路径**: `commons/utils/compliance/`
**版本**: 0.1.15

## 概述

compliance 模块提供 C 编码规范强制机制：把一类「无边界、不可重入、易误用」的
C 标准库函数从全部编译单元中封禁，强制调用方改用 commons 提供的带边界检查或
线程安全的替代接口。整个模块只有两个头文件，不产生任何目标代码，纯编译期机制。

## 目录结构

```
compliance/
├── banned_functions.h    # 禁止函数封禁头（Strict poison / Standard deprecated 两档）
└── compliance_exempt.h   # 非 Strict 档下的弃用警告抑制区间
```

## 注入方式

本模块不需要（也不应该）在源文件中手工包含。agentrt 根 `CMakeLists.txt` 提供
选项 `AIRY_COMPLIANCE_STRICT`（默认 `ON`）；开启时构建树会对**所有**编译单元
全局注入本头：

- 添加编译定义 `AIRY_COMPLIANCE_STRICT`；
- 添加编译选项 `-include .../commons/utils/compliance/banned_functions.h`。

因此策略对每个翻译单元自动生效，遗漏包含不存在。

## Strict 模式（默认）

定义 `AIRY_COMPLIANCE_STRICT` 且未定义 `AIRY_COMPLIANCE_IMPL` 时，通过
`#pragma GCC poison` 封禁以下标识符——任何再次引用都是**编译错误**，而非警告：

| 类别 | 被封禁函数 | 封禁原因 |
|------|-----------|---------|
| 堆分配 | `malloc` `free` `calloc` `realloc` `strdup` `strndup` | 绕过统一分配器与内存统计 |
| 无界输出 | `printf` `fprintf`（定义 `AIRY_HAS_CURL` 时豁免）、`sprintf` `vsprintf` `asprintf` `vasprintf` | 无输出长度上界 / 绕过结构化日志 |
| 无界字符串 | `strcpy` `strcat` `strncpy` `strtok` `gets` | 目标缓冲区无界或语义陷阱 |
| 无界扫描 | `scanf` `fscanf` `sscanf` | `%s` 系列无宽度上界 |
| 裸内存块 | `memcpy` `memmove` `memset` | 强制经由带目的容量检查的包装 |
| 非重入时间 | `localtime` `gmtime` | 返回共享静态缓冲区 |
| 不安全临时文件 | `tmpnam` `mktemp` | 名字可预测，存在竞态 |

头文件在 poison 前先无条件拉入 `unistd.h`（macOS SDK 在其中声明 `mktemp`）
并在 Apple 平台拉入 `mach/mach.h`（其头链中的结构体成员 `free` 会命中 poison），
两者均有头卫保护、无副作用——这是 poison 语义下的必要先决条件，不是依赖。

## Standard 模式

配置时显式关闭 `AIRY_COMPLIANCE_STRICT` 后，`banned_functions.h` 退化为
「弃用包装」：对 `malloc` `free` `calloc` `realloc` `strdup` `strndup`
`strcpy` `strcat` `strtok` `localtime` `gmtime` 以及 `printf` `gets` `scanf`
`fscanf` `sscanf` 定义 `__attribute__((deprecated))` 的内联包装并以宏转发，
调用点仅产生编译警告，代码仍可编译。适合渐进迁移或第三方源码混编场景。

## 豁免机制

两种机制适用场景不同：

1. **`AIRY_COMPLIANCE_IMPL`（真豁免）**：作为编译定义、或在包含
   `banned_functions.h` 之前定义，会使整个封禁区被跳过。供替代实现自身使用
   （如 memory 模块的分配器封装层必须触碰裸 `malloc`）；由各所属目录的
   CMakeLists 以 `set_source_files_properties` 对特定源文件单独设置。
2. **`compliance_exempt.h`（警告抑制区间）**：`AIRY_COMPLIANCE_EXEMPT_BEGIN`
   /`AIRY_COMPLIANCE_EXEMPT_END` 一对宏。Strict 模式下为空操作（poison 无法
   在同一编译单元内局部解除）；仅在 Standard 模式下展开为
   `-Wdeprecated-declarations` 的 push/ignored/pop 区间，用于确需调用弃用
   包装且接受其行为的局部代码。

## 被禁函数与替代接口

下表只列 commons 中**实际提供**的替代物：

| 被禁函数 | 替代 | 定义位置 |
|---------|------|---------|
| `malloc` / `free` | `AIRY_MALLOC(size)` / `AIRY_FREE(ptr)` | `utils/memory/airy_memory_inline.h` |
| `calloc` / `realloc` | `AIRY_CALLOC(num, size)` / `AIRY_REALLOC(ptr, new_size)` | 同上 |
| `strdup` / `strndup` | `AIRY_STRDUP(str)` / `AIRY_STRNDUP(str, n)` | 同上 |
| 敏感数据释放 | `AIRY_SECURE_FREE(ptr, size)`（先擦除后释放） | 同上 |
| `memcpy` / `memmove` / `memset` | `AIRY_MEMCPY` / `AIRY_MEMMOVE` / `AIRY_MEMSET`（带目的容量校验），或 `AIRY_MEMCPY_SAFE(dst, src, size, dst_capacity)` | 同上 |
| `strncpy` | `AIRY_STRNCPY_TERM(dst, src, size)`（保证 NUL 终止） | 同上 |
| `strcpy` / `strcat` | `airy_strlcpy(dest, src, dest_size)` / `airy_strlcat(dest, src, dest_size)` | `utils/compat/compat.h` |
| `printf` / `fprintf` / `sprintf` | `snprintf` / `vsnprintf`；日志输出走 `log_write` / `log_write_va` | 标准库；`utils/logging/logging.h` |
| `gets` | `fgets(buf, size, stream)` | 标准库 |
| `scanf` / `fscanf` | `fgets` 读入后显式解析（如 `strtol`） | 标准库 |
| `sscanf` | 校验缓冲区边界后使用；输出方向改用 `snprintf` | 标准库 |
| `strtok` | `strtok_r` | 标准库 |
| `localtime` / `gmtime` | `localtime_r` / `gmtime_r` | 标准库 |
| `tmpnam` / `mktemp` | `mkstemp` | 标准库 |

## 用法示例

```c
#include <airy_memory.h>   /* AIRY_* 内存宏与 AIRY_MEMCPY_SAFE */
#include "compat.h"        /* airy_strlcpy / airy_strlcat      */

char name[32];
airy_strlcpy(name, user_input, sizeof(name));   /* 替代 strcpy  */

void *buf = AIRY_MALLOC(256);                   /* 替代 malloc  */
if (buf != NULL) {
    AIRY_FREE(buf);
}
```

## 依赖

| 方向 | 对象 | 说明 |
|------|------|------|
| 注入方 | agentrt 根 `CMakeLists.txt` | `AIRY_COMPLIANCE_STRICT` 选项与 `-include` 全局注入 |
| 替代实现 | `commons/utils/memory/` | `AIRY_*` 内存/字符串边界宏 |
| 替代实现 | `commons/utils/compat/` | `airy_strlcpy` / `airy_strlcat` |
| 替代实现 | `commons/utils/logging/` | 结构化日志出口（替代 `printf` 系） |

compliance 自身仅依赖标准库头，不引入任何 agentrt 内部依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
