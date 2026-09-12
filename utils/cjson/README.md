# cjson — cJSON 宏辅助层

**模块路径**: `commons/utils/cjson/` · **版本**: 0.1.15

面向 cJSON 的**纯头文件（header-only）宏辅助层**：把「解析判空、字段提取、作用域释放」的重复样板压缩为一行声明式写法，不改变 cJSON 自身语义。

## 概述

- **header-only，无运行期代码**：整个模块只有 `cjson_helpers.h`，无独立 `.c` 编译单元，不新增链接符号。
- **`AIRY_HAS_CJSON` 门控**：未定义该宏时，头文件仅定义一个空的 `CJSON_AUTO_FREE`，其余宏均不存在，保证未链接 cJSON 的编译目标不会引用未定义符号。
- **RAII 式自动释放**：`CJSON_AUTO_FREE` 基于 GCC/Clang 的 `__attribute__((cleanup))`，变量离开作用域时自动调用 `cJSON_Delete`；在不支持该属性的编译器（如 MSVC）下退化为空宏，需手动释放。
- **失败路径由调用方掌控**：可能失败的宏（`CJSON_PARSE_GUARD` / `CJSON_GET_REQUIRED`）把失败动作交由调用方的 `on_fail` 代码块，可自由配合 `goto cleanup`、`return` 等风格。
- **无共享状态**：宏全部展开为对局部变量的操作，本头文件不引入全局或静态可变状态，无额外线程安全约束。

## 目录结构

```
commons/utils/cjson/
└── cjson_helpers.h    # 唯一文件：辅助宏与配套 static inline 实现（内部自动包含 <cjson/cJSON.h>）
```

## 宏

| 宏 | 语义 |
|----|------|
| `CJSON_PARSE_GUARD(var, text, on_fail)` | 声明一个带 `CJSON_AUTO_FREE` 的 `cJSON *var = cJSON_Parse(text)`；解析结果为 NULL 时执行 `on_fail`，不继续向下。 |
| `CJSON_DEEP_COPY(node)` | 以「序列化后重新解析」方式深拷贝节点；`node` 为 NULL 时返回 NULL。返回新节点由**调用方拥有**，须以 `cJSON_Delete` 释放。 |
| `CJSON_GET_REQUIRED(out, parent, key, on_fail)` | 声明 `cJSON *out = cJSON_GetObjectItem(parent, key)`；字段缺失时执行 `on_fail`。子节点归属 `parent`，**不得单独 `cJSON_Delete`**。 |
| `CJSON_GET_OPTIONAL(out, parent, key)` | 同上但字段缺失仅得 NULL、无失败动作，由调用方决定是否落到默认值。 |
| `CJSON_AUTO_FREE` | 修饰 cJSON 指针声明，令其在作用域结束时自动 `cJSON_Delete`（GCC/Clang cleanup 属性）。 |

`CJSON_DEEP_COPY` 展开为本头文件内的 static inline 实现；序列化产生的中间字符串经 `cJSON_free` 就地释放，不留泄漏。

## `CJSON_AUTO_FREE` 使用约束

- **只用于根节点**（应当被 `cJSON_Delete` 的独立指针）。作用于 `cJSON_GetObjectItem` 返回的子节点会造成随父释放后的二次删除。
- **同作用域多变量按声明逆序（LIFO）释放**，与常见依赖方向一致；但不要跨变量引用已被释放的对象。
- **勿与手动 `cJSON_Delete` 混用**：手动释放后作用域结束仍会触发 cleanup，导致 double free。
- 展开对 NULL 指针安全：指针为 NULL 时作用域结束调用 `cJSON_Delete(NULL)` 无副作用。
- 不支持 cleanup 属性的编译器下该宏为**空**，此时所有对象必须手动释放。

## 用法

```c
#include <cjson_helpers.h>

int handle_request(const char *json_text)
{
    /* 解析失败立即返回；成功则根节点在函数结束自动 Delete */
    CJSON_PARSE_GUARD(root, json_text, return -1);

    /* 必填字段：缺失即失败；子节点随根节点一并释放 */
    CJSON_GET_REQUIRED(id, root, "id", return -1);
    CJSON_GET_REQUIRED(action, root, "action", return -1);

    /* 可选字段：NULL 表示未提供，落到默认值 */
    CJSON_GET_OPTIONAL(timeout, root, "timeout");
    int timeout_ms = (timeout != NULL) ? timeout->valueint : 1000;

    /* 需要脱离 root 生命周期的独立副本：由调用方释放 */
    cJSON *snapshot = CJSON_DEEP_COPY(root);
    if (snapshot != NULL) {
        consume(snapshot); /* consume 接管所有权并负责 cJSON_Delete */
    }

    return dispatch(id, action, timeout_ms);
}
```

在未启用 `AIRY_HAS_CJSON`、或以不支持 cleanup 属性的编译器构建时，上述辅助宏不可用或失去自动释放能力，需改写为完整的 `cJSON_Parse` / NULL 检查 / `cJSON_Delete` 样板。

## 构建与依赖

本模块为 header-only，**不向 `airy_common` 贡献任何编译源文件**；构建系统将 `utils/cjson` 目录作为公共 include（PUBLIC 导出），并随库安装 `cjson_helpers.h`。

外部依赖为**可选的 cJSON 库**：仅当 `AIRY_HAS_CJSON` 选项启用且构建系统成功找到 cJSON 包时，`airy_common` 以 `PRIVATE` 可见性链接 cJSON。使用本宏头的目标需自行保证 cJSON 头文件与库可被找到。

| 依赖 | 类型 | 用途 |
|------|------|------|
| cJSON | 外部库，可选 | 提供宏展开所调用的 `cJSON_Parse` / `cJSON_GetObjectItem` / `cJSON_free` / `cJSON_Delete` 及类型定义 |

本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
