# compat — 跨平台兼容层

**模块路径**: `commons/utils/compat/` · **版本**: 0.1.15

编译器与平台差异的**统一收口层**：编译器/平台检测宏、属性与内建包装、对齐与位操作、安全字符串/内存函数、断言设施、Windows 平台的 POSIX 头垫片（`unistd`、`dirent`、`netdb`、`sys/mman`），以及一个轻量 POSIX ERE 正则引擎。

## 概述

- **最底层 include**：`compat.h` 本身仅依赖 `atomic_compat.h` 与 C 标准库，是 commons 内其他模块最先引入的一层。
- **三编译器 + 未知回退**：GCC/Clang、MSVC 分支给出真实属性映射，其余编译器全部退化为无害空操作或不带提示的等价写法，保证「能编译、行为不变」。
- **Windows POSIX 垫片**：`unistd.h`、`netdb.h` 以同名头文件形式提供——Windows 下给出宏与内联实现，非 Windows 下经 `#include_next` 透传系统头，调用方写法跨平台一致。
- **自带 ERE 引擎**：Windows（MSVC 无 `<regex.h>`）下 `regcomp`/`regexec`/`regfree` 映射到内置的 `airy_re_*` 引擎；引擎在所有平台均编译，POSIX 调用方则走系统 `<regex.h>`。
- **断言可接管**：`AIRY_ASSERT` 在 `NDEBUG` 下编译为空；生效时先写日志，再调用注册的自定义 handler（若有），否则按 POSIX/Windows 惯例中止进程。

## 目录结构

```
commons/utils/compat/
├── compat.h                 # 主头：检测/属性/路径/通用工具/断言/版本（594 行）
├── compat.c                 # 断言与版本实现 + Windows 垫片函数（310 行）
├── airy_regex.h             # 正则引擎公共 API 与 POSIX 名称映射（74 行）
├── airy_regex.c             # 引擎 VM 与 regcomp/regexec/regfree（280 行）
├── airy_regex_parse.c       # ERE 语法解析（262 行）
├── airy_regex_compile.c     # AST 编译与字符类表（248 行）
├── airy_regex_internal.h    # 引擎内部结构（不随库安装）
├── unistd.h                 # POSIX <unistd.h> 垫片（146 行）
├── airy_dirent.h            # POSIX dirent 的 Windows 实现（77 行）
├── airy_mman.h              # POSIX sys/mman 的 Windows 实现（122 行）
└── netdb.h                  # POSIX <netdb.h> 垫片（35 行）
```

## 编译器与平台检测

| 宏 | 说明 |
|----|------|
| `AIRY_COMPILER_GCC` / `_CLANG` / `_MSVC` / `_UNKNOWN` | 恰有一个定义为 1；附 `AIRY_COMPILER_NAME`（字符串）与 `AIRY_COMPILER_VERSION` |
| `AIRY_PLATFORM_LINUX` / `_WINDOWS` / `_MACOS` / `_UNKNOWN` | 恰有一个定义为 1；附 `AIRY_PLATFORM_NAME` |
| `AIRY_PATH_SEP` / `AIRY_PATH_SEP_STR` | `\` / `/` 与对应字符串 |
| `AIRY_PATH_MAX` | Windows 260，其余 4096 |
| `AIRY_THREAD_LOCAL` | `__declspec(thread)` / `__thread` |
| `AIRY_VERSION_MAJOR/MINOR/PATCH/STRING` | 由构建系统注入实际版本，未注入时回退头文件默认值；`airy_version_string()` 返回版本串，`airy_build_info()` 返回含编译器/平台/构建时刻的静态信息串 |

## 属性与内建宏

| 宏 | GCC/Clang | MSVC | 其余编译器 |
|----|-----------|------|------------|
| `AIRY_API` | `visibility("default")` | `dllexport`/`dllimport`（按 `AIRY_BUILD_SHARED`/`AIRY_USE_SHARED`），静态库时为空 | 空 |
| `AIRY_INLINE` | `static inline __attribute__((always_inline))` | `static __forceinline` | `static inline` |
| `AIRY_NOINLINE` / `AIRY_UNUSED` / `AIRY_USED` / `AIRY_WEAK` / `AIRY_PACKED` / `AIRY_ALIGNED(x)` / `AIRY_DEPRECATED` / `AIRY_FALLTHROUGH` | 对应 `__attribute__` | `__declspec` 系或空操作 | 全部空操作 |
| `AIRY_PRINTF_FORMAT(fmt,args)` / `AIRY_SCANF_FORMAT` | `format` 属性检查 | 空 | 空 |
| `AIRY_LIKELY(x)` / `AIRY_UNLIKELY(x)` | `__builtin_expect` | 恒等 | 恒等 |
| `AIRY_PREFETCH(x)` / `AIRY_UNREACHABLE()` / `AIRY_ASSUME(x)` | 内建函数 | `__assume` 系 | 空 |
| `AIRY_ATOMIC_FETCH_ADD(ptr,val)` / `_ADD64` | C11 原子 relaxed 加（采样计数等近似场景，基于 `atomic_compat.h`） | 同左 | 同左 |

## 通用工具

| 名称 | 语义 |
|------|------|
| `AIRY_ARRAY_SIZE(arr)` / `AIRY_OFFSETOF(type,m)` / `AIRY_CONTAINER_OF(ptr,type,m)` | 数组长度 / 成员偏移 / 由成员指针反推宿主结构指针 |
| `airy_is_aligned(ptr, align)` | 指针是否按 `align`（2 的幂）对齐 |
| `airy_align_up(v, align)` / `airy_align_down(v, align)` | 向上 / 向下对齐取值 |
| `airy_bit_test/set/clear/flip(x, bit)` | 位测试与改写；写操作对 NULL 指针静默忽略 |
| `airy_popcount(x)` / `airy_clz(x)` / `airy_ctz(x)` | 32 位值的置 1 计数、前导零、尾随零；优先用 `__builtin_*` 或 `_BitScan*`。`clz/ctz` 的输入为 0 时在 GCC/Clang 内建分支未定义，调用方须保证非零 |
| `AIRY_STATIC_ASSERT(cond,msg)` / `AIRY_COMPILE_TIME_ASSERT` / `AIRY_CHECK_SIZE(type,n)` | 编译期断言（C11 下用 `_Static_assert`） |

## 安全字符串 / 内存函数

| 函数 | 语义 |
|------|------|
| `airy_strlcpy(dest, src, dest_size)` / `airy_strlcat(...)` | 边界安全拷贝 / 拼接，成功返回 0、失败非 0；声明于 `compat.h`，实现由同库 platform 域提供 |
| `airy_strncpy_safe(dest, src, dest_size)` | 截断拷贝并保证 NUL 终止，返回 `dest`；参数 NULL 或 `dest_size==0` 时原样返回 |
| `airy_memset_s(dest, c, dest_size, count)` / `airy_memcpy_s` / `airy_memmove_s` | 越界（`count > dest_size`）或 NULL 返回 `AIRY_EINVAL`；`memcpy_s` 检测到区域重叠时自动改用 `memmove` |

## 断言与调试

- `AIRY_ASSERT(cond)` / `AIRY_ASSERT_MSG(cond, msg)`：`NDEBUG` 下编译为空。失败时经日志记录条件与位置；随后若已通过 `airy_set_assert_handler` 注册回调则仅调用回调（可优雅降级），否则 raise `SIGABRT`（Windows 在附加调试器时先 `DebugBreak`）并 `abort()`。`airy_get_assert_handler` 读取当前回调。
- `AIRY_DEBUG_BREAK()` / `airy_debug_break()`：仅 `DEBUG` 宏开启时展开为断点中断。

## POSIX 兼容垫片（仅 Windows 分支生效）

| 头 | 内容 |
|----|------|
| `unistd.h` | `STDIN/STDOUT/STDERR_FILENO`、`F_OK/R_OK/W_OK/X_OK`、`pid_t/uid_t/gid_t/ssize_t`；`sleep/usleep/getpid/getuid/read/write/close/access/unlink/lseek/isatty/chdir/getcwd/exec*/pipe/fsync/ftruncate` 等宏映射；`sysconf()` 与 `getentropy()`（转调平台熵源）为真实实现；`readlink/symlink/truncate/fork/setsid` 无对应语义、恒返回 -1，`ttyname*` 恒 NULL |
| `airy_dirent.h` | `opendir`/`readdir`/`closedir` 基于 `FindFirstFileA`/`FindNextFileA`，`struct dirent` 仅含 `d_name`（260 字符） |
| `airy_mman.h` | `mmap`（文件映射或 `MAP_ANONYMOUS` 走 `VirtualAlloc`）、`munmap`、`mprotect`；`shm_open`/`shm_unlink` 无 Windows 对应概念，按 POSIX 失败约定恒返回 -1；附 `PROT_*`/`MAP_*` 常量与 `off_t/mode_t` typedef |
| `netdb.h` | 引入 Winsock2 与 `ws2tcpip.h`（`getaddrinfo` 族由系统提供） |

`compat.h` 的 Windows 分支另提供 `strcasecmp/strncasecmp/strdup` 映射、`CLOCK_REALTIME/CLOCK_MONOTONIC` 常量，并在 `compat.c` 实现 `nanosleep`、`clock_gettime`（单调钟走性能计数器、实时钟走系统文件时间）、`setenv`、`strndup`、`localtime_r`、`sysconf` 等非 MSVC 运行库缺失的 POSIX 函数（`strtok_r` 仅非 MSVC 分支提供，MSVC/UCRT 已有原生 `strtok_s`）。

## ERE 正则引擎

```c
#include <airy_regex.h>

int name_matches(const char *pattern, const char *text)
{
    regex_t re; /* Windows：映射到内置 airy_re_* 引擎；POSIX：系统 regex_t */

    if (regcomp(&re, pattern, REG_EXTENDED) != 0) {
        return -1;
    }
    int matched = (regexec(&re, text, 0, NULL, 0) == 0);
    regfree(&re);
    return matched;
}
```

- 引擎原生 API 为 `airy_re_regcomp` / `airy_re_regexec` / `airy_re_regfree`，操作不透明的 `airy_regex_t`（引擎自持 AST、指令表与字符类表），匹配结果经 `airy_regmatch_t{rm_so, rm_eo}` 返回。
- 支持范围：字面量、`.`、字符类（区间与取反）、锚点 `^ $`、量词 `* + ? {n,m}`（含非贪婪变体）、分支 `|`、最多 9 组捕获、转义。
- POSIX 名称 `regex_t/regcomp/regexec/regfree/REG_EXTENDED/REG_NOSUB/REG_NOMATCH` 仅在 `_WIN32` 下映射到引擎；非 Windows 下本头直接包含系统 `<regex.h>`，引擎仍随库编译以供单元测试。

## 构建与依赖

`compat.c` 与三个 `airy_regex_*.c` 编译进静态库 `airy_common`；`utils/compat/` 整目录作为公共 include（PUBLIC 导出）并随库安装 `*.h`（排除 `*_internal.h`）。

| 依赖 | 类型 | 用途 |
|------|------|------|
| `atomic_compat.h`（`utils/include/`） | 同库头 | C11 原子操作包装 |
| logging / error | 同库模块 | 断言失败日志与 Windows 垫片错误上报（仅 `compat.c`） |
| platform | 同库模块 | `airy_strlcpy` / `airy_strlcat` 实现与熵源后端 |
| Windows SDK | 外部（仅 Windows） | 文件映射、进程/计时、Winsock |
| C 标准库 | 外部 | — |

本模块无 agentrt 内部上游依赖（同库内模块除外）。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
