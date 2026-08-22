# airy_ext — 统一扩展注册表（能力契约环）

`airy_ext` 是 AirymaxRT 的**统一扩展注册表**（阶段 2 统一扩展机制），位于
`commons/utils/ext`。它将 memory 域的"能力接缝"（provider vtable + 注册/获取）
模式统一推广到 **LLM / tool / storage / sandbox / memory** 五域：每个扩展以
`(domain, name)` 为唯一注册键，挂载域专有 vtable（Definition）+ 实现实例
（Provider），消费方按 key 查找后经 vtable 调用（Consumer）——即
"契约-实现-消费"三分离的能力契约环。

## 扩展域

| 域 | vtable 契约头 | 职责 |
|----|--------------|------|
| `AIRY_EXT_DOMAIN_MEMORY` | `memory_provider.h`（atoms） | 记忆域 provider（首个接入域，验证范式） |
| `AIRY_EXT_DOMAIN_LLM` | `airy_llm_provider.h` | LLM 域（complete / complete_stream / stats） |
| `AIRY_EXT_DOMAIN_TOOL` | `airy_tool_provider.h` | 工具域（list / execute / execute_stream） |
| `AIRY_EXT_DOMAIN_STORAGE` | `airy_storage_provider.h` | 存储域（get / set / delete / list） |
| `AIRY_EXT_DOMAIN_SANDBOX` | `airy_sandbox_provider.h` | 沙箱域（能力描述） |

## 设计约束

- 注册表仅持有**通用扩展头**（domain/name/version/capabilities/vtable/impl），
  不感知各域 vtable 内部结构（域头各自定义）。
- 注册发生在启动/配置阶段；查询（get/count/foreach）可在运行期并发，注册表
  内部以互斥锁保护。
- name/version 注册时**深拷贝**，调用方传入字符串可复用栈/字面量。
- `foreach` 回调在注册表锁内执行，回调中**禁止**再调用本模块注册/注销 API。

## API 一览

| 函数 | 说明 |
|------|------|
| `airy_ext_register(&ext)` | 注册（或按 (domain,name) 覆盖）扩展 |
| `airy_ext_unregister(domain, name)` | 注销扩展 |
| `airy_ext_get(domain, name)` | 按 key 查找（只读指针，注销后失效） |
| `airy_ext_count(domain)` | 域内已注册数量 |
| `airy_ext_foreach(domain, fn, ud)` | 域内遍历（锁内回调） |
| `airy_ext_clear(domain)` | 清空域内全部扩展 |
| `airy_ext_domain_name(domain)` | 域名（日志/诊断） |

能力标记：`AIRY_EXT_FLAG_BUILTIN`（内置实现）/ `REMOTE`（远程/RPC 后端）/
`OPTIONAL`（可选扩展，缺失不影响运行）。

## 状态

- **实现**：`src/airy_ext.c` + `src/airy_providers.c` + 5 个域 vtable 契约头。
- **测试**：commons 单元测试覆盖注册/覆盖/注销/查询/遍历/清空。
- **演进**：这是能力契约环（M5）的落地载体；后续在 daemon 契约中引入依赖宣言
  （M2）与服务索引（M3）后，扩展可实现运行时按需激活与替换（见内部文档
  `closed-docs/agentrt/07-subsystem-specs/13-module-boundaries.md`）。
