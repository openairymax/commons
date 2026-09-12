# quality — 代码质量保障宏与内联工具

**模块路径**: `commons/utils/quality/` · **版本**: 0.1.15

header-only 的代码质量工具集：输入验证宏、goto 清理宏、安全内存/字符串操作、整数溢出检测与安全类型转换，全部以宏或 `static inline` 实现，零运行时调度开销。

## 概述

- **验证即返回**：`AIRY_CHECK_*` 宏把「检查失败 → 返回错误码」压缩为一行，统一函数入口防御风格。
- **goto 清理模式**：`*_GOTO` / `AIRY_SAFE_*` 宏服务于单出口清理标签惯用法，减少资源泄漏。
- **敏感数据清零释放**：`AIRY_SECURE_FREE` 先显式清零再释放，防止密钥、令牌等残留在堆上。
- **溢出安全算术**：`safe_add_* / safe_mul_*` 在运算前检测越界，成功返回 `AIRY_SUCCESS` 并经出参写回结果。

## 目录结构

```
utils/quality/
├── airy_quality.h   # 全部宏与内联函数（header-only，无 .c）
└── README.md
```

## 输入验证宏

失败时直接 `return`（或 `goto`）指定的错误码。

| 宏 | 语义 |
|---|---|
| `AIRY_CHECK_NULL(ptr, err)` / `AIRY_CHECK_NULL_GOTO(ptr, label, err)` | 指针为 NULL 则返回 / 跳转 |
| `AIRY_CHECK_CONDITION(cond, err)` / `AIRY_CHECK_CONDITION_GOTO(cond, label, err)` | 条件不成立则返回 / 跳转 |
| `AIRY_CHECK_RANGE(value, min, max, err)` | 值不在 `[min, max]` 则返回 |
| `AIRY_CHECK_MIN(value, min, err)` / `AIRY_CHECK_MAX(value, max, err)` | 下界 / 上界检查 |
| `AIRY_CHECK_STR_LEN(str, max_len, err)` | 字符串为 NULL 或超长则返回 |
| `AIRY_CHECK_ARRAY_INDEX(index, size, err)`（别名 `AIRY_CHECK_BOUNDS`） | 数组索引越界检查 |
| `AIRY_CHECK_EMPTY(str, err)` | 字符串为 NULL 或空串则返回 |

`*_GOTO` 变体把错误码写入调用作用域中名为 `err` 的整型变量后跳转，因此要求函数内预先声明 `int err`。

## 错误处理与资源管理宏

| 宏 | 语义 |
|---|---|
| `AIRY_SAFE_EXEC(expr, cleanup, err)` | `expr` 非 0 时记录返回值并跳转清理标签 |
| `AIRY_SAFE_ALLOC(var, size, cleanup, err)` / `AIRY_SAFE_CALLOC(...)` | 经 `airy_malloc` / `airy_calloc` 分配，失败置 `err = -1` 并跳转 |
| `AIRY_LOG_ERROR_AND_RETURN(err, fmt, ...)` | 返回错误码（当前实现仅返回，不含实际日志动作） |
| `AIRY_RESOURCE_GUARD_SCOPE_BEGIN()` / `_END()` | 显式作用域块（配合块尾清理惯用法） |
| `AIRY_SAFE_FREE(ptr)` | `airy_free` 后悬挂空指针 |
| `AIRY_SECURE_FREE(ptr, size)` / `AIRY_SECURE_FREE_T(ptr, type)` | 先 `airy_explicit_bzero` 清零再 `free`，并置 NULL |

`AIRY_SECURE_FREE` 直接调用 CRT `free`，只应用于 CRT 分配器取得的内存；清零经由 `airy_explicit_bzero`（volatile 逐字节，防编译器消除）。

## 内联函数

| 组 | 函数 |
|---|---|
| 数值验证 | `airy_validate_non_negative` / `positive` / `percentage`（[0,100]）/ `probability`（[0,1]）/ `priority(min,max)`，返回 `bool` |
| 溢出检测 | `safe_add_int` / `safe_mul_int` / `safe_add_size` / `safe_mul_size(a, b, result)`，成功 `AIRY_SUCCESS`，溢出或出参为 NULL 返回 `AIRY_EINVAL` |
| 访问判定 | `is_safe_array_access(index, size)` / `is_safe_ptr_offset(ptr, offset, size)` / `is_safe_str_copy(src, dest, dest_size)`，返回 `bool` |
| 内存/字符串 | `safe_memcpy` / `safe_memset` / `safe_strcpy` / `safe_strcat`（带边界检查，违规返回 `AIRY_EINVAL`）；`safe_strlen`（NULL 返回 0）；`safe_strcmp`（NULL 按空串参与比较，返回 strcmp 三态而非错误码） |
| 类型转换 | `safe_int_to_size` / `safe_size_to_int` / `safe_double_to_int(value, result)`，越界返回 `AIRY_EINVAL` |
| 清零 | `airy_explicit_bzero(s, n)` |

兼容别名：`airy_safe_strcpy` / `airy_safe_strcat` 分别展开为 `safe_strcpy` / `safe_strcat`。

## 语义与约束

- 本头仅额外 `#include "error.h"`（commons `utils/error`）获取 `AIRY_SUCCESS` / `AIRY_EINVAL`；`bool`、`AIRY_MEMCPY`、`AIRY_MEMSET` 需在包含本头之前已可见（后者由 `utils/memory` 的 `airy_memory_inline.h` 提供）。
- `airy_malloc` / `airy_calloc` / `airy_free` 在本头中仅有声明，实现由 `utils/memory`（`airy_malloc` 族为 static inline 兼容包装）或消费方提供；`AIRY_SAFE_ALLOC` 失败时写 `err = -1` 而非 commons 错误码。
- `AIRY_AUTO_FREE` / `AIRY_AUTO_CLOSE` 引用了本模块未提供的 cleanup 回调（`airy_auto_free` / `airy_auto_close`），当前不可直接使用；需要 GCC `cleanup` 属性自动释放时，请使用 `utils/memory` 提供的 `AUTO_FREE`。
- 头文件未注册进 `airy_common` 的 PUBLIC include 路径，也没有安装规则：消费方需自行把 `utils/quality/` 加入头文件搜索路径后 `#include "airy_quality.h"`。

## 用法示例

```c
#include <stdbool.h>

#include "airy_memory.h"   /* 提供 AIRY_MEMCPY / airy_malloc 等 */
#include "airy_quality.h"

#define NAME_MAX 256

int load_entry(const char *name, size_t count)
{
    int err = AIRY_SUCCESS;
    char *buffer = NULL;

    AIRY_CHECK_NULL(name, AIRY_EINVAL);
    AIRY_CHECK_EMPTY(name, AIRY_EINVAL);
    AIRY_CHECK_STR_LEN(name, NAME_MAX, AIRY_EINVAL);
    AIRY_CHECK_MIN(count, 1, AIRY_EINVAL);

    size_t need;
    if (safe_mul_size(count, sizeof(long), &need) != AIRY_SUCCESS) {
        return AIRY_EINVAL;
    }
    AIRY_SAFE_ALLOC(buffer, need, cleanup, err);

cleanup:
    AIRY_SAFE_FREE(buffer);
    return err;
}
```

## 构建与依赖

无编译单元，不参与 `airy_common` 源列表。

| 依赖 | 用途 |
|---|---|
| commons `utils/error`（`error.h`） | `AIRY_SUCCESS` / `AIRY_EINVAL` 错误码 |
| commons `utils/memory`（包含顺序要求） | `airy_malloc` 族实现与 `AIRY_MEMCPY` / `AIRY_MEMSET` 宏 |
| C 标准库 | `string.h`、`limits.h`、`stddef.h`、`stdint.h` |

commons 测试套件中无本模块专项测试；其消费方位于 agentrt 上层原子模块（自行添加 include 路径）。本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
