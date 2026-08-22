# commons — 统一基础库

> Airymax 运行时的最底层：所有其他 agentrt 模块都构建在 commons 之上。
> [agentrt](../) 管理仓下的叶子仓。

**语言:** [English](README.md) | 简体中文

[![Version](https://img.shields.io/badge/version-0.1.1-5a6b7e)](https://atomgit.com/openairymax/commons)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

- **仓库地址：** `git@atomgit.com:openairymax/commons.git`
- **分支：** `feature/official-hubs-01`
- **版本：** 0.1.1（Airymax 奠基版本）

---

## 概述

**commons** 是 Airymax 智能体运行时的**基础层**。它提供跨平台、跨语言、跨模块的基础设施，所有上层——原子原语（`atoms`）、守护进程、安全穹顶（`cupolas`）、网关、协议、存储——都构建其上。commons 自身**不依赖**任何其他 Airymax 模块：它是依赖图的最底层，也是 SDK 层最终回绑、闭合 Airymax 循环架构的规范基底。

作为项目类型定义（`airy_types.h`）和统一错误码契约（`airy_err_t`）的权威来源，commons 保证跨模块类型一致性，消除跨模块类型冲突。其设计目标是：零依赖抽象（平台无关的类型系统和接口定义使内核与外围代码解耦）、统一错误契约、高性能基础设施（内存池、无锁队列、零拷贝流水线）、内置可观测性（标准化的日志/指标/追踪捕获接口）、默认安全的 I/O 路径（参数校验、边界检查、资源限制）。

commons 构建单一静态库 `airy_common`，聚合 32 个工具模块；include 路径以 PUBLIC 导出，消费者通过单次 target 链接即可访问所有子模块头文件。在 Airymax 0.1.1 发行版中，工作区被拆分为 **38 个仓库**（1 umbrella + 5 management + 29 leaf + 3 top-level）；`commons` 是 [agentrt](../) 管理仓聚合的 7 个叶子仓之一，是其他 6 个叶子仓共享的**唯一基础点**。

## 模块分类

**A 类 —— 基础 / 原子层。**

commons 是 agentrt 中绝对最底层的叶子仓，零 intra-agentrt 上游依赖（仅依赖 OS / 编译器 / 可选外部库），被其他所有叶子仓（atoms、cupolas、heapstore、protocols、gateway、daemons）消费。作为 A 类模块，commons 在同一主版本内保证 ABI 稳定，提供将整个运行时绑定在一起的类型系统 + 错误契约。

## 目录结构

```
commons/
├── CMakeLists.txt               # CMake 构建配置（单一静态库 airy_common）
├── README.md                    # 英文版
├── README_zh.md                 # 本文件（中文）
├── LICENSE                      # 双许可证文本（AGPL-3.0 + Apache-2.0）
├── NOTICE                       # 版权声明
├── platform/                    # 平台抽象层
│   ├── include/
│   │   ├── platform.h           # 平台检测与基础定义
│   │   └── export.h             # 符号导出控制
│   ├── compat/                  # 平台兼容头文件（stdbool.h、stdint.h）
│   └── platform.c               # 平台抽象实现
├── include/                     # 全局公共头文件
│   └── airy_types.h          # 统一类型与错误码定义（权威来源）
├── utils/                       # 工具模块集合（32 个模块）
│   ├── include/                 # 跨模块共享头文件
│   │   ├── atomic_compat.h      # 跨平台原子操作兼容层
│   │   └── check.h              # 通用检查宏
│   ├── logging/                 # 日志（三级：Core → Atomic → Service）
│   ├── sync/                    # 同步原语（8+ 锁与队列）
│   ├── memory/                  # 内存管理（池、智能指针、零拷贝、arena、tcache）
│   ├── string/                  # 字符串操作与安全格式化
│   ├── ipc/                     # IPC 抽象层
│   ├── token/                   # 令牌管理（API Key / JWT 生命周期、counter、budget）
│   ├── cost/                    # 成本估算与控制
│   ├── observability/           # 可观测性（OpenTelemetry 指标/追踪、logger）
│   ├── platform/                # 平台特定工具函数（platform_adapter）
│   ├── error/                   # 错误处理框架
│   ├── types/                   # 通用类型定义与转换
│   ├── config_unified/          # 统一配置（三级：Core → Source → Service；yaml_minimal）
│   ├── execution/               # 任务检查点（checkpoint：持久化/会话/快照）
│   ├── io/                      # 文件 I/O 工具
│   ├── cache/                   # 缓存管理（LRU / TTL）
│   ├── compat/                  # 跨版本 / 跨平台兼容
│   ├── cognition/               # 认知管理（智能体信息、调度、规划）
│   ├── strategy/                # 加权评分策略引擎
│   ├── network/                 # 网络工具（HTTP / URI / DNS）
│   ├── security/                # 输入校验与安全过滤
│   ├── resource/                # 资源保护与配额（guard、quota）
│   ├── uuid/                    # UUID 生成与解析
│   ├── print/                   # 打印 / 格式化助手（统一打印宏）
│   ├── compliance/              # 合规校验与策略执行
│   ├── quality/                 # 代码质量检查
│   ├── sd/                      # 安全删除 / 服务发现（共享内存跨进程注册表）
│   ├── effect/                  # 回卷机制（airy_effect：注册即副作用、逆序回滚）
│   ├── ext/                     # 统一扩展注册表（LLM/tool/storage/sandbox/memory 五域）
│   ├── id/                      # 品牌化 ID 生成（trace_id / msg_id）
│   ├── task/                    # A-TD 任务描述符（创建 + CRC32 完整性校验）
│   ├── cjson/                   # cJSON 三步宏辅助层（CJSON_PARSE_GUARD 等）
│   └── ime/                     # 轻量内置输入法（全拼词典，纯 C）
└── tests/                       # 测试套件
    ├── utils/                   # 测试工具框架
    ├── unit/                    # 单元测试
    └── integration/             # 集成测试
```

## 核心组件

### 类型系统（`airy_types.h`）

整个项目唯一的权威类型定义来源：

| 类型 | 说明 |
|------|------|
| `airy_err_t` | 统一错误码类型（`int32_t`；负数 = 错误，0 = 成功） |
| `airy_ipc_hdr_t` | 应用层 IPC 消息头（magic/version/type/flags/msg_id） |
| `airy_ipc_msg_t` | 应用层 IPC 消息结构（header + payload） |
| `airy_task_id_t` | 任务 ID 类型（`uint64_t`） |
| `airy_message_id_t` | 消息 ID 类型（`uint64_t`） |

统一错误码系统（`AIRY_E*`）覆盖 29 个标准错误，包括无效参数、内存不足、权限拒绝、超时、I/O 错误、协议错误、配额超限等。

### 工具模块（32 个）

| 模块 | 路径 | 职责 |
|------|------|------|
| logging | `utils/logging/` | 三级日志（Core → Atomic → Service）；JSON/文本格式；原子日志 |
| sync | `utils/sync/` | 8+ 同步原语：自旋锁、互斥锁、读写锁、信号量、条件变量、事件、屏障、递归互斥锁 |
| memory | `utils/memory/` | 内存池、智能指针、零拷贝、arena、tcache、预分配、调试、统计上报 |
| string | `utils/string/` | 字符串操作与安全字符串工具（严格合规下替换不安全的 `strcpy` 家族） |
| ipc | `utils/ipc/` | IPC 抽象层（ipc_common；仅 POSIX） |
| token | `utils/token/` | 令牌管理：API Key / JWT 生命周期、counter、budget |
| cost | `utils/cost/` | 成本估算与控制器 |
| observability | `utils/observability/` | OpenTelemetry 对齐的指标/追踪；logger |
| platform | `utils/platform/` | 平台特定工具（platform_adapter） |
| error | `utils/error/` | 错误处理框架（handler） |
| types | `utils/types/` | 通用类型定义与转换 |
| config_unified | `utils/config_unified/` | 三级配置（Core → Source → Service）；yaml_minimal 回退 |
| execution | `utils/execution/` | 任务检查点（checkpoint：持久化/会话/快照/统计） |
| io | `utils/io/` | 文件 I/O 工具（file_utils） |
| cache | `utils/cache/` | 缓存管理（LRU / TTL）；cache_common |
| compat | `utils/compat/` | 跨版本 / 跨平台兼容（compat、compat2） |
| cognition | `utils/cognition/` | 认知管理（智能体信息、调度、规划） |
| strategy | `utils/strategy/` | 加权评分策略引擎 |
| network | `utils/network/` | 网络工具（HTTP / URI / DNS） |
| security | `utils/security/` | 输入校验与安全过滤（input_validator） |
| resource | `utils/resource/` | 资源保护与配额（resource_guard、resource_quota） |
| uuid | `utils/uuid/` | UUID 生成与解析（uuid_generator） |
| print | `utils/print/` | 运行时统一打印宏（airy_print_*，委托 log_write） |
| compliance | `utils/compliance/` | 合规校验与策略执行 |
| quality | `utils/quality/` | 代码质量检查 |
| sd | `utils/sd/` | 跨进程服务发现（共享内存注册表、心跳/过期/负载均衡） |
| effect | `utils/effect/` | 回卷机制（airy_effect：注册即副作用、逆序回滚） |
| ext | `utils/ext/` | 统一扩展注册表（LLM/tool/storage/sandbox/memory 五域） |
| id | `utils/id/` | 品牌化 ID 生成（trace_id / msg_id） |
| task | `utils/task/` | A-TD 任务描述符（创建 + CRC32 完整性校验） |
| cjson | `utils/cjson/` | cJSON 三步宏辅助层（CJSON_PARSE_GUARD 等） |
| ime | `utils/ime/` | 轻量内置输入法（全拼词典，纯 C，随安装分发） |

### 平台抽象层（`platform/`）

- **平台检测** —— 自动检测 Linux / Windows / macOS。
- **文件系统** —— 路径规范化与文件操作抽象。
- **线程与同步** —— `airy_thread_t`、`airy_mtx_t`、`airy_cond_t`。
- **动态库加载** —— 跨平台 FFI 支持。
- **系统信息** —— CPU 核数、内存大小、进程 ID。

### 原子操作兼容层（`atomic_compat.h`）

| 后端 | 环境 | 实现 |
|------|------|------|
| C11 stdatomic | Linux / macOS（C11+ 编译器） | `<stdatomic.h>` |
| Windows Interlocked | Windows（MSVC / MinGW） | `<intrin.h>` Interlocked API |
| POSIX 回退 | 旧版 GCC/Clang | `__atomic` 内建函数 |

覆盖 11 种类型，包括 `atomic_bool`、`atomic_int`、`atomic_uint`、`atomic_int64_t`、`atomic_uint64_t`、`atomic_size_t`。

### 统一内存管理（`airy_memory.h`）

| 宏 | 替换 | 说明 |
|------|------|------|
| `AIRY_MALLOC(size)` | `malloc(size)` | 统一分配 |
| `AIRY_CALLOC(num, size)` | `calloc(num, size)` | 统一零初始化分配 |
| `AIRY_FREE(ptr)` | `free(ptr)` | 统一释放 |

## 架构

```
┌──────────────────────────────────────────────┐
│             Applications (OpenLab)            │
├──────────────────────────────────────────────┤
│             Ecosystem (Toolkit / SDK)         │
├──────────────────────────────────────────────┤
│              Daemon Services                  │
├──────────────────────────────────────────────┤
│   protocols / gateway / heapstore / cupolas   │
├──────────────────────────────────────────────┤
│                  atoms                        │
├──────────────────────────────────────────────┤
│          ★ commons (支撑层) ★                │  ← 最底层，零 intra-agentrt 依赖
├──────────────────────────────────────────────┤
│            Operating System / Hardware        │
└──────────────────────────────────────────────┘

  airy_common（单一静态库）
  ┌────────────────────────────────────────────┐
  │  airy_types.h  （权威类型）              │
  │  platform/        （OS 抽象）               │
  │  utils/           （32 个工具模块）          │
  │   logging sync memory string ipc token     │
  │   cost observability platform error types  │
  │   config_unified execution io cache compat │
  │   cognition strategy network security      │
  │   resource uuid print compliance quality sd│
  │   effect ext id task cjson ime             │
  └────────────────────────────────────────────┘
        ▲     ▲     ▲     ▲     ▲     ▲
        │     │     │     │     │     │
      atoms cupolas heapstore protocols gateway daemons
```

## 上游依赖

> commons 是运行时的最底层——它**没有**任何上游 Airymax 模块依赖。

| 依赖 | 是否必需 | 用途 |
|------|----------|------|
| C11 编译器 | 是 | `<stdatomic.h>` 支持、原子兼容层 |
| pthreads / Win32 threads | 是 | 线程与同步原语 |
| libyaml | 否 | 完整 YAML 支持（回退到 `yaml_minimal.c`） |
| cJSON | 否 | JSON 配置解析（由 `AIRY_HAS_CJSON` 门控） |

> **BAN-12**：外部依赖由伞仓 `CMakeLists.txt` 集中探测；子模块**禁止**独立调用 `find_package`。探测结果通过 CMake 缓存变量（如 `AIRY_HAS_CJSON`、`AIRY_HAS_YAML`）传播。

当 cJSON/YAML 不可用时，commons 优雅降级：定义 `AIRY_NO_CJSON`，JSON/YAML 解析器回退到最小内置实现。

## 下游消费者

> commons 是**所有** agentrt 模块的基础库——其他所有叶子仓都消费它。

| 消费者 | 用途 |
|--------|------|
| **atoms** | 平台抽象、内存管理、错误框架、类型系统——每个 atoms 子模块拉取约 18 个 commons 工具模块（logging、sync、platform、error、types、config_unified、observability、ipc、io、cache、cost、memory、string、token、network、security、resource、uuid） |
| **cupolas** | 类型系统（`airy_types.h`）、同步原语、内存宏、security/resource 工具用于安全穹顶 |
| **heapstore** | logging、config_unified、内存池、同步原语用于堆式持久化 |
| **protocols** | 类型系统（`airy_ipc_hdr_t` / `airy_ipc_msg_t`）、sync、observability、ipc 用于 AgentsIPC 线协议 |
| **gateway** | network 工具、token 管理、logging、config_unified 用于网关守护进程 |
| **daemons** | logging、config_unified、network、token、cost、observability、cognition、strategy——全部 12 个守护进程消费完整接口面 |
| SDK 层 | Python/Go/Rust/TypeScript SDK 最终回绑 commons 类型与错误契约，闭合 Airymax 循环架构 |

## 构建

commons 构建单一静态库 `airy_common`，聚合所有工具模块。include 路径以 PUBLIC 导出，消费者通过单次 target 链接即可访问所有子模块头文件。

```bash
# 标准构建（源外构建，BAN-33 强制要求）
cmake -S . -B /tmp/commons-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/commons-build --parallel $(nproc)

# 启用单元 + 集成测试
cmake -S . -B /tmp/commons-build -DBUILD_TESTS=ON
ctest --test-dir /tmp/commons-build --output-on-failure

# 安装
cmake --install /tmp/commons-build --prefix /opt/airymax
```

**CMake 选项：**

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | `ON` | 构建单元与集成测试 |
| `AIRY_HAS_CJSON` | 自动 | 由伞仓 CMake 自动探测；门控依赖 cJSON 的代码路径 |
| `AIRY_HAS_YAML` | 自动 | 由伞仓 CMake 自动探测；门控依赖 libyaml 的代码路径 |

**构建产物：**

- `airy_common` —— 包含所有工具模块的静态库
- 公共头文件安装到 `include/agentrt/{platform,utils/*}`

## API

commons 通过统一类型头和各模块公共头暴露接口。权威入口：

- `include/airy_types.h` —— 统一类型与错误码定义（`airy_err_t`、`airy_ipc_hdr_t`、`airy_ipc_msg_t`、`AIRY_E*` 错误码）
- `platform/include/platform.h` —— 平台检测与基础定义
- `utils/include/atomic_compat.h` —— 跨平台原子操作（11 种类型，3 种后端）
- `utils/include/check.h` —— 通用检查宏
- 各模块入口头：`utils/logging/include/logging.h`、`utils/sync/include/sync.h`、`utils/memory/include/memory.h`、`utils/string/include/string.h`、`utils/config_unified/include/config_unified.h`、`utils/observability/include/observability.h`、`utils/token/include/token.h`、`utils/cost/include/cost.h`、`utils/error/include/error.h`、`utils/network/include/network.h`、`utils/security/include/security.h`、`utils/resource/include/resource.h`、`utils/uuid/include/uuid.h`、`utils/cache/include/cache.h`、`utils/io/include/io.h`、`utils/ipc/include/ipc.h`、`utils/execution/include/checkpoint.h`、`utils/cognition/include/cognition.h`、`utils/strategy/include/strategy.h`、`utils/types/include/types.h`、`utils/platform/include/platform_adapter.h`、`utils/compat/include/compat.h`、`utils/print/include/airy_print.h`、`utils/compliance/include/compliance.h`、`utils/quality/include/quality.h`、`utils/sd/include/service_discovery.h`、`utils/effect/include/airy_effect.h`、`utils/ext/include/airy_ext.h`、`utils/cjson/include/cjson_helpers.h`、`utils/ime/include/airy_ime.h`

内存宏（`AIRY_MALLOC` / `AIRY_CALLOC` / `AIRY_FREE`）和严格合规的不安全函数投毒（如通过 `utils/string` 替换 `strcpy`）是项目级的。

### 使用示例

```c
#include "airy_types.h"
#include "logging.h"
#include "config_unified.h"

log_config_t log_cfg = {
    .level = LOG_LEVEL_INFO,
    .output = LOG_OUTPUT_CONSOLE,
    .format = LOG_FORMAT_JSON
};
log_init(&log_cfg);

config_context_t *ctx = config_context_create("myapp");
config_value_t *val = CONFIG_STRING("0.0.0.0");
config_context_set(ctx, "server.host", val);

LOG_INFO("system initialized, host: %s",
         CONFIG_GET_STRING_SAFE(ctx, "server.host", "localhost"));

log_cleanup();
config_context_destroy(ctx);
```

## 许可证

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.

本模块采用双许可证，您可以选择以下任一许可证遵守：

- **GNU Affero General Public License v3.0 or later**
  ([AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.txt))，或
- **Apache License, Version 2.0**
  ([Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0.txt))

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`

完整许可证文本见 [LICENSE](LICENSE) 文件；版权声明见 [NOTICE](NOTICE)。默认适用 AGPL-3.0-or-later 条款；Apache-2.0 备选用于 AGPL 无法覆盖的下游集成场景（如闭源或专有分发）。
