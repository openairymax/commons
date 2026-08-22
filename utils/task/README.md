# task — A-TD 任务描述符

`task` 提供 **A-TD（Airymax Task Descriptor）** 实现，位于 `commons/utils/task`：
任务描述符的创建与完整性校验（真实实现，非桩）。头文件契约在
`airymax/task_desc.h`。

## 设计要点

- **magic** `0x41475453`（`'AGTS'`），独立于 IPC 消息头 magic。
- **CRC32** 覆盖 header[0:72) + payload（与 IPC C-S12 相同算法，IEEE 802.3）。
- **validate()** 检查 magic / version / flags / reserved / payload_len / CRC32。
- **时间戳** 使用单调时钟（monotonic ns），跨平台（POSIX / Win32）。

## API 一览

- `airy_task_desc_crc32(data, len)` — CRC-32 计算（IEEE 802.3）
- 描述符创建与 `validate()` 校验入口见 `airymax/task_desc.h`

## 状态

- **实现**：`src/task_desc.c`（纯 C，跨平台）。
- **测试**：随调用方（atoms taskflow / IPC）集成测试覆盖创建与校验路径。
