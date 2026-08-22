# id — 品牌化 ID 生成

`id` 提供**品牌化 ID 实现**（阶段 3），位于 `commons/utils/id`：为
trace_id / msg_id 提供结构化命名。类型声明见 `commons/include/airy_types.h`
（品牌化 ID 段）。

## ID 规范

| ID | 命名 | 构造 |
|----|------|------|
| `trace_id` | `tr-<16 hex>`（W3C 风格） | 64 位熵：splitmix64 混合（时间 + ASLR 地址 + 状态） |
| `msg_id` | `msg-<ts:08x>-<seq:08x>` | 高 32 位秒时间戳 \| 低 32 位进程内单调序列（C11 原子） |

## API 一览

- `airy_trace_id_generate()` — 生成 trace_id（64 位熵，线程安全）
- `airy_trace_id_eq(a, b)` — trace_id 相等比较
- `airy_msg_id_generate()` — 生成 msg_id（时间戳 + 单调序列）
- 其余见 `airy_types.h` 品牌化 ID 段声明

## 状态

- **实现**：`src/airy_id.c`（纯 C，C11 原子，线程安全）。
- **测试**：commons 单元测试覆盖生成/比较/唯一性。
