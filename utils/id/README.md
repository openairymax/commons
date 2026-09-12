# id — 品牌化 ID 生成

**模块路径**: `commons/utils/id/` · **版本**: 0.1.15

`trace_id` 与 `msg_id` 两类品牌化标识的生成、比较与结构化命名（字符串化/解析）。类型与函数声明位于 `commons/include/airy_types.h` 的品牌化 ID 段，本目录仅提供实现。

## 概述

- **类型层隔离**：`airy_trace_id_t` 与 `airy_msg_id_t` 是两个独立的结构体（各包装一个 `uint64_t value`），编译器阻止相互赋值或混用；与 IPC 线格式交互时取 `.value` 数值。
- **trace_id**：64 位熵，来自 splitmix64 混合（首次调用以时间与函数地址（ASLR）经原子 CAS 播种，其后经原子状态计数器推进），生成即近似全局唯一；字符串形态 `tr-<16 hex>`。
- **msg_id**：高 32 位为秒级时间戳、低 32 位为进程内原子单调序列，字符串形态 `msg-<ts:08x>-<seq:08x>`，可按时间大致排序；时间戳回绕或跨进程不保证全局唯一。
- **线程安全**：生成与比较均为无锁原子操作，可并发调用。

## 目录结构

```
utils/id/
├── airy_id.c    生成/比较/字符串化/解析实现（C11 原子，无外部依赖）
└── README.md
```

## 数据结构与常量

声明于 `airy_types.h`：

| 名称 | 说明 |
|---|---|
| `airy_trace_id_t` / `airy_msg_id_t` | 品牌化 ID 结构体（`.value` 为 `uint64_t`） |
| `AIRY_TRACE_ID_NULL` / `AIRY_MSG_ID_NULL` | 零值常量，亦用作解析失败返回值 |
| `AIRY_TRACE_ID_STR_MAX` / `AIRY_MSG_ID_STR_MAX` | 字符串化所需最小缓冲，均为 `24`（含前缀、分隔符与终止符） |

## 接口

| 函数 | 语义 |
|---|---|
| `airy_trace_id_generate()` | 生成一个 trace_id（64 位熵） |
| `airy_msg_id_generate()` | 生成一个 msg_id（时间戳 \| 单调序列） |
| `airy_trace_id_eq(a, b)` / `airy_msg_id_eq(a, b)` | 相等比较，返回 1/0 |
| `airy_trace_id_to_string(id, out, out_cap)` | 写入 `tr-<16 hex>`；`out` 为 NULL 或 `out_cap` 为 0 时不做任何事，容量不足时安全截断且保持 NUL 终止 |
| `airy_msg_id_to_string(id, out, out_cap)` | 写入 `msg-<ts:08x>-<seq:08x>`，缓冲语义同上 |
| `airy_trace_id_from_string(str)` | 解析 `tr-` 前缀 + 恰 16 位十六进制（大小写均可）；NULL、前缀或长度不符、含非 hex 字符时返回 `AIRY_TRACE_ID_NULL` |
| `airy_msg_id_from_string(str)` | 解析 `msg-` + 8 hex + `-` + 8 hex；非法输入返回 `AIRY_MSG_ID_NULL` |

注意：`value == 0` 与"解析失败"都以 `*_NULL` 表达；生成侧的熵与时间戳构造使零值实际不可达。

## 用法示例

```c
#include "airy_types.h"

void name_and_roundtrip(void)
{
    airy_trace_id_t tid = airy_trace_id_generate();
    char buf[AIRY_TRACE_ID_STR_MAX];
    airy_trace_id_to_string(tid, buf, sizeof(buf)); /* "tr-1a2b3c4d5e6f7a8b" */

    airy_trace_id_t parsed = airy_trace_id_from_string(buf);
    if (airy_trace_id_eq(parsed, tid)) {
        /* 往返一致 */
    }

    airy_msg_id_t mid = airy_msg_id_generate();
    char mbuf[AIRY_MSG_ID_STR_MAX];
    airy_msg_id_to_string(mid, mbuf, sizeof(mbuf)); /* "msg-66a1b2c3-00000001" */
}
```

## 构建与依赖

`airy_id.c` 随 `airy_common` 静态库编译。实现仅依赖 `airy_types.h` 与 C 标准库（`stdatomic.h`、`stdio.h`、`time.h` 等），无 agentrt 内部上游依赖，也不引入链接依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
