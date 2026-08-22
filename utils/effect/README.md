# airy_effect — 回卷机制（统一作用域效应原语）

`airy_effect` 是 AirymaxRT 的**可逆组合语义**原语（"注册即副作用、逆序回滚"），
位于 `commons/utils/effect`。它是"除了核心，一切都是服务"工程化的第一块基石：
任何外部能力（daemon 方法、白名单、工具、资源句柄）的挂载都应以 effect 登记，
卸载时逆序回滚，保证大规模拔插不残留、不污染核心。

## 设计原则

- **注册即副作用**：调用 `airy_effect_add` 时副作用**已经发生**，该调用只登记
  撤销动作（disposer），不执行任何实际逻辑。
- **逆序回滚**：失败/回滚路径上，已登记的 disposer 按**注册逆序**执行
  （先打开的资源后释放），跨模块撤销依赖得以保持。
- **成功即提交**：成功路径上 `airy_effect_commit` 清空作用域但**不执行**
  disposer（副作用保留，无需撤销）。
- **单线程契约**：一个作用域对应一个流程/请求，无内部锁；回滚/提交后作用域
  清空并可复用。

## API 一览

| 函数 | 说明 |
|------|------|
| `airy_effect_create(&scope)` | 创建空作用域（OWNER，需 destroy） |
| `airy_effect_add(scope, disposer, ctx)` | 登记撤销动作（注册即副作用） |
| `airy_effect_rollback(scope)` | 逆序执行全部 disposer 并清空（失败路径） |
| `airy_effect_commit(scope)` | 清空但**不执行** disposer（成功路径） |
| `airy_effect_dispose(scope, ctx)` | 按 ctx 提前单个注销（最晚注册者匹配） |
| `airy_effect_count(scope)` | 已登记 disposer 数量 |
| `airy_effect_destroy(scope)` | 释放；若仍有未回滚/未提交的 disposer，先逆序执行 |

## 使用示例

```c
airy_effect_t *fx = NULL;
if (airy_effect_create(&fx) != AIRY_EOK) return;
airy_effect_add(fx, my_undo, my_handle);   /* 副作用已发生，登记撤销 */
if (!do_step()) {                          /* 失败：逆序回滚全部 */
    airy_effect_rollback(fx);
    airy_effect_destroy(fx);
    return;
}
airy_effect_commit(fx);                    /* 成功：保留副作用 */
airy_effect_destroy(fx);
```

## 状态

- **实现**：`src/airy_effect.c` + `include/airy_effect.h`，纯 C，跨 Linux/macOS/Windows。
- **测试**：commons 单元测试覆盖（create/add/rollback/commit/dispose/count/destroy）。
- **演进**：已作为请求级原语验证；P0 计划推广为 daemon 级作用域（见内部文档
  `closed-docs/agentrt/07-subsystem-specs/13-module-boundaries.md` 回卷机制）。
