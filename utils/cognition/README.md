# cognition — 执行体角色词汇表 SSoT

**模块路径**: `commons/utils/cognition/` · **版本**: 0.1.19

认知层（规划、调度、协调）共享的**执行体角色词汇表唯一权威点**（Single Source of Truth）：规范角色名、别名映射与只读角色集只在此处定义，任何模块做角色判定或赋值都必须引用本头文件导出的宏。

## 概述

- **纯词表模块**：仅含常量宏与三个纯函数，无状态、无锁、无动态分配。
- **严格解析与驱动归一两分**：安全判定用 `agent_vocab_resolve`（未登记即 NULL，fail-closed）；计划驱动用 `agent_vocab_canonical`（未登记回落兜底，保证计划可执行）。

## 目录结构

```
commons/utils/cognition/
├── README.md
├── agent_vocab.h         # 执行体角色词汇表 SSoT 声明（62 行）
└── agent_vocab.c         # 角色归一化与只读判定实现（98 行）
```

## 角色词汇表 SSoT

- `AGENT_VOCAB_ROLE_*`：11 个规范角色名（`product_manager` / `architect` / `backend` / `frontend` / `devops` / `security` / `tester` / `coding` / `data_engineer` / `reviewer` / `analyst`）。
- `AGENT_VOCAB_FALLBACK`：无角色信息时的兜底执行体（`coding`）。
- `agent_vocab_resolve(name)`：严格解析——仅命中规范名或别名时返回规范名，**未登记角色返回 NULL**（不回落兜底）。权限授予、只读隔离等安全判定必须用它。
- `agent_vocab_canonical(name)`：驱动用归一化——`resolve` 结果为空时回落 `AGENT_VOCAB_FALLBACK`，保证计划总能被驱动；**不得用于安全判定**（会把未登记角色当成默认执行体而 fail-open）。
- `agent_vocab_is_readonly(name)`：判定归一化后的角色是否只读（`tester` / `reviewer`）。

`daemons/common/include/agent_vocab.h` 仅为重导出兼容头，指向本目录实现；不得在别处再建同名表或重复实现归一化逻辑。该约束由发布门禁 S 组（`scripts/verify_release_gates.sh`）自动守护：S1 断言角色字面量零外泄，S2 断言语义权威点唯一与归一化边界单点。

## 构建与依赖

`agent_vocab.c` 编译进静态库 `airy_common`；`utils/cognition/` 作为公共 include 目录（PUBLIC 导出），头文件随库安装（`*.h`，排除 `*_internal.h`）。本模块无 agentrt 内部上游依赖，仅依赖 C 标准库（`string.h`）。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
