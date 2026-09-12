# ext — 统一扩展注册表

**模块路径**: `commons/utils/ext/` · **版本**: 0.1.15

进程级的能力接缝注册表：以 `(domain, name)` 为唯一键，在 memory / LLM / tool / storage / sandbox 五个域内注册、查找并遍历 provider 扩展；注册表只持有通用扩展头，域专有调用经不透明 vtable 转发。

## 概述

- **注册表模型**：每个扩展为一个 `airy_extension_t`（域、名称、版本、能力标记、vtable、impl）。`(domain, name)` 重复注册为**覆盖**语义（计数不变，vtable/impl 被替换）。
- **vtable 不透明**：注册表不感知各域 vtable 的内部结构。本目录提供 LLM / tool / storage / sandbox 四域的 vtable 契约头与注册辅助函数；memory 域的 vtable 类型由上层内存能力接口定义（不在本模块内），注册表按其存放不透明指针。
- **字符串生命周期**：`name`、`version` 注册时深拷贝，调用方可传栈缓冲或字面量；`vtable` 与 `impl` 仅存指针，其生命周期必须覆盖整个注册期。
- **线程安全**：注册表内部以互斥锁保护，注册/注销/查询/计数可并发调用；注册与注销通常发生在启动或配置阶段。
- **容量上限**：每域最多 `AIRY_EXT_MAX_PER_DOMAIN`（16）个条目，超出时注册失败并返回负错误码。

## 目录结构

```
utils/ext/
├── airy_ext.h                 注册表公开契约：域枚举、能力标记、扩展条目、7 个注册表 API
├── airy_ext.c                 注册表实现：按域定长数组 + 互斥锁，name/version 深拷贝
├── airy_llm_provider.h        LLM 域 vtable 契约与注册辅助
├── airy_tool_provider.h       Tool 域 vtable 契约与注册辅助
├── airy_storage_provider.h    Storage 域 vtable 契约与注册辅助
├── airy_sandbox_provider.h    Sandbox 域 vtable 契约与注册辅助
├── airy_providers.c           四域注册辅助实现（构造条目并挂载到统一注册表）
└── README.md
```

## 数据结构

| 类型 | 说明 |
|---|---|
| `airy_ext_domain_t` | 扩展域：`AIRY_EXT_DOMAIN_MEMORY` / `_LLM` / `_TOOL` / `_STORAGE` / `_SANDBOX`，另有哨兵 `_MAX` |
| `airy_ext_capabilities_t` | `feature_bits`（按各域契约自行解释）+ `flags`（通用位） |
| `airy_extension_t` | 注册条目：`domain`、`name`、`version`、`capabilities`、`vtable`（`const void *`，域专有）、`impl`（实现实例数据） |
| `AIRY_EXT_FLAG_BUILTIN` | `0x1`，内置实现 |
| `AIRY_EXT_FLAG_REMOTE` | `0x2`，远程 / RPC 后端（daemon 代理） |
| `AIRY_EXT_FLAG_OPTIONAL` | `0x4`，可选扩展 |
| `AIRY_EXT_MAX_PER_DOMAIN` | 每域注册上限，`16` |

## 注册表接口

所有函数定义于 `airy_ext.h`。返回码为 `airy_err_t`：成功 `AIRY_SUCCESS`，失败为负值错误码。

| 函数 | 语义 |
|---|---|
| `airy_ext_register(&ext)` | 注册或覆盖一个扩展。`ext`/`name`/`vtable` 为 NULL 或域越界返回 `AIRY_ERR_NEG(AIRY_EINVAL)`；域容量满返回负错误码 |
| `airy_ext_unregister(domain, name)` | 注销一个扩展；条目不存在返回 `AIRY_ERR_NEG(AIRY_ENOENT)` |
| `airy_ext_get(domain, name)` | 按 `(domain, name)` 查找。返回注册表持有的**内部只读指针**（注销或清空后失效），未找到或非法参数返回 NULL |
| `airy_ext_count(domain)` | 域内已注册条目数 |
| `airy_ext_foreach(domain, fn, ud)` | 域内遍历，按注册槽位顺序回调；`fn` 为 NULL 时忽略。回调在注册表锁内执行 |
| `airy_ext_clear(domain)` | 清空域内全部扩展（释放深拷贝的 `name`/`version`，不触碰 `vtable`/`impl`） |
| `airy_ext_domain_name(domain)` | 域名字符串字面量（`memory`/`llm`/`tool`/`storage`/`sandbox`）；非法域返回 `"UNKNOWN"` |

## 域 provider 契约与注册辅助

四域各有一个契约头，vtable 结构本身即作为 `airy_extension_t.vtable` 挂载（`e->vtable` 可转回对应 `airy_*_provider_t *`）。每域提供一对辅助函数：

| 域 | vtable 成员 | 注册必填成员 | 注册时 flags |
|---|---|---|---|
| LLM（`airy_llm_provider.h`） | `complete`、`complete_stream`、`is_connected` | `complete` | `AIRY_EXT_FLAG_REMOTE` |
| Tool（`airy_tool_provider.h`） | `list`、`execute`、`execute_stream` | `execute` | `AIRY_EXT_FLAG_REMOTE` |
| Storage（`airy_storage_provider.h`） | `get`、`set`、`delete`、`list` | `get` 且 `set` | `AIRY_EXT_FLAG_BUILTIN` |
| Sandbox（`airy_sandbox_provider.h`） | `is_available`、`describe` | `is_available` | `AIRY_EXT_FLAG_BUILTIN` |

- `airy_<domain>_provider_register(provider)`：以 `provider->name` 为键注册，`provider->version` 为 NULL 时记作 `"0.0.0"`；必填成员缺失返回 `AIRY_ERR_NEG(AIRY_EINVAL)`。
- `airy_<domain>_provider_unregister(name)`：委托 `airy_ext_unregister` 到对应域。
- LLM / Tool 域以 JSON 字符串为统一传输形态（与运行时 daemon 的 JSON-RPC 契合）：`complete` / `execute` 等接口中由实现方分配的出参字符串（`out_response_json`、`out_tools_json`、`out_value`、`out_keys` 等）由调用方释放（`AIRY_FREE`）。
- Sandbox 域为能力描述型：`is_available` 探测 `AIRY_SANDBOX_CAP_LANDLOCK` / `AIRY_SANDBOX_CAP_SECCOMP` 等能力是否可用，`describe` 返回字符串字面量（无需释放）。执行隔离本身由各平台机制承担，此处仅供上层决策。
- Storage 域 `get` 在 key 不存在时约定返回 `AIRY_ENOENT`；`list` 的 `prefix` 可为 NULL 表示全部。

## 语义与约束

- `airy_ext_get` 返回的指针在对应条目被注销或清空后失效；需要跨阶段持有时应复制所需字段或重新查找。
- 禁止在 `airy_ext_foreach` 回调中再调用本模块的注册/注销/清空接口（回调在注册表锁内执行）。
- 覆盖注册会释放旧条目的 `name`/`version` 深拷贝并替换全部字段；若旧 `vtable` 指针正被其他线程使用，需自行保证不被并发替换。
- 新增扩展域需同时扩展 `airy_ext_domain_name()` 与注册表容量数组。

## 用法示例

```c
#include "airy_ext.h"
#include "airy_llm_provider.h"

/* provider 结构体生命周期须覆盖注册期；注册表仅存其指针 */
static airy_llm_provider_t g_llm;

static airy_err_t echo_complete(airy_llm_provider_t *p, const char *request_json,
                                char **out_response_json)
{
    (void)p;
    *out_response_json = strdup(request_json); /* 示例：回显请求 */
    return (*out_response_json != NULL) ? AIRY_SUCCESS : AIRY_ERR_NEG(AIRY_ENOMEM);
}

int provide_and_consume(void)
{
    g_llm.name = "echo_llm";
    g_llm.version = "1.0.0";
    g_llm.complete = echo_complete;

    if (airy_llm_provider_register(&g_llm) != AIRY_SUCCESS) {
        return -1;
    }

    const airy_extension_t *e = airy_ext_get(AIRY_EXT_DOMAIN_LLM, "echo_llm");
    if (e == NULL) {
        return -1;
    }
    airy_llm_provider_t *llm = (airy_llm_provider_t *)e->vtable;

    char *response = NULL;
    if (llm->complete(llm, "{\"prompt\":\"hi\"}", &response) != AIRY_SUCCESS) {
        airy_llm_provider_unregister("echo_llm");
        return -1;
    }
    /* ... 使用 response ... */
    AIRY_FREE(response); /* 契约：实现方分配的出参由调用方释放 */

    airy_llm_provider_unregister("echo_llm");
    return 0;
}
```

## 构建与依赖

`airy_ext.c` 与 `airy_providers.c` 随 `airy_common` 静态库编译；`utils/ext/` 为公开头目录，头文件随安装规则导出。消费方仅需链接 `airy_common` 并包含 `airy_ext.h`（及所需域的契约头）。

| 依赖 | 用途 |
|---|---|
| `airy_types.h` / `error.h` | 错误码与 `airy_err_t` / `AIRY_ERR_NEG` |
| `airy_memory.h`（utils/memory） | `name`/`version` 深拷贝与释放 |
| `sync.h`（utils/sync） | 注册表互斥锁 |

本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
