# IPC — 守护进程 RPC 与通信可靠性组件

**模块路径**: `commons/utils/ipc/`
**版本**: 0.1.20

## 概述

本模块是 AgentRT 进程间通信的唯一实现面：**通道形态为 Unix Domain Socket 上的
JSON-RPC 2.0**（守护进程 `daemon_rpc_client` 为唯一客户端），其余文件提供与通道形态
解耦的协议与可靠性组件（JSON-RPC 报文构造、方法分发、参数校验、熔断、退避与聚合）。

wire 契约（`struct airy_ipc_msg_hdr` / `AIRY_IPC_MAGIC`）位于 `commons/include/airymax/ipc.h`，
本模块只消费该契约，不另立格式。

## 0.1.20 退役说明

0.1.16 A-IPC 收口后，旧「通道族」（`ipc_common.c` / `ipc_common_io.c` /
`ipc_common_message.c` / `ipc_server_client.c` / `ipc_shm.c` / `ipc_mq.c` / `ipc_rpc.c`
及其 11 个 API 头）已无任何生产消费者（daemons / gateway / atoms / heapstore /
products / sdk 全零扇入，仅自身单测自循环），且其 POSIX-only 条件编译与
「Windows / Linux / macOS 三端等价可用」目标背离。该族于 0.1.20 整件退役，
守护进程 RPC 成为单一 SSoT；对应单测随实现一并退役。

## 目录结构

```
ipc/
├── daemon_rpc_client.h      # 守护进程 RPC 客户端契约（socket 路径 + 方法 + 参数）
├── daemon_rpc_req.c         # 请求封装、超时分级、可取消调用与流式调用实现
├── daemon_replies.h / .c    # 守护进程标准应答（health / stats 信封）
├── jsonrpc_helpers.h / .c   # JSON-RPC 2.0 报文构造与解析
├── method_dispatcher.h / .c # 方法注册表与分发
├── param_validator.h / .c   # JSON-RPC 参数校验
├── circuit_breaker.h / .c   # 熔断器（仅依赖互斥锁与标准库）
├── ipc_backoff.h / .c       # 指数退避与滑动窗口聚合
└── README.md
```

## 接口要点

| 组件 | 主要接口 | 说明 |
|------|----------|------|
| 客户端调用 | `daemon_rpc_call` / `daemon_rpc_call_cancelable` / `daemon_rpc_call_stream` | 阻塞、可取消、流式三种形态；超时由 `rpc_timeout_for(method)` 按方法分级 |
| 报文构造 | `daemon_rpc_json_req` | 生成 `{"jsonrpc":"2.0","id":…,"method":…,"params":…}` |
| 标准应答 | `reply_health` / `reply_stats` | 守护进程健康与统计的固定信封 |
| 方法分发 | `md_*` | 名称 → 处理函数注册表，未注册方法回 `-32601` |
| 参数校验 | `pv_*` | 按 schema 校验必填/类型，失败回 `-32602` 并携带参数契约 |
| 熔断 | `cb_*` | 连续失败达阈值后快速失败，半开探测恢复 |
| 退避/聚合 | `ipc_bkoff_*` / `ipc_agg_*` | 重连退避（base→cap）与按窗口的速率聚合 |

## 平台可用性

| 组件 | Linux | macOS | Windows |
|------|-------|-------|---------|
| 守护进程 RPC（socket + JSON-RPC） | ✓ | ✓ | ✓ |
| JSON-RPC / 分发 / 校验 / 熔断 / 退避 | ✓ | ✓ | ✓ |

Socket 抽象由 `commons/platform` 提供（Windows 走 AF_UNIX 兼容层），本模块不含平台分支。

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `airymax/ipc.h` | 共享 wire 契约头与魔数 |
| `error.h` / `types.h` | 错误码与基础类型 |
| `platform.h` | 互斥锁、socket、时间等平台原语 |

---

© 2025-2026 SPHARX Ltd.

SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0（双许可，任选其一遵守，完整文本见仓库根 [LICENSE](../../LICENSE)）
