# task — A-TD 任务描述符

**模块路径**: `commons/utils/task/` · **版本**: 0.1.15

提供 A-TD（Airymax Task Descriptor）128 字节任务描述符的创建、CRC32 计算与完整性校验实现。契约头文件位于 commons 的共享头目录 `commons/include/airymax/task_desc.h`，实现编译进静态库 `airy_common`。

## 概述

A-TD 是任务提交/分发路径上的自描述结构：固定 128 字节头部、`'AGTS'` magic、单调时钟时间戳与覆盖头部和载荷的 CRC32。本模块只做「构造 + 校验」两件事，不包含任务调度或投递逻辑。

## 目录结构

```
utils/task/
├── task_desc.c            # crc32 / create / validate 实现
└── README.md

include/airymax/
└── task_desc.h            # 契约头（布局、常量、API 声明）
```

## 布局与常量

`struct airy_task_desc_hdr` 为 128 字节、64 字节对齐，头部偏移由 `_Static_assert` 锁定（`magic`@0、`opcode`@6、`payload_len`@56、`crc32`@68、`reserved`@72）：

| 常量 | 值 | 说明 |
|------|-----|------|
| `AIRY_TASK_DESC_MAGIC` | `0x41475453`（`'AGTS'`） | 独立于 IPC 消息头 magic |
| `AIRY_TASK_DESC_VERSION` | 1 | 当前版本 |
| `AIRY_TASK_OP_*` | 0x0001–0x0004 | CREATE / EXECUTE / CANCEL / STATUS |
| `AIRY_TASK_FLAG_CRITICAL` / `DETACHED` | 0x0001 / 0x0002 | 可用标志位 |
| `AIRY_TASK_FLAG_RESERVED` | `0xFFFC` | 保留位掩码，必须为 0 |

## 接口

| 函数 | 说明 |
|------|------|
| `airy_task_desc_crc32(data, len)` | CRC-32（IEEE 802.3，反射形式，多项式 `0xEDB88320`，初值/终值异或 `0xFFFFFFFF`）；`len == 0` 时返回 0 |
| `airy_task_desc_create(desc, opcode, task_id, parent_task_id, deadline_ns, src_task, dst_task, payload, payload_len, flags, priority)` | 清零整个头部后填充各字段，`submit_time_ns` 取 `airy_time_ns()` 单调时钟，最后计算 CRC32。`desc` 为 NULL、`payload == NULL` 而 `payload_len != 0`、`flags` 含保留位时返回 `AIRY_EINVAL` |
| `airy_task_desc_validate(desc, payload, payload_len)` | 依序检查 magic（`AIRY_EIPC_MAGIC`）、version（`AIRY_ECFGVERSION`）、flags 保留位（`AIRY_EIPC_FLAGS`）、`reserved` 全零（`AIRY_EIPC_RESERVED`）、载荷长度一致（`AIRY_EIPC_PAYLOAD`）、CRC32（`AIRY_EIPC_CRC32`），全部通过返回 `AIRY_EOK` |

## 语义与约束

- CRC32 实际覆盖**头部前 68 字节**（`offsetof(crc32)` 之前的全部内容，`crc32` 字段自身除外）**加上传入的载荷**；头文件注释中「header[0:72)」的表述与实现不符，以 `offsetof` 计算为准。
- `create` 与 `validate` 之间载荷内容不得改变，否则 CRC 校验必然失败。
- `validate` 要求传入的 `payload_len` 与 `desc->payload_len` 一致（头部为 0 时传入长度也必须为 0），且不小于头部声明的载荷长度。
- 注意区分同目录 `airymax/sched.h` 中的 64 字节 `struct airy_task_desc`（调度运行时描述符）：两者 magic 同为 `'AGTS'` 但布局与用途不同，属不同契约域，不得混用。
- 三个函数均无内部状态，可并发调用。
- `airymax/task_desc.h` 与 `task_desc.c` 的文件级 SPDX 声明为 `BSD-3-Clause OR GPL-2.0`，区别于 commons 其余代码的双许可口径。

## 用法示例

```c
#include <airymax/task_desc.h>

/* 构造并校验一个 EXECUTE 描述符（载荷在两步入参间须保持不变） */
int probe_desc(__u64 task_id, const void *payload, __u32 len)
{
    struct airy_task_desc_hdr desc;
    airy_err_t err;

    err = airy_task_desc_create(&desc, AIRY_TASK_OP_EXECUTE, task_id, 0ULL,
                                0ULL, 0ULL, 0ULL, payload, len,
                                AIRY_TASK_FLAG_DETACHED, 0U);
    if (err != AIRY_EOK) {
        return -1;
    }
    return airy_task_desc_validate(&desc, payload, len) == AIRY_EOK ? 0 : -1;
}
```

## 构建与依赖

`task_desc.c` 由 `commons/CMakeLists.txt` 列入 `airy_common` 源列表。`utils/task` 目录本身没有独立的 include 注册项——契约头经 `commons/include/` 的 `#include <airymax/task_desc.h>` 路径使用，并随 `install(DIRECTORY include/airymax/)` 导出到 `include/airymax/`。

| 依赖 | 用途 |
|------|------|
| `commons/include/airymax/task_desc.h` | 布局、常量与 API 声明 |
| `commons/include/airymax/error.h` | `AIRY_EOK`、`AIRY_EINVAL`、`AIRY_EIPC_*` 错误码 |
| `commons/include/airymax/uapi_compat.h` | `__u8`–`__u64` 整数别名 |
| `platform_misc.h`（`commons/platform/`） | `airy_time_ns()` 单调时钟 |

本模块无 agentrt 内部上游依赖。当前仓库内尚无本模块 API 的调用方，commons 测试套件亦无专项用例（`test_sc_headers.c` 覆盖的是 `airymax/sched.h` 的 64 字节调度描述符，与本模块无关）。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
