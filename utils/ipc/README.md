# IPC — 进程间通信模块

**模块路径**: `commons/utils/ipc/`
**版本**: 0.1.15

## 概述

IPC 模块提供进程间通信抽象层，支持管道、命名管道、Unix Domain Socket、共享内存、消息队列和 RPC 调用框架。该模块是 AgentRT 中组件间通信的基础设施，统一消息格式和协议，支持同步和异步通信模式。

通道、服务端/客户端、共享内存、消息队列与 RPC 的传输层实现基于 POSIX 机制，仅在非 Windows 平台编译；JSON-RPC 辅助、方法分发器、参数校验与熔断器等协议/可靠性组件全平台可用。

## 设计目标

- **统一消息格式**：标准化消息头与负载结构，支持 CRC32 校验、请求-响应关联和序列化
- **跨边界布局兼容**：wire 格式前 128 字节复用共享契约头 `struct airy_ipc_msg_hdr`（`include/airymax/ipc.h`），与内核态快速路径逐字节兼容
- **同步与异步**：支持阻塞、非阻塞和回调模式的消息收发
- **内置可靠性**：超时控制、事件通知、统计信息与熔断器
- **RPC 框架**：基于传输通道的远程过程调用，支持方法注册和同步调用

## 目录结构

模块目录为平铺结构（无 `include/`、`src/` 子目录），按角色分组：

```
ipc/
├── ipc_common.h                  # 聚合入口（包含下列 9 个拆分 API 头）
│
│  # —— 公共 API 头（按域拆分）——
├── ipc_types.h                   # 类型、枚举、常量与消息头定义
├── ipc_core_api.h                # 子系统初始化与通道/收发核心 API
├── ipc_server_api.h              # 服务端 API
├── ipc_client_api.h              # 客户端 API
├── ipc_shm_api.h                 # 共享内存 API
├── ipc_mq_api.h                  # 消息队列 API
├── ipc_message_api.h             # 消息构造/序列化辅助 API
├── ipc_rpc_api.h                 # RPC 框架 API
├── ipc_util_api.h                # 工具 API（错误消息、有效性检查、刷新）
│
│  # —— 内部与守护进程辅助头 ——
├── ipc_common_internal.h         # 实现内部共享定义
├── svc_common.h                  # 服务通用定义
├── daemon_bootstrap_ipc.h        # 守护进程 IPC 引导辅助
├── daemon_rpc_client.h           # 守护进程 RPC 客户端辅助
├── ipc_bus_helper.h              # 服务总线辅助
├── ipc_service_bus.h             # 服务总线接口
│
│  # —— 全平台组件 ——
├── circuit_breaker.h / .c        # 熔断器（仅依赖互斥锁与标准库）
├── jsonrpc_helpers.h / .c        # JSON-RPC 2.0 辅助
├── method_dispatcher.h / .c      # 方法分发器（注册表模式）
├── param_validator.h / .c        # JSON-RPC 参数校验
│
│  # —— POSIX 传输层实现（NOT WIN32 编译）——
├── ipc_common.c                  # 通道与核心机制实现
├── ipc_common_io.c               # 消息收发域
├── ipc_common_message.c          # 消息构造与序列化域
├── ipc_server_client.c           # 服务端/客户端实现
├── ipc_shm.c                     # 共享内存实现
├── ipc_mq.c                      # 消息队列实现
├── ipc_rpc.c                     # RPC 框架实现
└── README.md                     # 本文档
```

## 常量与枚举（ipc_types.h）

### 常量

| 宏 | 值 | 说明 |
|------|------|------|
| `IPC_MAGIC` | `AIRY_IPC_MAGIC`（`0x41524531`，即 `'ARE1'`） | 消息魔数（复用标准头字段） |
| `IPC_DEFAULT_TIMEOUT_MS` | 5000 | 默认超时 |
| `IPC_MAX_MESSAGE_SIZE` | 1MB | 最大消息大小 |
| `IPC_DEFAULT_BUFFER_SIZE` | 65536 | 默认缓冲区大小 |
| `IPC_MAX_NAME_LEN` | 256 | 通道名最大长度 |
| `IPC_MAX_CONNECTIONS` | 128 | 服务端默认最大连接数 |
| `IPC_MESSAGE_ALIGN` | 8 | 消息对齐 |

### 枚举

| 类型 | 取值 |
|------|------|
| `ipc_type_t` | `IPC_TYPE_PIPE` / `NAMED_PIPE` / `SOCKET` / `SHM` / `MQ` / `RPC` |
| `ipc_mode_t` | `IPC_MODE_READ` / `WRITE` / `READ_WRITE` |
| `ipc_state_t` | `IPC_STATE_CLOSED` / `OPENING` / `OPEN` / `CLOSING` / `ERROR` |
| `ipc_flag_t` | `IPC_FLAG_NONE` / `NONBLOCK` / `PRIORITY` / `BROADCAST` / `EXCLUSIVE` / `PERSISTENT` |
| `ipc_msg_type_t` | `IPC_MSG_DATA` / `REQUEST` / `RESPONSE` / `NOTIFICATION` / `ERROR` / `CONTROL` |
| `ipc_event_t` | `IPC_EVENT_CONNECTED` / `DISCONNECTED` / `MESSAGE` / `ERROR` / `TIMEOUT` / `BUFFER_FULL` / `BUFFER_EMPTY` |

回调类型：`ipc_event_callback_t`（通道事件）、`ipc_message_callback_t`（消息到达，返回 0 继续处理）。

## 核心数据结构

### ipc_message_header_t — 消息头

wire 格式前 128 字节为共享契约标准头，其后为 IPC 模块扩展段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `aipc` | `struct airy_ipc_msg_hdr` | 128B 标准头（offset 0，含 `magic` / `payload_len` / `crc32` 等） |
| `version` | `uint32_t` | IPC 协议版本 |
| `type` | `uint32_t` | 消息类型（`IPC_MSG_*`） |
| `flags` | `uint32_t` | 消息标志（`IPC_FLAG_*`） |
| `msg_id` | `uint64_t` | 消息 ID |
| `correlation_id` | `uint64_t` | 关联 ID（请求-响应配对） |
| `source` | `char[64]` | 发送者标识 |
| `target` | `char[64]` | 目标标识 |
| `checksum` | `uint32_t` | 校验和（header_crc ^ payload_crc） |
| `timestamp` | `airy_timestamp_t` | 时间戳（纳秒） |
| `reserved` | `uint8_t[32]` | 保留 |

兼容访问宏：`IPC_HDR_MAGIC(h)`、`IPC_HDR_PAYLOAD_LEN(h)`、`IPC_HDR_CRC32(h)` 复用标准头字段。

### ipc_message_t — 消息结构

| 字段 | 类型 | 说明 |
|------|------|------|
| `header` | `ipc_message_header_t` | 消息头 |
| `payload` | `void *` | 负载数据 |
| `payload_size` | `size_t` | 负载大小 |

### ipc_config_t — 通道配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `ipc_type_t` | 通道类型 |
| `name` | `const char *` | 通道名称 |
| `mode` | `ipc_mode_t` | 读写模式 |
| `buffer_size` | `size_t` | 缓冲区大小（默认 65536） |
| `max_message_size` | `size_t` | 最大消息大小（默认 1MB） |
| `timeout_ms` | `uint32_t` | 默认超时（默认 5000ms） |
| `max_connections` | `uint32_t` | 最大连接数（服务端，默认 128） |
| `nonblocking` | `bool` | 是否非阻塞 |
| `persistent` | `bool` | 是否持久化 |
| `permissions` | `const char *` | 权限设置（Unix 权限字符串） |

### ipc_stats_t — 统计信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `messages_sent` / `messages_received` | `uint64_t` | 已发送 / 已接收消息数 |
| `bytes_sent` / `bytes_received` | `uint64_t` | 已发送 / 已接收字节数 |
| `errors` / `timeouts` | `uint64_t` | 错误 / 超时次数 |
| `avg_latency_us` / `max_latency_us` | `uint64_t` | 平均 / 最大延迟（微秒） |

句柄类型（不透明指针）：`ipc_channel_t`、`ipc_server_t`、`ipc_client_t`，以及 shm/mq/rpc 域的 `ipc_shm_t`、`ipc_mq_t`、`ipc_rpc_server_t`、`ipc_rpc_client_t`。

## 接口说明

### 初始化与核心 API（ipc_core_api.h）

| 函数 | 说明 |
|------|------|
| `ipc_init()` / `ipc_cleanup()` | 初始化 / 清理 IPC 子系统 |
| `ipc_create_default_config(type)` | 创建默认 IPC 配置 |
| `ipc_channel_create(config)` / `ipc_channel_destroy(channel)` | 创建 / 销毁通道 |
| `ipc_channel_open(channel)` / `ipc_channel_close(channel)` | 打开 / 关闭通道 |
| `ipc_channel_get_state(channel)` / `get_name` / `get_type` | 查询通道属性 |
| `ipc_channel_set_timeout(channel, timeout_ms)` | 设置通道超时 |
| `ipc_channel_set_event_callback(channel, cb, user_data)` | 设置事件回调 |
| `ipc_channel_get_stats(channel, stats)` / `ipc_channel_reset_stats(channel)` | 统计信息查询 / 重置 |
| `ipc_send(channel, message)` / `ipc_send_data(channel, data, len, sent)` | 发送消息 / 简化发送 |
| `ipc_send_request(channel, request, response, timeout_ms)` | 发送请求并等待响应 |
| `ipc_broadcast(channel, message)` / `ipc_notify(channel, notification, len)` | 广播 / 通知 |
| `ipc_receive(channel, message, timeout_ms)` / `ipc_receive_data(...)` / `ipc_try_receive(...)` | 接收消息 |
| `ipc_set_message_callback(channel, cb, user_data)` | 设置消息回调 |

### 服务端与客户端 API（ipc_server_api.h / ipc_client_api.h）

| 函数 | 说明 |
|------|------|
| `ipc_server_create(config)` / `ipc_server_destroy(server)` | 创建 / 销毁服务端 |
| `ipc_server_start(server)` / `ipc_server_stop(server)` | 启动 / 停止服务端 |
| `ipc_server_accept(server, timeout_ms)` | 接受客户端连接 |
| `ipc_server_disconnect(server, channel)` | 断开指定连接 |
| `ipc_server_connection_count(server)` | 获取连接数 |
| `ipc_server_broadcast(server, message)` | 广播给所有客户端 |
| `ipc_client_create(config)` / `ipc_client_destroy(client)` | 创建 / 销毁客户端 |
| `ipc_client_connect(client, timeout_ms)` / `ipc_client_disconnect(client)` | 连接 / 断开 |
| `ipc_client_get_channel(client)` | 获取客户端通道 |

### 共享内存与消息队列 API（ipc_shm_api.h / ipc_mq_api.h）

| 函数 | 说明 |
|------|------|
| `ipc_shm_create(config)` / `ipc_shm_destroy(shm)` | 创建 / 销毁共享内存 |
| `ipc_shm_map(shm)` / `ipc_shm_unmap(shm)` | 映射 / 取消映射 |
| `ipc_shm_get_size(shm)` / `ipc_shm_sync(shm)` | 大小查询 / 同步 |
| `ipc_mq_create(config)` / `ipc_mq_destroy(mq)` | 创建 / 销毁消息队列 |
| `ipc_mq_send(mq, data, len, priority)` | 发送（带优先级） |
| `ipc_mq_receive(mq, buffer, len, received, priority, timeout_ms)` | 接收 |
| `ipc_mq_count(mq)` / `ipc_mq_clear(mq)` | 队列深度 / 清空 |

### RPC 框架（ipc_rpc_api.h）

| 函数 | 说明 |
|------|------|
| `ipc_rpc_server_create(config)` / `ipc_rpc_server_destroy(server)` | 创建 / 销毁 RPC 服务端 |
| `ipc_rpc_server_start(server)` / `ipc_rpc_server_stop(server)` | 启动 / 停止 |
| `ipc_rpc_server_process(server, timeout_ms)` | 处理单个 RPC 请求 |
| `ipc_rpc_server_register_method(server, method)` | 注册 RPC 方法 |
| `ipc_rpc_server_find_method(server, method_name)` | 查找已注册方法 |
| `ipc_rpc_client_create(config)` / `ipc_rpc_client_destroy(client)` | 创建 / 销毁 RPC 客户端 |
| `ipc_rpc_call_sync(client, method_name, request, ...)` | 同步 RPC 调用 |

### 消息辅助与工具函数（ipc_message_api.h / ipc_util_api.h）

| 函数 | 说明 |
|------|------|
| `ipc_message_create(type, payload, payload_len)` / `ipc_message_free(message)` | 创建 / 释放消息 |
| `ipc_message_clone(message)` | 复制消息 |
| `ipc_message_checksum(message)` / `ipc_message_verify(message)` | 计算 / 验证校验和 |
| `ipc_message_serialize(...)` / `ipc_message_deserialize(...)` | 序列化 / 反序列化 |
| `ipc_get_error_message(channel)` / `ipc_is_valid(channel)` / `ipc_flush(channel)` | 工具函数 |

## 使用示例

```c
#include "ipc_common.h"

/* 初始化 IPC 子系统 */
ipc_init();

/* === 服务端 === */
ipc_config_t server_config = ipc_create_default_config(IPC_TYPE_SOCKET);
server_config.name = "/tmp/agentrt.sock";
server_config.mode = IPC_MODE_READ_WRITE;

ipc_server_t *server = ipc_server_create(&server_config);
ipc_server_start(server);

ipc_channel_t *client_chan = ipc_server_accept(server, 5000);
if (client_chan != NULL) {
    ipc_message_t msg;
    airy_err_t err = ipc_receive(client_chan, &msg, 5000);
    if (err == AIRY_EOK) {
        /* msg.payload 为收到的有效载荷；原样回发 */
        ipc_send(client_chan, &msg);  /* echo back */
    }
    ipc_channel_destroy(client_chan);
}
ipc_server_stop(server);
ipc_server_destroy(server);

/* === 客户端 === */
ipc_config_t client_config = ipc_create_default_config(IPC_TYPE_SOCKET);
client_config.name = "/tmp/agentrt.sock";
client_config.mode = IPC_MODE_READ_WRITE;

ipc_client_t *client = ipc_client_create(&client_config);
ipc_client_connect(client, 5000);

ipc_channel_t *chan = ipc_client_get_channel(client);
const char *data = "Hello from client";
ipc_send_data(chan, data, strlen(data), NULL);

ipc_client_disconnect(client);
ipc_client_destroy(client);

/* 清理 IPC 子系统 */
ipc_cleanup();
```

## 平台可用性

| 组件 | Linux | macOS | Windows |
|------|-------|-------|---------|
| 通道 / 服务端 / 客户端传输层 | ✓ | ✓ | ✗ |
| 共享内存 (SHM) | ✓ | ✓ | ✗ |
| 消息队列 (MQ) | ✓ | ✓ | ✗ |
| RPC 框架 | ✓ | ✓ | ✗ |
| JSON-RPC 辅助 / 方法分发器 / 参数校验 | ✓ | ✓ | ✓ |
| 熔断器 (circuit_breaker) | ✓ | ✓ | ✓ |

传输层实现在构建中以 `NOT WIN32` 条件编译；测试 `test_ipc` 同样仅在 POSIX 平台构建。

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `airymax/ipc.h`（commons/include/） | 共享契约头 `struct airy_ipc_msg_hdr` 与 `AIRY_IPC_MAGIC` |
| `error.h` / `types.h` | 错误码（`airy_err_t`）与基础类型（`airy_timestamp_t` 等） |
| `platform.h` | 互斥锁等平台原语（circuit_breaker 依赖） |

---

© 2025-2026 SPHARX Ltd.

SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0（双许可，任选其一遵守，完整文本见仓库根 [LICENSE](../../LICENSE)）
