# execution — 任务检查点（checkpoint）

`execution` 提供 **AgentRT 任务检查点接口**，位于 `commons/utils/execution`：
将任务执行现场（已完成/待办节点 + 状态 JSON）持久化，支持中断恢复与统计上报。

## 核心概念

- **检查点状态机**：`PENDING` → `COMPLETED` / `FAILED` / `INVALID`。
- **现场数据**：task_id / session_id / sequence_num / timestamp / state_json /
  已完成节点 / 待办节点 / checksum / metadata。
- **持久化**：快照写入 + 会话管理（`checkpoint_persist.c` / `checkpoint_session.c` /
  `checkpoint_snapshot.c`）。
- **钩子**：`airy_checkpoint_hook_fn` 检查点事件回调。

## API 一览

- `airy_checkpoint_create(...)` — 创建检查点（task/session/sequence/state_json/节点）
- 恢复、统计（`airy_checkpoint_stats_t`）、钩子注册等见 `include/checkpoint.h`

## 状态

- **实现**：`src/checkpoint.c` + `checkpoint_persist.c` + `checkpoint_session.c` +
  `checkpoint_snapshot.c`（真实实现）。
- **测试**：commons 单元测试覆盖创建/持久化/恢复/统计。
