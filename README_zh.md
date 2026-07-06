**语言:** [English](README.md) | 简体中文

# Airymax Commons — 统一基础库

`agentrt/commons/`

**版本：** 0.1.1
**许可证：** AGPL-3.0-or-later OR Apache-2.0（双许可证）
**分支：** `feature/official-hubs-01`

---

## 1. 模块定位

Commons 是 Airymax 智能体运行时的**基础层**。它为所有上层模块——原子原语、
守护进程、安全穹顶、网关、协议——提供跨平台、跨语言、跨模块的通用基础
设施。Commons 本身不依赖任何其它 Airymax 模块：它是依赖图的最底层。

作为全项目的类型定义权威源（`agentrt_types.h`），Commons 保证了模块间
类型一致性，消除了跨模块类型冲突。其设计目标：

- **零依赖抽象** —— 平台无关的类型系统与接口定义，确保内核与外围代码分离。
- **统一错误契约** —— 全系统共享的错误码体系（`agentrt_error_t`）与传播机制。
- **高性能基础设施** —— 内存池、无锁队列、零拷贝数据管道等底层原语。
- **可观测性内建** —— 日志、指标、链路追踪的标准化采集接口。
- **安全默认** —— 所有 I/O 路径默认经过参数校验、边界检查和资源限制。

---

## 2. 目录结构

```
commons/
├── CMakeLists.txt               # CMake 构建配置
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
│   └── agentrt_types.h          # 统一类型与错误码定义
├── utils/                       # 工具模块集合（24+ 个）
│   ├── include/                 # 跨模块共享头文件
│   │   ├── atomic_compat.h      # 跨平台原子操作兼容层
│   │   └── check.h              # 通用检查宏
│   ├── logging/                 # 日志系统（三层：Core → Atomic → Service）
│   ├── sync/                    # 同步原语（8+ 种锁与队列）
│   ├── memory/                  # 内存管理（内存池、智能指针、零拷贝）
│   ├── string/                  # 字符串操作与格式化
│   ├── ipc/                     # IPC 抽象层
│   ├── token/                   # 令牌管理（API Key/JWT 生命周期）
│   ├── cost/                    # 成本估算与控制
│   ├── observability/           # 可观测性（OpenTelemetry 指标/追踪）
│   ├── platform/                # 平台相关工具函数
│   ├── error/                   # 错误处理框架
│   ├── types/                   # 通用类型定义与转换
│   ├── config_unified/          # 统一配置（三层：Core → Source → Service）
│   ├── execution/               # 命令执行引擎（安全校验、跨平台）
│   ├── io/                      # 文件 I/O 工具
│   ├── cache/                   # 缓存管理（LRU/TTL）
│   ├── compat/                  # 跨版本/跨平台兼容性适配
│   ├── cognition/               # 认知管理（Agent 信息、任务调度、计划生成）
│   ├── strategy/                # 加权评分策略引擎
│   ├── network/                 # 网络工具（HTTP/URI/DNS）
│   ├── security/                # 输入校验与安全过滤
│   ├── resource/                # 资源保护与配额
│   ├── uuid/                    # UUID 生成与解析
│   ├── print/                   # 打印/格式化辅助
│   ├── compliance/              # 合规性校验与策略执行
│   └── quality/                 # 代码质量检查
└── tests/                       # 测试套件
    ├── utils/                   # 测试工具框架
    ├── unit/                    # 单元测试
    └── integration/             # 集成测试
```

### 核心组件

#### 类型系统（`agentrt_types.h`）

全项目唯一的类型定义权威源：

| 类型 | 说明 |
|------|------|
| `agentrt_error_t` | 统一错误码类型（`int32_t`，负值为错误，0 为成功） |
| `agentrt_ipc_header_t` | 应用级 IPC 消息头（magic/version/type/flags/msg_id 等） |
| `agentrt_ipc_message_t` | 应用级 IPC 消息结构（header + payload） |
| `agentrt_task_id_t` | 任务 ID 类型（`uint64_t`） |
| `agentrt_message_id_t` | 消息 ID 类型（`uint64_t`） |

统一错误码体系（`AGENTRT_E*`）覆盖参数无效、内存不足、权限不足、超时、
I/O 错误、协议错误、配额超限等 29 种标准错误。

#### 平台抽象层（`platform/`）

- **平台检测** —— 自动识别 Linux / Windows / macOS。
- **文件系统** —— 路径规范化、文件操作抽象。
- **线程与同步** —— `agentrt_thread_t`、`agentrt_mutex_t`、`agentrt_cond_t` 等。
- **动态库加载** —— 跨平台 FFI 支持。
- **系统信息** —— CPU 核心数、内存大小、进程 ID。

#### 原子操作兼容层（`atomic_compat.h`）

| 后端 | 适用环境 | 实现方式 |
|------|----------|----------|
| C11 stdatomic | Linux / macOS（C11+ 编译器） | `<stdatomic.h>` |
| Windows Interlocked | Windows（MSVC / MinGW） | `<intrin.h>` Interlocked API |
| POSIX fallback | 旧版 GCC/Clang 环境 | `__atomic` builtins |

覆盖 `atomic_bool`、`atomic_int`、`atomic_uint`、`atomic_int64_t`、
`atomic_uint64_t`、`atomic_size_t` 等 11 种类型。

#### 统一内存管理（`memory_compat.h`）

| 宏 | 替代 | 说明 |
|------|------|------|
| `AGENTRT_MALLOC(size)` | `malloc(size)` | 统一内存分配 |
| `AGENTRT_CALLOC(num, size)` | `calloc(num, size)` | 统一零初始化分配 |
| `AGENTRT_FREE(ptr)` | `free(ptr)` | 统一内存释放 |

---

## 3. 上游 / 下游依赖关系

### 上游（Commons 依赖）

| 依赖 | 必需 | 用途 |
|------|------|------|
| C11 编译器 | 是 | 支持 `<stdatomic.h>` |
| pthreads / Win32 threads | 是 | 线程与同步原语 |
| libyaml | 否 | 完整 YAML 支持 |
| cJSON | 否 | JSON 配置解析 |

> **BAN-12**：外部依赖由伞仓根 `CMakeLists.txt` 集中检测，子模块不得
> 独立调用 `find_package`。检测结果通过 `AGENTRT_HAS_CJSON`、
> `AGENTRT_HAS_YAML` 等 CMake 缓存变量传递。

Commons **没有**任何上游 Airymax 模块依赖——它是运行时的最底层。

### 下游（消费 Commons）

| 消费者 | 用途 |
|--------|------|
| **atoms** | 平台抽象、内存管理、错误框架、类型系统——每个 atoms 模块都引入约 18 个 commons 子模块 |
| **cupolas** | 类型系统、同步原语、内存管理宏 |
| **heapstore** | 日志、config_unified、内存池、同步原语 |
| **protocols** | 类型系统（`agentrt_ipc_header_t`）、sync、observability |
| **gateway** | 网络工具、令牌管理、日志、config_unified |
| **daemons** | 日志、config_unified、网络、令牌、成本、可观测性、认知、策略——全量使用 |

---

## 4. 构建说明

Commons 构建为单一静态库 `agentrt_common`，聚合所有工具模块。include
路径以 PUBLIC 形式导出，消费者只需链接一个目标即可看到所有子模块头文件。

```bash
# 标准构建（在伞仓根目录或本仓独立构建）
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# 运行测试套件
ctest --test-dir build
```

### CMake 选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `BUILD_TESTS` | `ON` | 构建单元测试和集成测试 |
| `AGENTRT_HAS_CJSON` | 自动 | 由伞仓 CMake 自动检测，决定 cJSON 相关代码路径 |
| `AGENTRT_HAS_YAML` | 自动 | 由伞仓 CMake 自动检测，决定 libyaml 相关代码路径 |

当 cJSON/YAML 不可用时，Commons 优雅降级：定义 `AGENTRT_NO_CJSON`，
JSON/YAML 解析器回退到内置最小实现（如 `yaml_minimal.c`）。

### 构建产物

- `agentrt_common` —— 包含所有工具模块的静态库
- 公共头文件安装到 `include/agentrt/{platform,utils/*}`

### 安装

```bash
cmake --install build --prefix /opt/airymax
```

---

## 5. 使用示例

```c
#include "agentrt_types.h"
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

LOG_INFO("系统初始化完成, host: %s",
         CONFIG_GET_STRING_SAFE(ctx, "server.host", "localhost"));

log_cleanup();
config_context_destroy(ctx);
```

---

## 6. 许可证

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.

本模块采用双许可证，您可以选择以下任一许可证遵守：

- **GNU Affero General Public License v3.0 or later**
  ([AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.txt))，或
- **Apache License, Version 2.0**
  ([Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0.txt))

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`

完整许可证文本见 [LICENSE](LICENSE) 文件，版权声明见 [NOTICE](NOTICE)。
默认适用 AGPL-3.0-or-later 条款；Apache-2.0 备选用于 AGPL 无法覆盖的
下游集成场景（如闭源或专有分发）。
