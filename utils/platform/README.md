# platform — 平台适配工具集

**模块路径**: `commons/utils/platform/` · **版本**: 0.1.15

面向应用层的跨平台工具集：平台识别、文件系统操作、环境变量与工作目录、路径处理、时间戳与休眠，以及 Windows 网络栈的初始化封装。

## 概述

- **屏蔽 `#ifdef`**：上层业务代码经统一接口操作文件/环境/路径，无需自行编写平台分支。
- **应用层定位**：与顶层 `platform/`（系统层低级抽象）互补，本模块只提供业务逻辑常用的高层工具。
- **字符串返回值**：所有返回 `char *` 的接口（环境变量、cwd、临时路径、路径拼接等）均以 `AIRY_STRDUP` 产出，用 `AIRY_FREE` 释放。
- **子进程执行不在本模块**：统一使用顶层 `platform.h` 的 `airy_process_run_capture()`（fork+execvp，不经 shell）。

## 与顶层 platform/ 模块的区别

| 维度 | 本模块 (`utils/platform/`) | 顶层模块 (`platform/`) |
|---|---|---|
| 抽象层级 | 应用层（high-level） | 系统层（low-level） |
| 核心功能 | 文件系统、环境变量、路径操作 | 线程原语、Socket、时间 |
| 使用场景 | 业务逻辑代码 | 基础设施代码 |
| 典型用户 | cognition、strategy 等模块 | sync、ipc 等底层模块 |

## 目录结构

```
utils/platform/
├── platform_adapter.h   # 27 个 API 的接口定义
├── platform_adapter.c   # 实现（POSIX / Windows 分支）
└── README.md
```

## 核心数据结构

### `platform_type_t` — 平台类型

`PLATFORM_UNKNOWN` · `PLATFORM_WINDOWS` · `PLATFORM_LINUX` · `PLATFORM_MACOS` · `PLATFORM_UNIX`

### `platform_file_info_t` — 文件信息

| 字段 | 类型 | 说明 |
|---|---|---|
| `path` | `const char *` | 查询时传入的路径 |
| `size` | `size_t` | 文件大小（字节），仅普通文件填充 |
| `mtime` | `time_t` | 最后修改时间 |
| `is_directory` | `bool` | 是否为目录 |
| `exists` | `bool` | 路径是否存在 |

## 接口

### 平台信息

| 函数 | 语义 |
|---|---|
| `platform_get_type()` | 返回当前 `platform_type_t` |
| `platform_get_name()` | 返回静态字符串 `"Windows"` / `"Linux"` / `"macOS"` / `"Unix"` / `"Unknown"` |

### 文件系统操作

| 函数 | 语义 |
|---|---|
| `platform_get_file_info(path)` | POSIX 经 `stat`；Windows 经 `FindFirstFileA`；路径非法时返回 `exists=false` 的零值结构 |
| `platform_mkdir(path)` | 创建单层目录（0755，Windows 忽略权限） |
| `platform_mkdir_recursive(path)` | 逐级创建多级目录；已存在的层级跳过（幂等） |
| `platform_unlink(path)` / `platform_rmdir(path)` | 删除文件 / 删除空目录 |
| `platform_copy_file(src, dest)` | POSIX 以 4KB 块流式复制并覆盖目标；Windows 经 `CopyFileA`（bFailIfExists=FALSE） |
| `platform_move_file(src, dest)` | POSIX `rename` / Windows `MoveFileA`；跨设备移动失败返回 false |

### 环境与工作目录

| 函数 | 语义 |
|---|---|
| `platform_get_env(name, default_value)` | 读取环境变量并复制返回；未设置且有默认值时返回默认值副本，未设置且无默认值时返回 `NULL` 并压错误栈 |
| `platform_set_env(name, value)` | POSIX `setenv`（覆盖）；Windows `SetEnvironmentVariableA` |
| `platform_get_cwd()` | POSIX `getcwd(NULL, 0)` 自动分配；Windows 经 Win32 API |
| `platform_chdir(path)` | 切换当前工作目录 |

### 临时文件与路径处理

| 函数 | 语义 |
|---|---|
| `platform_get_temp_dir()` | POSIX 取 `TMPDIR`，否则 `/tmp`；Windows `GetTempPathA` |
| `platform_get_temp_file(prefix)` | 生成临时文件路径；POSIX 下经 `mkstemp` **真实创建文件**后返回路径（`prefix` 为空时用 `"agentrt"`）；Windows `GetTempFileNameA` |
| `platform_path_join(p1, p2)` | 按 `p1` 结尾是否已有分隔符自动补接 |
| `platform_path_normalize(path)` | 当前实现为原样复制返回（不折叠 `.` 与 `..`） |
| `platform_path_basename(path)` / `platform_path_dirname(path)` | 拆分文件名/目录部分；仅识别本平台原生分隔符；`dirname` 对无分隔符路径返回 `"."` |
| `platform_path_exists/is_directory/is_file(path)` | 基于 `platform_get_file_info` 的判定 |

### 系统服务与生命周期

| 函数 | 语义 |
|---|---|
| `platform_get_timestamp_ms()` / `platform_get_timestamp_us()` | Unix 毫秒/微秒时间戳；POSIX 下委托顶层 `airy_time_ms()` / `airy_time_ns()`，Windows 下由 `GetSystemTimeAsFileTime` 换算 |
| `platform_sleep_ms(ms)` | POSIX `usleep` / Windows `Sleep` |
| `platform_adapter_init()` | Windows 下执行 `WSAStartup`；POSIX 下直接返回 true |
| `platform_adapter_cleanup()` | Windows 下 `WSACleanup`；POSIX 下无操作 |

## 语义与约束

- 布尔返回的接口以 `false` 表示失败；需要错误详情时，部分路径（如 `platform_get_env` 未命中且无默认值）会经 commons 错误栈上报，可配合 `airy_err_last()` 查询。
- `platform_path_normalize` 目前仅做字符串复制，不做 `.` / `..` 折叠，勿依赖其消除路径回溯段。
- `platform_path_basename` / `platform_path_dirname` 只按当前平台的原生分隔符切分：POSIX 不识别 `\`，Windows 不识别 `/`。
- Windows 下 `platform_get_file_info` 的 `mtime` 取自 `FILETIME` 低 32 位，超过窗口期会回绕；`size` 仅对非目录填充。
- `platform_get_temp_file` 在 POSIX 下返回的路径对应一个已创建（空）文件，调用方可直接使用或自行删除。
- 返回值需 `AIRY_FREE` 释放的接口不要直接 `free`。

## 用法示例

```c
#include <stdio.h>

#include "platform_adapter.h"
#include "airy_memory.h"

bool ensure_data_dir(char *out_dir, size_t out_cap)
{
    char *base = platform_get_env("AGENTRT_DATA_DIR", "/var/lib/agentrt");
    if (base == NULL) {
        return false;
    }

    char *target = platform_path_join(base, "sessions");
    AIRY_FREE(base);
    if (target == NULL) {
        return false;
    }

    bool ok = platform_mkdir_recursive(target);
    if (ok && out_dir != NULL && out_cap > 0) {
        snprintf(out_dir, out_cap, "%s", target);
    }
    AIRY_FREE(target);
    return ok;
}
```

## 构建与依赖

`platform_adapter.c` 随 `airy_common` 静态库编译（CMake 源列表 `utils/platform/platform_adapter.c`），`platform_adapter.h` 经 PUBLIC include 路径以 `#include "platform_adapter.h"` 消费，并安装至 `include/agentrt/utils/platform/`。

| 依赖 | 用途 |
|---|---|
| commons `utils/memory` | 字符串返回值的分配与复制（`AIRY_STRDUP` 等） |
| commons `utils/error` | 失败路径的错误栈上报（`AIRY_ERROR_NULL`） |
| 顶层 `platform.h` | POSIX 时间戳委托 `airy_time_ms()` / `airy_time_ns()` |
| 平台 API | POSIX（`stat`/`mkdir`/`mkstemp`/`setenv`）与 Windows（Win32 文件/环境/时间 API、Winsock） |

本模块无 agentrt 内部上游依赖（除 commons 自身与顶层 platform 时间接口外）。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
