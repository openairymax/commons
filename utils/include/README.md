# include — 共享头目录

**模块路径**: `commons/utils/include/` · **版本**: 0.1.15

commons 的 header-only 共享头目录：跨平台原子操作兼容层、`AIRY_LOG_*` 兼容转发头与检查宏。本目录不产生编译单元，作为 `airy_common` 的公开接口搜索路径导出，供各模块与下游直接引用。

## 目录结构

```
utils/include/
├── atomic_compat.h            聚合入口：一次包含即获得完整原子兼容层
├── atomic_compat_platform.h   平台选择与底层原子原语（C11 / GCC 内建 / Windows）
├── atomic_compat_api.h        统一原子类型与操作 API
├── logging_compat.h           AIRY_LOG_* 宏兼容转发头（旧 include 路径入口）
├── check.h                    参数校验 / 错误跳转 / 资源分配检查宏
└── README.md
```

## 原子操作兼容层（atomic_compat*.h）

将 C11 `<stdatomic.h>`、GCC/Clang `__atomic` 内建与 Windows `Interlocked` API 统一为一组 `atomic_load_64` / `atomic_compare_exchange_strong_ptr` 等带位宽后缀的接口，并提供 `atomic_int`、`atomic_uint64_t`、`atomic_double` 等类型别名与 `memory_order_*` 常量。消费方只需包含聚合入口 `atomic_compat.h`。

完整的操作矩阵、类型别名与内存顺序说明见 [utils/compat README](../compat/README.md)（该模块文档承载原子兼容层的 API 细节）。

## 日志宏转发头（logging_compat.h）

`AIRY_LOG_ERROR / WARN / INFO / DEBUG / FATAL` 的兼容入口：当 observability 模块的权威 `logger.h` 可感知时由其提供定义；否则本头回退为直接写 stderr 的同名宏（`[AIRY][级别] 文件:行 函数: 消息` 格式，`FATAL` 输出后 `abort()`）。新代码应直接使用 `observability/logger.h` 的权威定义。

## 检查宏（check.h）

共 18 个宏，分三组。所有宏无运行时副作用，可在任意线程使用；`*_RET` 组要求所在函数以 `airy_err_t` 返回，`*_GOTO` / `*_CHECK` 组要求作用域内存在目标清理标签。

**校验并返回**：

| 宏 | 语义 |
|---|---|
| `CHECK_NULL_RET(ptr, err)` | `ptr == NULL` 时 `return err` |
| `CHECK_NULL(ptr)` | 同上，固定返回 `AIRY_EINVAL` |
| `CHECK_COND_RET(expr, err)` | `expr` 为假时 `return err` |
| `CHECK_COND(expr)` | 同上，固定返回 `AIRY_EINVAL` |
| `CHECK_ERR_RET(call, var)` | 以 `airy_err_t var = call` 承接调用结果，非 `AIRY_SUCCESS` 时原样返回 |
| `CHECK_RANGE_RET(value, min, max, err)` | `value` 不在闭区间 `[min, max]` 时 `return err` |
| `CHECK_NONZERO_RET(value, err)` | `value == 0` 时 `return err` |
| `CHECK_STRING_RET(str, err)` | `str` 为 NULL 或空串时 `return err` |

**校验并跳转**：

| 宏 | 语义 |
|---|---|
| `CHECK_ERR_GOTO(call, var, label)` | 调用失败时 `goto label`（错误值保留在 `var`） |
| `CHECK_NULL_GOTO(ptr, label)` | `ptr == NULL` 时 `goto label` |
| `CHECK_NULL_GOTO_ERR(ptr, label, var, err)` | `ptr == NULL` 时置 `var = err` 并跳转 |

**资源分配检查**（失败跳转或置错后跳转）：

| 宏 | 语义 |
|---|---|
| `SAFE_FREE(ptr)` | 非 NULL 时 `AIRY_FREE(ptr)` 并置 NULL |
| `ALLOC_CHECK(var, size, label)` | `AIRY_MALLOC` 失败跳转 |
| `CALLOC_CHECK(var, count, size, label)` | `AIRY_CALLOC` 失败跳转 |
| `STRDUP_CHECK(dest, src, label)` | `AIRY_STRDUP` 失败跳转 |
| `MALLOC_CHECK_ERR(var, size, label, err_var, err)` | 分配失败置错误码并跳转 |
| `CALLOC_CHECK_ERR(var, count, size, label, err_var, err)` | 同上（calloc） |
| `STRDUP_CHECK_ERR(dest, src, label, err_var, err)` | 同上（strdup） |

**使用前提**：`check.h` 本身仅包含 `airy_types.h`（`airy_err_t`、`AIRY_SUCCESS`、`AIRY_EINVAL` 等）；`AIRY_MALLOC` / `AIRY_FREE` / `AIRY_CALLOC` / `AIRY_STRDUP` 在宏体内于使用点展开，调用方所在翻译元必须先包含 `airy_memory.h`。

## 用法示例

```c
#include "airy_memory.h" /* 分配宏的展开前提 */
#include "check.h"

airy_err_t load_name(const char *path, char **out_name)
{
    airy_err_t ret = AIRY_SUCCESS;
    CHECK_STRING_RET(path, AIRY_EINVAL);
    CHECK_NULL_RET(out_name, AIRY_EINVAL);

    char *copy = NULL;
    STRDUP_CHECK_ERR(copy, path, cleanup, ret, AIRY_ENOMEM);

    *out_name = copy;
    return AIRY_SUCCESS;

cleanup:
    SAFE_FREE(copy);
    return ret;
}
```

## 构建与依赖

本目录为纯头文件，无编译产物；作为 `airy_common` 目标的公开（PUBLIC）头搜索路径导出。

| 头 | 依赖 |
|---|---|
| `atomic_compat*.h` | 仅标准头与编译器内建，无项目内依赖 |
| `logging_compat.h` | 可选转发到 `observability/logger.h`，回退分支仅用 stdio/stdlib |
| `check.h` | `airy_types.h`（类型与错误码）；分配宏使用点需可见 `airy_memory.h` |

本目录无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
