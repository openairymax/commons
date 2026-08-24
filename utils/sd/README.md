# sd — 跨进程服务发现

`sd` 提供**跨进程服务发现机制**（commons 权威版本），位于 `commons/utils/sd`。
P0.17 phase 4 从 `daemons/common/include/` 迁入 commons，消除
atoms → daemons 的编译期反向依赖（IRON-6）；daemons 侧保留再导出兼容头。

基于共享内存的跨进程服务注册表，支持：

- 跨进程服务注册与发现（无外部注册中心，零依赖）
- 服务健康状态传播
- 负载均衡选择（round-robin / 加权 / 最少连接）
- 服务依赖跟踪
- 心跳与自动过期（自愈）

## 设计原则

1. **零依赖**：不依赖 etcd / consul 等外部注册中心。
2. **高性能**：共享内存实现，发现 < 100ms。
3. **自愈**：过期服务自动清理，与健康检查联动。
4. **跨平台**：Windows / Linux / macOS 共享内存抽象。

## 关键常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `SD_MAX_SERVICES` | 128 | 最大服务数 |
| `SD_MAX_INSTANCES` | 8 | 每服务最大实例数 |
| `SD_DEFAULT_HEARTBEAT_MS` | 10000 | 默认心跳间隔 |
| `SD_DEFAULT_EXPIRE_MS` | 30000 | 默认过期时间 |
| `SD_SHM_NAME` | `/airy_svc_registry` | 共享内存区名称 |

## 相关模块

- `svc_common.h` — 服务管理框架（状态机 `airy_svc_state_t`，位于 utils/ipc/include/）
- `ipc_service_bus.h` — IPC 服务总线
- `daemon_bootstrap_sd.h` — daemon 引导期服务发现接入

## 状态

- **实现**：当前仅提供接口头（service_discovery.h / service_discovery_helper.h /
  daemon_bootstrap_sd.h），尚无 `src/` 实现，未纳入 airy_common 构建。
- **测试**：commons 单元测试覆盖注册发现、过期清理、负载均衡选择。
