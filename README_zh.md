# commons — 统一基础库

> Airymax 运行时的最底层：agentrt 的其他所有模块都构建在 commons 之上。

**语言：** [English](README.md) | 简体中文

[![Version](https://img.shields.io/badge/version-0.1.15-5a6b7e)](https://atomgit.com/openairymax/commons)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

---

## 这是什么

**commons** 是 Airymax 智能体运行时
（[agentrt](https://atomgit.com/openairymax/agentrt)）的基础层库。它为上层
所有部分提供跨平台、跨模块的基础设施：原子原语
（[atoms](https://atomgit.com/openairymax/atoms)）、安全防护罩
（[cupolas](https://atomgit.com/openairymax/cupolas)）、存储
（[heapstore](https://atomgit.com/openairymax/heapstore)）、线协议
（[protocols](https://atomgit.com/openairymax/protocols)）、网关
（[gateway](https://atomgit.com/openairymax/gateway)）以及守护进程服务
（[daemons](https://atomgit.com/openairymax/daemons)）。

commons **不依赖**任何其他 Airymax 模块：它位于依赖图的最底层，仅依赖
操作系统、C11 编译器和可选外部库（pthreads、libyaml、cJSON）。

它编译为**单一静态库**（target `airy_common`），聚合 32 个内聚工具模块
外加平台抽象层。include 路径以 `PUBLIC` 导出，消费者链接 `airy_common`
后即可通过单次链接看到全部子模块头文件。

## 能力

- **统一类型与错误契约** — `include/airy_types.h` 中的 `airy_err_t` 与
  品牌化 ID 类型（`airy_trace_id_t` / `airy_msg_id_t`）；跨边界错误码契约
  在 `include/airymax/error.h`；128 字节 IPC 线格头
  `struct airy_ipc_msg_hdr` 在 `include/airymax/ipc.h`。
  一套类型体系保证所有模块 ABI 一致。
- **平台抽象** — `platform/` 将 Linux / Windows / macOS 的差异（线程、
  同步、路径、进程、时间、文件系统、动态加载）隐藏在同一套 API 之后。
- **高性能基础设施** — 内存池、同步原语与队列、LRU/TTL 缓存、事件循环
  与定时器、原子操作兼容层。
- **智能体运行时工具** — token 计数与预算、成本估算、任务检查点、
  认知/策略辅助、服务发现、IPC/RPC、带内置极简 YAML 解析器的统一配置、
  拼音输入法词典。
- **可观测性** — 日志（控制台/文件/JSON）、指标与跟踪，提供
  Prometheus 风格的导出接口。
- **默认安全** — 输入校验、路径规范化、资源守护与配额、日志脱敏，
  以及可选的严格合规模式（对整个工程投毒不安全 libc 函数）。

## 目录构成

```
commons/
├── CMakeLists.txt               # 构建单一静态库 airy_common
├── README.md / README_zh.md     # 本文件（英文/中文）
├── LICENSE                      # 双许可文本（AGPL-3.0-or-later OR Apache-2.0）
├── NOTICE                       # 版权声明
├── platform/                    # 平台抽象层
│   ├── include/                 # 公共头（platform.h + 按领域拆分的头文件）
│   ├── compat/                  # 兼容性头文件（stdbool.h、stdint.h）
│   └── src/                     # 实现（base / paths / process / sync / time）
├── include/                     # 全局公共头
│   ├── airy_types.h             # 统一类型与错误契约入口
│   ├── airy_defaults.h          # 全工程默认值（路径、限额、调优参数）
│   ├── airy_run_stream.h        # run-stream 契约类型
│   ├── airy_tool_schema.h       # tool schema 契约类型
│   ├── airyrt_version.h         # 版本宏
│   ├── airymax/                 # 跨边界契约头
│   │   ├── error.h              # airy_err_t + AIRY_E* / AIRY_FAULT_* 错误码
│   │   ├── ipc.h                # airy_ipc_msg_hdr（128 字节线格头）
│   │   ├── task_desc.h          # 任务描述符契约
│   │   ├── uapi_compat.h        # 用户态/内核态 ABI 辅助
│   │   └── syscalls.h sched.h memory_types.h cognition_types.h
│   │       security_types.h lsm_types.h log_types.h bpf_struct_ops.h
│   └── third_party/             # 内嵌第三方头（如 nghttp2）
├── utils/                       # 32 个工具模块（另有 utils/include 共享头目录）
│   ├── include/                 # 跨模块共享头
│   │   ├── atomic_compat.h      # 跨平台原子操作（总入口）
│   │   ├── atomic_compat_api.h  # 按类型的原子 API
│   │   ├── atomic_compat_platform.h # 原子后端选择
│   │   ├── logging_compat.h     # 日志兼容垫片
│   │   └── check.h              # 通用检查宏
│   ├── logging/  sync/  memory/  string/  ipc/  token/  cost/
│   ├── observability/  platform/  error/  types/  config_unified/
│   ├── execution/  io/  cache/  compat/  cognition/  strategy/
│   ├── network/  security/  resource/  uuid/  print/  compliance/
│   ├── quality/  sd/  effect/  ext/  id/  task/  cjson/  ime/
│   └── <module>/                # 每个模块目录扁平存放：头文件与源码同目录
└── tests/                       # 测试套件（unit / bench / 辅助框架）
```

每个工具模块的公共头与实现文件都直接放在模块根目录下——没有按模块划分的
`include/` 或 `src/` 子目录。每个模块目录内都有自己的 README 说明 API 细节。

## 模块列表

| 模块 | 职责 |
|------|------|
| logging | 三层日志（core → atomic → service）；JSON/文本格式 |
| sync | 同步原语（mutex、递归 mutex、rwlock、spinlock、信号量、条件变量、barrier、event）、取消令牌、线程池、事件循环、定时器 |
| memory | 统一分配宏、内存池、预分配、守护、统计、调试辅助 |
| string | 字符串操作与安全字符串工具 |
| ipc | IPC 抽象（channel/server/client/shm/mq/rpc）、JSON-RPC 辅助、熔断器 |
| token | LLM token 计数与预算；API Key 标准 |
| cost | 成本估算与预算控制器 |
| observability | 指标、跟踪、结构化日志门面、统一指标、告警管理器 |
| platform | 平台适配工具（文件系统、环境变量、路径、时间、系统信息） |
| error | 错误处理宏与用户态扩展错误码 |
| types | 共享类型定义（通用类型、cupolas/LLM/工具类型头文件） |
| config_unified | 三层配置（core → source → service），内置极简 YAML 解析器 |
| execution | 任务检查点（持久化 / 会话 / 快照） |
| io | 文件读写、目录辅助 |
| cache | LRU / TTL 缓存 |
| compat | 跨平台兼容（regex、dirent、mman、netdb、unistd） |
| cognition | 认知辅助（agent 信息、规划、协调） |
| strategy | 加权评分与 agent 选择策略 |
| network | HTTP 客户端、连接池、DNS 解析 |
| security | 输入校验、日志脱敏 |
| resource | 资源守护、配额、API 恢复 |
| uuid | UUID 生成与解析 |
| print | 统一运行时打印宏（airy_print_*） |
| compliance | 严格合规的不安全函数投毒与豁免宏 |
| quality | 代码质量检查宏（空指针检查、作用域守护、数值安全） |
| sd | 跨进程服务发现（shm 注册表、心跳、过期） |
| effect | 回滚效应作用域（先注册、逆序撤销） |
| ext | 统一 provider 注册表（LLM / tool / storage / sandbox 域） |
| id | 品牌化 ID 生成（trace_id / msg_id） |
| task | 任务描述符创建与 CRC32 完整性校验 |
| cjson | cJSON 辅助宏（解析守护、自动释放、深拷贝） |
| ime | 轻量拼音输入法（随安装交付二进制词典） |

## 用法与构建

commons 通常作为 [agentrt](https://atomgit.com/openairymax/agentrt) 树的一部分
构建（由根 `CMakeLists.txt` 通过 `add_subdirectory` 引入），对外暴露
`airy_common` target：

```bash
cmake -S agentrt -B agentrt/build -DCMAKE_BUILD_TYPE=Release
cmake --build agentrt/build --parallel
```

消费者只需链接这一个 target：

```cmake
target_link_libraries(my_component PRIVATE airy_common)
```

所有模块的 include 目录均以 `PUBLIC` 导出，链接 `airy_common` 后全部
子模块头文件即可见。

**关键构建选项**（默认值以 agentrt 根 `CMakeLists.txt` 实测为准）：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | `ON` | 构建单元测试与基准测试 |
| `BUILD_SHARED_LIBS` | `OFF` | commons 为静态库 |
| `WARNINGS_AS_ERRORS` | `OFF` | CI 中将警告提升为错误 |
| `ENABLE_SANITIZERS` | `OFF` | ASan/UBSan 插桩 |
| `AIRY_COMPLIANCE_STRICT` | `ON` | 通过 `utils/compliance/banned_functions.h` 投毒不安全 libc 函数（以 `-include` 全工程注入） |
| `AIRY_HAS_CJSON` | 自动 | 由依赖检测设置；控制 cJSON 代码路径 |
| `AIRY_HAS_YAML` | 自动 | 由依赖检测设置；控制 libyaml 代码路径 |

cJSON 或 libyaml 不可用时 commons 自动降级：定义 `AIRY_NO_CJSON`，
配置解析回退到内置的极简 YAML 解析器（`utils/config_unified/yaml_minimal`）。

**安装布局：** 静态库与公共头安装到 `include/agentrt/{platform,utils/*}`；
IME 词典安装到 `share/agentrt/ime`。

### 快速示例

```c
#include "airy_types.h"
#include "logging.h"
#include "config_unified.h"

int main(void) {
    log_config_t log_cfg = {0};
    log_init(&log_cfg);

    config_context_t *ctx = config_context_create("myapp");
    config_context_set(ctx, "server.host", CONFIG_STRING("0.0.0.0"));

    log_write(LOG_LEVEL_INFO, "demo", __LINE__, "host=%s",
              CONFIG_GET_STRING_SAFE(ctx, "server.host", "localhost"));

    config_context_destroy(ctx);
    log_cleanup();
    return 0;
}
```

## 与其他模块的关系

```
┌──────────────────────────────────────────────┐
│                  应用层                       │
├──────────────────────────────────────────────┤
│          daemons / gateway / protocols       │
├──────────────────────────────────────────────┤
│           atoms / cupolas / heapstore        │
├──────────────────────────────────────────────┤
│                ★ commons ★                   │  ← 最底层，零上游依赖
├──────────────────────────────────────────────┤
│              操作系统 / 硬件                   │
└──────────────────────────────────────────────┘
```

- **atoms** — 平台抽象、类型/错误契约以及大部分工具模块
  （logging、sync、memory、error、types、config_unified、observability 等）。
- **cupolas** — 类型系统、同步原语、内存宏、安全与资源工具。
- **heapstore** — 日志、配置、内存池、同步原语。
- **protocols** — `airy_err_t`、`struct airy_ipc_msg_hdr`、sync、observability。
- **gateway** — 网络工具、token 管理、日志、配置。
- **daemons** — 全量接口：logging、config、network、token、cost、
  observability、cognition、strategy、IPC、service discovery。

各语言 SDK 最终绑定回同一套 commons 类型与错误码，保持运行时的宿主端
与嵌入式端一致。

## 文档

项目文档位于 [openairymax/docs](https://atomgit.com/openairymax/docs) 的
`docs/AirymaxRT/` 目录下，其中包含治理本仓库的工程标准规范手册。

## 许可

Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者。

本模块以下列两种许可之一双许可发布：

- **GNU Affero 通用公共许可证 v3.0 或更新版本**
  （[AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.txt)），或
- **Apache 许可证 2.0 版本**
  （[Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0.txt)）

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`

完整许可文本见 [LICENSE](LICENSE) 文件，版权声明见 [NOTICE](NOTICE)。
你可以任选其一遵守。提供 Apache-2.0 备选许可是为了适配 AGPL 无法容纳的
下游集成场景。
