# io — 文件与目录工具

**模块路径**: `commons/utils/io/` · **版本**: 0.1.15

跨平台的文件读写与目录管理最小工具集：整读文件、原子写文件、目录创建/枚举/删除。

## 概述

- **原子写入**：`airy_io_write_file` 先写同目录 `<path>.tmp` 临时文件、`fflush` + `fsync`（Windows 为 `_commit`）落盘，再原子改名覆盖目标（Windows 经 `MoveFileExA` 允许替换已存在目标）；中途失败不会在目标路径留下截断/半写文件。
- **整读便捷**：`airy_io_read_file` 一次读出全部字节并补 NUL 终止符，结果既可按二进制长度使用也可按 C 字符串使用。
- **幂等删除**：删除不存在的文件/目录按成功处理，适合清理临时工作区。
- **跨平台**：POSIX 走 `dirent`/`unistd`，Windows 走 Win32 API；目录遍历经 compat 层的 `airy_dirent.h` 统一。

## 目录结构

```
utils/io/
├── io.h           # 8 个 API 的接口定义
├── file_utils.c   # 实现（跨平台）
└── README.md
```

## 接口

### 文件读写

| 函数 | 语义 |
|---|---|
| `airy_io_read_file(path, out_len)` | 读取文件全部内容，返回以内存分配器分配的缓冲区（末字节补 `'\0'`），`out_len` 可选输出长度；失败返回 `NULL` 并把错误压入 commons 错误栈（可经 `airy_err_last()` 获取） |
| `airy_io_write_file(path, data, len)` | 原子写入（见「概述」）；`len` 传 `(size_t)-1` 时按 C 字符串自动计算长度；成功返回 0，失败返回负值 |

### 目录操作

| 函数 | 语义 |
|---|---|
| `airy_io_ensure_dir(path)` | 目录不存在则创建单层目录（0755），已存在返回 0 |
| `airy_io_mkdir_p(path, mode)` | 递归创建多级目录；`mode` 为 Unix 风格权限，Windows 忽略；路径已存在且为目录返回 0，存在但不是目录返回负值 |
| `airy_io_list_files(path, out_files, out_count)` | 枚举目录下的普通文件（不含子目录项），逐个以 `AIRY_STRDUP` 产出文件名，成功后经 `airy_io_free_list` 释放 |
| `airy_io_free_list(files, count)` | 释放文件列表；`files` 为 NULL 时安全无操作 |

### 删除操作

| 函数 | 语义 |
|---|---|
| `airy_io_remove_file(path)` | 删除单个文件；文件不存在按成功处理（幂等） |
| `airy_io_remove_dir_recursive(path)` | 递归删除目录树；传入普通文件则直接删除；路径不存在按成功处理（幂等） |

## 语义与约束

- 参数非法（NULL 路径、NULL 数据等）返回 `AIRY_EINVAL`；原子写过程失败（含路径过长导致临时文件名无法构造）返回 `-1`。
- 实现内部使用固定 1024 字节路径缓冲（临时文件拼接、逐级创建、子路径拼接），路径应控制在 1023 字符以内。
- `airy_io_read_file` 的返回值来自 commons 内存分配器，用 `AIRY_FREE` / `memory_free` 释放，不要直接 `free`。
- 模块不持有全局状态，可对不同路径并发调用；对同一路径并发写入时，原子性仅保证「目标文件内容为某一次完整写入」，不保证多次写入的先后顺序。

## 用法示例

```c
#include "io.h"
#include "airy_memory.h"

void config_roundtrip(void)
{
    const char *payload = "{\"level\":\"debug\"}";
    if (airy_io_mkdir_p("/var/lib/app/conf", 0755) != 0) {
        return;
    }
    /* len 传 -1：按字符串长度写入，原子替换目标文件 */
    if (airy_io_write_file("/var/lib/app/conf/settings.json", payload, (size_t)-1) != 0) {
        return;
    }

    size_t len = 0;
    char *text = airy_io_read_file("/var/lib/app/conf/settings.json", &len);
    if (text != NULL) {
        /* text[0..len) 为文件内容，末尾另有 NUL */
        AIRY_FREE(text);
    }

    airy_io_remove_file("/var/lib/app/conf/settings.json");
}
```

## 构建与依赖

`file_utils.c` 随 `airy_common` 静态库编译（CMake 源列表 `utils/io/file_utils.c`），`io.h` 经 PUBLIC include 路径以 `#include "io.h"` 消费，并安装至 `include/agentrt/utils/io/`。

| 依赖 | 用途 |
|---|---|
| commons `utils/memory` | 读文件缓冲区与列表项的分配/释放（`memory_alloc`、`AIRY_STRDUP`、`AIRY_FREE`） |
| commons `utils/error` | `airy_io_read_file` 失败路径经错误栈宏 `AIRY_ERROR_NULL` 上报 |
| commons `utils/compat` | `airy_dirent.h` 目录遍历兼容层 |
| 平台 CRT | POSIX（`stdio`/`dirent`/`unistd`）与 Windows（Win32 `MoveFileExA` 等） |

单元测试见 `tests/unit/test_io.c`（写入/读回/覆盖/幂等删除、目录树创建与枚举、参数非法路径）。本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
