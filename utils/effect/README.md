# effect — 作用域效应原语

**模块路径**: `commons/utils/effect/` · **版本**: 0.1.15

「注册即副作用、逆序回滚」的轻量作用域原语：把一组撤销动作（disposer）登记进一个作用域，失败路径按注册逆序统一回滚，成功路径提交保留。

## 概述

- **注册即副作用**：`airy_effect_add` 只记录「如何撤销」，本身不执行任何逻辑——到达登记点时副作用已经发生。
- **逆序回滚**：回滚时按注册的**逆序**执行 disposer（最后登记的最先撤销），保证先获取的资源后释放，跨模块的撤销依赖成立。
- **成功即提交**：`airy_effect_commit` 只清空作用域，**不执行** disposer，副作用保留。
- **安全默认**：`airy_effect_destroy` 若发现仍有未回滚/未提交的条目，先逆序执行再释放内存。
- **单线程契约**：一个作用域对应一个流程/请求，内部无锁；回滚或提交后作用域为空、可继续复用。
- 底层为可增长数组（初始容量 4，倍增扩容），条目压缩保序。

## 目录结构

```
effect/
├── airy_effect.h   (127 行)  公开接口与语义契约
└── airy_effect.c   (130 行)  数组型作用域实现
```

## 数据结构

- `airy_effect_t`：不透明作用域句柄，经 `airy_effect_create` 创建、`airy_effect_destroy` 释放。
- `airy_effect_disposer_t`：撤销回调 `void (*)(void *ctx)`，`ctx` 为登记时捕获的用户上下文（可为 `NULL`）。

## 接口

| 函数 | 语义 |
|------|------|
| `airy_effect_create(&scope)` | 创建空作用域；`AIRY_EOK` 成功，`AIRY_EINVAL` 参数非法，`AIRY_ENOMEM` 分配失败 |
| `airy_effect_add(scope, disposer, ctx)` | 登记撤销动作；`AIRY_EOK` / `AIRY_EINVAL`（scope 或 disposer 为 `NULL`）/ `AIRY_ENOMEM` |
| `airy_effect_rollback(scope)` | 逆序执行全部 disposer 并清空；作用域保持可用（`NULL` 安全） |
| `airy_effect_commit(scope)` | 清空但**不执行** disposer（`NULL` 安全） |
| `airy_effect_dispose(scope, ctx)` | 按 `ctx` 指针提前撤销单个条目：自最晚登记向早前匹配，命中即执行其 disposer 并从作用域移除，后续 rollback/commit/destroy 不再触及 |
| `airy_effect_count(scope)` | 当前已登记条目数（`NULL` 返回 0） |
| `airy_effect_destroy(scope)` | 释放作用域；若有未处理条目先逆序回滚；`NULL` 安全 |

## 用法示例

```c
airy_effect_t *fx = NULL;
if (airy_effect_create(&fx) != AIRY_EOK) {
    return -1;
}

/* 副作用已发生，登记撤销动作 */
airy_effect_add(fx, close_handle, h1);
airy_effect_add(fx, release_slot, slot);

if (!do_step()) {
    /* 失败：按 release_slot → close_handle 逆序回滚 */
    airy_effect_rollback(fx);
    airy_effect_destroy(fx);
    return -1;
}

/* 成功：保留全部副作用 */
airy_effect_commit(fx);
airy_effect_destroy(fx);
```

## 构建与依赖

`airy_effect.c` 随 `airy_common` 静态库编译；`airy_effect.h` 随库以 PUBLIC 方式导出（`include/agentrt/utils/effect/`）。

| 依赖 | 用途 |
|------|------|
| `error` | `airy_err_t` 与 `AIRY_EOK` / `AIRY_EINVAL` / `AIRY_ENOMEM` |
| `memory` | `AIRY_CALLOC` / `AIRY_REALLOC` / `AIRY_FREE` |
| 标准库 | `stdlib` |

纯 C 实现，跨 Linux/macOS/Windows。本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*

*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
