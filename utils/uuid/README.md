# UUID — UUID 生成模块

**模块路径**: `commons/utils/uuid/` · **版本**: 0.1.15

为各类实体生成 RFC 4122 v4 风格的随机 UUID 字符串，支持带前缀标识、格式校验与二进制/字符串双向转换。

## 概述

UUID 模块提供 36 字符标准格式（`xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`）的
UUID 生成与处理能力：随机字节取自各平台的系统熵源，版本位与变体位按
RFC 4122 v4 写定；另支持在 UUID 前拼接调用方指定的前缀（如 `task_`、`mem_`），
以便在日志与调试中按前缀区分实体类型。模块自带一次性初始化状态与生成计数，
均通过原子操作维护，可并发调用。

## 目录结构

```
uuid/
├── uuid_generator.h           # API 声明与常量、错误码定义
├── uuid_generator.c           # 跨平台实现
└── README.md                  # 本文档
```

## 数据结构与常量

### airy_uuid_error_t — 返回码

本模块使用独立的负值小枚举，不复用 `utils/error` 的全局错误码体系。

| 枚举值 | 数值 | 说明 |
|------|------|------|
| `AIRY_UUID_SUCCESS` | 0 | 成功 |
| `AIRY_UUID_EINVALID` | -1 | 参数无效（空指针或缓冲区过小） |
| `AIRY_UUID_ENOMEM` | -2 | 预留；当前实现的任何 API 均不返回此值 |
| `AIRY_UUID_EUNAVAIL` | -3 | 熵源不可用或读取失败 |

### 缓冲区长度常量

| 宏 | 值 | 说明 |
|------|------|------|
| `AIRY_UUID_STR_LEN` | 37 | 标准 UUID 字符串缓冲（36 字符 + 空字符） |
| `AIRY_UUID_PREFIXED_STR_LEN` | 64 | 带前缀 UUID 字符串缓冲上限 |

## 接口

| 函数 | 说明 |
|------|------|
| `airy_uuid_init()` | 初始化生成器：探测平台熵源可用性并置一次性初始化标志 |
| `airy_uuid_cleanup()` | 复位初始化状态与计数，之后可再次初始化 |
| `airy_uuid_v4(out_buf, buf_len)` | 生成标准 v4 字符串（小写十六进制），要求 `buf_len >= 37` |
| `airy_uuid_with_prefix(prefix, out_buf, buf_len)` | 生成 `prefix` + UUID 的拼接串，要求 `buf_len >= 64` |
| `airy_uuid_is_valid(uuid)` | 结构校验：合法返回 1，否则返回 0 |
| `airy_uuid_bin_to_str(uuid_bin, out_buf, buf_len)` | 16 字节二进制渲染为字符串 |
| `airy_uuid_str_to_bin(uuid_str, out_bin)` | 字符串解析为 16 字节二进制 |

## 语义与约束

- **惰性初始化**：`airy_uuid_v4()` 在未初始化时自动调用 `airy_uuid_init()`；
  初始化经原子 CAS 仅执行一次，Windows 下以顺序 UUID 接口探测、POSIX 下以
  打开 `/dev/urandom` 探测熵源，探测失败返回 `AIRY_UUID_EUNAVAIL`。
- **熵源按平台选择**：Linux 每次生成打开并读取 `/dev/urandom`；Windows 使用
  系统 RPC UUID 接口；Apple 使用系统 `uuid_generate`；其余平台优先读
  `/dev/urandom`，失败时回退到 platform 抽象层的 `airy_random_bytes()`。
- **版本与变体位**：生成后统一写定第 7 字节高位为 `4`、第 9 字节高位为
  `10xx`，因此输出恒为 v4 / RFC 4122 变体格式。
- **前缀原样拼接**：`airy_uuid_with_prefix()` 不在前缀后自动补分隔符，
  需要 `mem_` 这类样式时前缀字符串本身须含结尾下划线；拼接结果超过
  `buf_len`（至少 64）时按 `snprintf` 语义截断，超长前缀不会额外报错。
- **校验仅查结构**：`airy_uuid_is_valid()` 接受长度 36 或 `{...}` 包裹的 38，
  检查连字符位于第 8/13/18/23 位、其余为十六进制字符；不校验版本位与
  变体位，非 v4 的合法格式 UUID 同样通过。
- **str_to_bin 的输出契约**：`out_bin` 须为调用方保证的至少 16 字节缓冲，
  函数不接收长度参数；解析前先执行结构校验，失败返回 `AIRY_UUID_EINVALID`。
- **覆盖情况**：commons 单元测试套件当前未包含本模块的专项用例；commons 内
  亦无模块调用本模块 API。在本仓库源码区中，coreloopthree 原子的 ID 工具
  （`atoms/coreloopthree/src/utils/id_utils.c`）调用 `airy_uuid_v4()` 生成标识。
  `utils/types` 另定义了同长的 `typedef char airy_uuid_t[37]` 字符串缓冲类型，
  与该生成器 API 无调用关系。

## 用法示例

```c
#include "uuid_generator.h"
#include <stdint.h>

/* 生成两类实体标识：前缀字符串本身须含结尾下划线。 */
airy_uuid_error_t prepare_ids(char *task_id, char *mem_id)
{
    airy_uuid_error_t err =
        airy_uuid_with_prefix("task_", task_id, AIRY_UUID_PREFIXED_STR_LEN);
    if (err != AIRY_UUID_SUCCESS) {
        return err;
    }
    return airy_uuid_with_prefix("mem_", mem_id, AIRY_UUID_PREFIXED_STR_LEN);
}

/* 外部传入的文本 UUID 先做结构校验，再持久化为 16 字节二进制。 */
int persist_binary(const char *uuid_str, uint8_t out_bin[16])
{
    if (!airy_uuid_is_valid(uuid_str)) {
        return -1;
    }
    return (airy_uuid_str_to_bin(uuid_str, out_bin) == AIRY_UUID_SUCCESS) ? 0 : -1;
}
```

## 构建与依赖

本模块无独立 CMakeLists，源文件直接并入单一静态库 target `airy_common`：

- `uuid_generator.c` 列入库源文件列表；
- `utils/uuid` 以 PUBLIC 方式注册为库的 include 目录，使用方仅需
  `target_link_libraries(<target> PRIVATE airy_common)`；
- 本模块头文件未列入安装规则，若需对外安装需随库整体处理。

| 依赖 | 用途 |
|------|------|
| `commons/platform` | 非主流平台的随机字节回退（`airy_random_bytes()`） |
| `commons/utils/include` | `atomic_compat.h`：一次性初始化与计数的原子操作 |
| C 标准库 / 系统接口 | `snprintf`、`strtol`；`/dev/urandom` 及各平台原生 UUID API |

本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
