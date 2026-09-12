# Tests — Commons 测试套件

**模块路径**: `commons/tests/`
**版本**: 0.1.15

## 概述

Tests 是 Commons 统一基础库的测试套件，覆盖平台抽象、错误处理、日志系统、
令牌管理、成本控制、IPC、输入法、正则引擎等核心模块。测试套件使用
CMake + CTest 框架，支持自动化构建和回归检测。

## 设计目标

- **全面覆盖**：覆盖 Commons 所有核心模块的基础功能路径
- **快速执行**：单元测试设计为轻量级，整体执行时间控制在分钟级
- **自动化集成**：通过 CTest 集成到 CI 流水线，每次提交自动执行
- **隔离性**：每个测试用例独立运行，不依赖外部资源和特定环境

## 目录结构

```
tests/
├── CMakeLists.txt               # 测试构建配置
├── README.md                    # 本文档
├── test_sc_headers.c            # 系统契约头（include/airymax/）编译自检
├── utils/                       # 测试工具框架
│   ├── test_framework.h         # 通用测试框架（断言宏、测试注册）
│   ├── test_macros.h            # 测试辅助宏（EXPECT_EQ、ASSERT_TRUE 等）
│   └── cmocka_stub.h            # cmocka 兼容适配层
├── unit/                        # 单元测试（test_*.c，IPC 按功能域拆分）
│   ├── test_platform.c / test_platform_time.c
│   ├── test_error.c / test_logger.c / test_print.c
│   ├── test_token.c / test_cost.c / test_io.c
│   ├── test_ipc.c + test_ipc_channel.c / test_ipc_send.c / test_ipc_server.c
│   │              test_ipc_shm_mq.c / test_ipc_message.c / test_ipc_rpc.c
│   │              test_ipc_internal.h
│   ├── test_cancel_token.c / test_airy_effect.c / test_airy_ext.c
│   ├── test_airy_id.c / test_airy_regex.c / test_ime.c
│   └── test_types.c / test_network.c / test_string_utils.c
│       test_observability.c / test_resource_guard.c / test_input_validator.c
└── bench/                       # 性能基准
    └── bench_platform_perf.c    # 平台层性能基准
```

## 测试框架

### 测试辅助宏

`test_macros.h` 提供类 xUnit 风格的断言宏：

| 宏 | 说明 |
|-----|------|
| `TEST_ASSERT(cond)` | 断言条件为真，失败则终止测试 |
| `TEST_ASSERT_EQUAL(expected, actual)` | 断言值相等 |
| `TEST_ASSERT_NOT_NULL(ptr)` | 断言指针非空 |
| `TEST_ASSERT_NULL(ptr)` | 断言指针为空 |
| `TEST_ASSERT_STR_EQUAL(expected, actual)` | 断言字符串相等 |

### 通用测试框架

`test_framework.h` 提供测试注册和执行基础设施：

| 功能 | 说明 |
|------|------|
| 测试注册 | 通过宏自动注册测试函数到测试套件 |
| 结果汇总 | 自动统计通过/失败/跳过的测试数量 |
| 输出格式化 | 统一的测试输出格式，CI 友好 |
| 内存泄漏检测 | 可选的内存追踪模式 |

### cmocka 适配

`cmocka_stub.h` 提供 cmocka 风格的兼容适配层，确保测试代码在无 cmocka
环境下也能编译执行。

## 构建与运行

### 启用测试

```bash
# 在项目根目录构建并启用测试
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build
```

### 运行所有测试

```bash
cd build && ctest --output-on-failure
```

### 运行特定测试

```bash
# 运行单个测试
ctest -R test_platform

# 运行所有单元测试
ctest -R "^commons_test_"

# 并行运行
ctest -j $(nproc) --output-on-failure
```

### 运行指定测试可执行文件

```bash
# 可执行文件位于构建目录下的 commons/tests/ 中
./build/**/commons/tests/test_platform
./build/**/commons/tests/test_error
```

## 已接入构建的测试

以下测试在 `tests/CMakeLists.txt` 中注册为 CTest 用例（前缀 `commons_test_`）：

| 测试 | 覆盖模块 |
|------|----------|
| `test_platform` | 平台抽象层（含 sysinfo / 文件锁 / 线程命名） |
| `test_platform_time` | 时间服务（逻辑墙钟 / 时区 / SNTP 校对 / 周期同步） |
| `test_error` | 错误处理框架 |
| `test_logger` | 日志系统 |
| `test_token` | 令牌管理 |
| `test_cost` | 成本估算与控制 |
| `test_print` | 打印工具 |
| `test_cancel_token` | 取消令牌（异步可中断） |
| `test_airy_effect` | 统一作用域 effect 原语 |
| `test_airy_ext` | 统一扩展注册表 + 四域 provider 契约 |
| `test_airy_id` | 品牌化 ID（trace_id/msg_id） |
| `test_airy_regex` | POSIX ERE 引擎（airy_re_*） |
| `test_ime` | 内置拼音输入法词典 |
| `test_io` | 文件 CRUD 跨平台往返 |
| `test_ipc` | IPC 抽象层（按功能域拆分 6 个文件；POSIX 平台专属） |

`unit/` 下另保留若干测试源码（如 `test_string_utils.c`、
`test_observability.c`、`test_resource_guard.c`、`test_input_validator.c`），
其构建接线以 `tests/CMakeLists.txt` 为准。

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `airy_common` | Commons 聚合库，所有测试的链接目标 |
| `airy_core` | 核心库（由 agentrt 构建树提供） |
| CMake 3.20+ | 构建系统 |
| CTest | 测试执行框架 |

---

Copyright (c) 2025-2026 SPHARX Ltd.

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`（双许可，任选其一遵守，
完整文本见仓库根 [LICENSE](../LICENSE)）。