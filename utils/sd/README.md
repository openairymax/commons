# sd — 跨进程服务发现契约头

**模块路径**: `commons/utils/sd/` · **版本**: 0.1.15

基于共享内存的跨进程服务注册表接口定义：服务注册/发现、健康状态传播、负载均衡选择、依赖检查、心跳与自动过期。本目录只提供 header-only 的接口声明，实现位于 agentrt 守护进程公共库，供各 daemon 链接使用。

## 概述

- **零外部注册中心**：不依赖 etcd / consul，注册表放在命名共享内存段（默认 `/airy_svc_registry`）中，跨进程直接读写。
- **三层接口**：核心 API（`sd_*`）→ 便捷层（`sd_helper_*`，封装注册信息与心跳线程）→ 一次性引导（`daemon_bootstrap_sd_*`，daemon 启动即注册、退出即注销）。
- **负载均衡选择**：内置 round-robin、加权、最少连接、随机、最少负载五种策略。
- **自愈**：心跳超时自动过期清理，健康状态变化可经事件回调传播。

## 目录结构

```
utils/sd/
├── service_discovery.h          # 核心注册表接口（23 个 sd_* API）
├── service_discovery_helper.h   # 便捷层（14 个 sd_helper_* API）
├── daemon_bootstrap_sd.h        # daemon 一次性引导（5 个 API）
└── README.md
```

## 数据结构与常量

| 类型 | 说明 |
|---|---|
| `service_discovery_t` | 不透明句柄（`sd_create` 返回） |
| `sd_instance_t` | 服务实例：instance_id、endpoint、状态、healthy、权重、活跃/最大连接、最后心跳、注册时间的 pid |
| `sd_service_entry_t` | 服务条目：name、version、type、tags、dependencies、capabilities、至多 8 个实例 |
| `sd_config_t` | 心跳间隔、过期时限、默认 LB 策略、自动过期/健康传播开关、共享内存名与大小 |
| `sd_stats_t` | 注册/注销/发现/心跳/过期/LB 选择计数与活跃服务、实例数 |
| `sd_event_type_t` | 事件：注册、注销、健康变化、过期、实例上线/下线 |
| `sd_lb_strategy_t` | `SD_LB_ROUND_ROBIN` / `WEIGHTED` / `LEAST_CONNECTION` / `RANDOM` / `LEAST_LOAD` |

| 常量 | 值 | 含义 |
|---|---|---|
| `SD_MAX_SERVICES` | 128 | 注册表服务上限 |
| `SD_MAX_INSTANCES` | 8 | 每服务实例上限 |
| `SD_DEFAULT_HEARTBEAT_MS` | 10000 | 默认心跳间隔 |
| `SD_DEFAULT_EXPIRE_MS` | 30000 | 默认过期时限 |
| `SD_SHM_NAME` | `/airy_svc_registry` | 默认共享内存段名 |

服务状态枚举 `airy_svc_state_t` 与错误码约定复用 commons `utils/ipc` 的服务管理框架头 `svc_common.h`。

## 核心 API（service_discovery.h）

| 组 | 函数 |
|---|---|
| 生命周期 | `sd_create(config)`（NULL 用默认） / `sd_destroy` / `sd_start` / `sd_stop` |
| 注册 | `sd_register(sd, name, type, instance, tags, dependencies)` / `sd_deregister(sd, name, instance_id)` / `sd_deregister_all` |
| 发现 | `sd_discover`（全部健康实例） / `sd_discover_by_type` / `sd_discover_by_tags` / `sd_select_instance(sd, name, strategy, out)` |
| 状态维护 | `sd_heartbeat` / `sd_update_health` / `sd_update_connections` |
| 依赖 | `sd_get_dependencies` / `sd_check_dependencies(sd, name, missing_out, max_len)`（全部满足返回 0） |
| 观测与工具 | `sd_register_event_callback` / `sd_get_stats` / `sd_service_count` / `sd_is_running` / `sd_lb_strategy_to_string` / `sd_create_default_config` / `sd_dump_stats`（单行统计摘要，适合周期日志） |

## 便捷层与引导（helper / bootstrap）

| 接口 | 说明 |
|---|---|
| `sd_helper_init(config)` / `sd_helper_shutdown` | 创建/销毁发现实例并托管注册信息与心跳线程 |
| `sd_helper_register(sdh, name, type, host, port, tags, ttl_ms)` | 自动生成 instance_id（host:port）并注册；`sd_helper_register_unix` 为 Unix socket 变体 |
| `sd_helper_start_heartbeat` / `sd_helper_stop_heartbeat` / `sd_helper_send_heartbeat` | 周期心跳线程控制与手动心跳 |
| `sd_helper_find` / `sd_helper_select` / `sd_helper_select_with_strategy` | 发现与 LB 选择的简化包装 |
| `sd_helper_get_sd` / `sd_helper_is_running` / `sd_helper_service_count` / `sd_helper_dump_stats` | 底层句柄与状态访问 |
| `daemon_bootstrap_sd_start(name, type, host, port, tags, ttl_ms)` | 一次性完成 init + 注册 + 心跳启动；`_start_unix` 为 socket 变体；`_stop` 自动注销并释放 |
| `daemon_bootstrap_sd_get_helper` / `daemon_bootstrap_sd_is_running` | 取回 helper 句柄与运行状态 |

`ttl_ms` 传 0 使用默认过期时限 30000ms。

## 语义与约束

- 本目录三个头文件均未注册进 `airy_common` 的构建（无源文件、无 PUBLIC include 条目、无安装规则）：消费方需自行把 `utils/sd/` 与 `utils/ipc/`（`svc_common.h`）加入头文件搜索路径。
- 接口实现与生命周期行为以守护进程公共库为准；仅链接本仓 `airy_common` 不会得到 `sd_*` 符号。
- 共享内存注册表为跨进程可变状态，`sd_*` 调用线程安全性由实现层决定；多实例进程应统一经心跳与过期机制收敛视图。
- Windows / Linux / macOS 共享内存抽象由实现层提供，头文件本身平台中立。

## 用法示例

```c
#include <stdlib.h>

#include "daemon_bootstrap_sd.h"
#include "service_discovery.h"

int worker_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* 引导：初始化 + 注册 + 心跳线程一体化 */
    daemon_bootstrap_sd_t *bsd = daemon_bootstrap_sd_start(
        "maths_d", "compute", "127.0.0.1", 8080, "core", 0);
    if (!bsd)
        return EXIT_FAILURE;

    /* 作为消费方：按 LB 策略选择远端服务实例 */
    sd_helper_t *sdh = daemon_bootstrap_sd_get_helper(bsd);
    sd_instance_t peer;
    if (sd_helper_select(sdh, "llm_d", &peer) == 0) {
        /* 使用 peer.endpoint 建立连接 */
    }

    /* ... 主循环（心跳自动维持） ... */

    daemon_bootstrap_sd_stop(bsd); /* 注销并释放 */
    return EXIT_SUCCESS;
}
```

## 构建与依赖

| 依赖 | 用途 |
|---|---|
| commons `utils/ipc`（`svc_common.h`） | `airy_svc_state_t` 服务状态机与 `AIRY_API` 装饰宏（其自身携带 `error.h`） |
| C 标准库 | `stdbool.h`、`stdint.h` |

本模块为纯接口头，无编译单元。commons 测试套件不含本模块测试；行为测试（注册发现、过期清理、LB 选择、健康传播）随守护进程公共库的测试套件维护。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
