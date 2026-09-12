# Types — 类型定义模块

**模块路径**: `commons/utils/types/`
**版本**: 0.1.15

## 概述

Types 模块是 AgentRT 系统的核心类型定义中心，提供系统范围内使用的核心数据类型。该模块为 header-only 模块，不包含任何实现代码（`.c` 文件），所有类型均为值类型或不可变类型，天然线程安全。

`types.h` 按八个类别组织：基础类型、任务类型、记忆类型、会话类型、Agent 类型、可观测性类型、IPC 类型、网络类型，并附带通用辅助宏。目录内另有若干独立规范头，用于消除跨模块、跨仓库的类型重复定义冲突（输入净化级别、凭据类型、签名者信息，以及 LLM / 工具 / 安全护栏等服务边界类型）。

## 设计目标

- **集中定义**：核心类型统一在本模块定义，各消费方直接包含，避免本地重复定义引发的冲突
- **契约明确**：每个类型有清晰的语义与所有权规则
- **命名语义化**：类型名称精确表达其用途
- **边界类型下沉**：跨仓库边界（如 atoms 与各服务之间）传递的类型统一定义在 commons，两侧都只包含本模块头，不互相包含上层头文件

## 目录结构

模块目录为平铺结构（无 `include/`、`src/` 子目录）：

```
types/
├── types.h                     # 核心类型定义（八大类别 + 辅助宏）
├── sanitize_level.h            # 输入净化级别规范类型（STRICT / NORMAL / RELAXED）
├── cupolas_vault_cred_type.h   # 凭据类型规范类型（Password / Token / Key / Certificate / Secret / Note）
├── cupolas_signer_info.h       # 代码签名者信息规范类型（X.509 证书身份字段）
├── llm_service_types.h         # LLM 服务边界类型（消息 / 请求配置 / 响应 / 流式回调）
├── tool_service_types.h        # 工具服务边界类型（元数据 / 执行请求 / 结果分级）
├── tool_approval_types.h       # 工具审批边界类型（审批结果 / 配置 / 明细）
├── safety_guard_types.h        # SafetyGuard 边界类型（事件 / 决策 / 严重度）
└── README.md                   # 本文档
```

## 核心数据结构（types.h）

### 基础类型

#### airy_result_t — 通用结果类型

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | `airy_err_t` | 错误码 |
| `message` | `const char *` | 错误消息 |
| `detail` | `const char *` | 详细信息 |

#### airy_priority_t — 优先级枚举

| 枚举值 | 说明 |
|------|------|
| `AIRY_PRIORITY_LOW` | 低优先级 |
| `AIRY_PRIORITY_NORMAL` | 普通优先级 |
| `AIRY_PRIORITY_HIGH` | 高优先级 |
| `AIRY_PRIORITY_CRITICAL` | 关键优先级 |

#### 基础类型别名

| 类型 | 定义 | 说明 |
|------|------|------|
| `airy_err_t` | `__s32`（共享契约头定义） | 错误码类型（负值为错误，0 为成功），见下文错误码说明 |
| `airy_timestamp_t` | `uint64_t` | 时间戳类型（纳秒精度） |
| `airy_millis_t` | `uint64_t` | 毫秒时间类型 |
| `airy_uuid_t` | `char[37]` | 唯一标识符类型 |

#### 错误码

错误码类型 `airy_err_t` 与 `AIRY_E*` 契约错误码宏**不再在本模块定义**，其权威位置为 commons 的共享契约头 `include/airymax/error.h`（经 `airy_types.h` 引入）：宏为正幅值，跨边界函数返回其负值形式（`-AIRY_E*`）。用户态扩展码 `AIRY_ERR_*` 定义于 `commons/utils/error/error_codes.h`，错误处理运行时见 [utils/error](../error/README.md)。

本模块仅保留成功常量 `AIRY_SUCCESS`（值为 0）。

### 任务类型

#### airy_task_status_t — 任务状态

| 枚举值 | 说明 |
|------|------|
| `AIRY_TASK_PENDING` | 等待中 |
| `AIRY_TASK_RUNNING` | 运行中 |
| `AIRY_TASK_SUCCEEDED` | 已成功 |
| `AIRY_TASK_FAILED` | 已失败 |
| `AIRY_TASK_CANCELLED` | 已取消 |
| `AIRY_TASK_TIMEOUT` | 已超时 |
| `AIRY_TASK_RETRYING` | 重试中 |

（同时提供 `TASK_STATUS_*` 无前缀别名。）

#### airy_task_type_t — 任务类型

| 枚举值 | 说明 |
|------|------|
| `AIRY_TASKTYPE_ONESHOT` | 一次性任务 |
| `AIRY_TASKTYPE_RECURRING` | 周期任务 |
| `AIRY_TASKTYPE_CONDITIONAL` | 条件任务 |

#### airy_task_config_t — 任务配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `input` | `const char *` | 任务输入（自然语言描述） |
| `input_len` | `size_t` | 输入长度 |
| `timeout_ms` | `uint32_t` | 超时时间（毫秒） |
| `priority` | `airy_priority_t` | 任务优先级 |
| `type` | `airy_task_type_t` | 任务类型 |
| `agent_id` | `const char *` | 指定执行的 Agent ID（可选） |
| `session_id` | `const char *` | 关联的会话 ID（可选） |
| `parent_task_id` | `const char *` | 父任务 ID（用于子任务） |

#### airy_task_result_t — 任务结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `task_id` | `char *` | 任务 ID |
| `status` | `airy_task_status_t` | 任务状态 |
| `output` | `char *` | 输出结果（JSON 字符串） |
| `output_len` | `size_t` | 输出长度 |
| `start_time` | `airy_timestamp_t` | 开始时间 |
| `end_time` | `airy_timestamp_t` | 结束时间 |
| `tokens_used` | `uint32_t` | 消耗的 Token 数 |
| `cost_usd` | `double` | 成本（美元） |
| `error_code` | `airy_err_t` | 错误码 |
| `error_message` | `char *` | 错误消息 |

### 记忆类型

#### airy_memory_layer_t — 四层记忆层级

| 枚举值 | 说明 |
|------|------|
| `AIRY_MEM_LAYER1_RAW` | Layer1: 原始记忆 |
| `AIRY_MEM_LAYER2_WORKING` | Layer2: 工作记忆 |
| `AIRY_MEM_LAYER3_EPISODIC` | Layer3: 情景记忆 |
| `AIRY_MEM_LAYER4_SEMANTIC` | Layer4: 语义记忆 |

#### airy_memory_type_t — 记忆类型

| 枚举值 | 说明 |
|------|------|
| `AIRY_MEMTYPE_TEXT` | 文本 |
| `AIRY_MEMTYPE_EMBEDDING` | 向量嵌入 |
| `AIRY_MEMTYPE_STRUCTURED` | 结构化数据 |
| `AIRY_MEMTYPE_BINARY` | 二进制 |

#### airy_memory_entry_t — 记忆条目

| 字段 | 类型 | 说明 |
|------|------|------|
| `memory_id` | `char *` | 记忆 ID |
| `layer` | `airy_memory_layer_t` | 记忆层级 |
| `type` | `airy_memory_type_t` | 记忆类型 |
| `content` | `char *` | 记忆内容 |
| `content_len` | `size_t` | 内容长度 |
| `embedding` | `float *` | 向量嵌入（可选） |
| `embedding_dim` | `size_t` | 嵌入维度 |
| `importance` | `float` | 重要性分数（0-1） |
| `decay_rate` | `float` | 衰减率 |
| `access_count` | `uint32_t` | 访问次数 |
| `created_at` | `airy_timestamp_t` | 创建时间 |
| `last_access` | `airy_timestamp_t` | 最后访问时间 |
| `session_id` | `char *` | 关联会话 ID |
| `task_id` | `char *` | 关联任务 ID |
| `tags` | `char **` | 标签列表 |
| `tag_count` | `size_t` | 标签数量 |

此外提供 `airy_memory_search_t`（检索配置：query、layer、top_k、threshold、tags）与 `airy_memory_result_t`（检索结果：entries、count、scores）。

### 会话类型

#### airy_session_status_t — 会话状态

| 枚举值 | 说明 |
|------|------|
| `AIRY_SESSION_ACTIVE` | 活跃状态 |
| `AIRY_SESSION_IDLE` | 空闲状态 |
| `AIRY_SESSION_CLOSED` | 已关闭 |
| `AIRY_SESSION_EXPIRED` | 已过期 |

#### airy_context_t — 执行上下文

| 字段 | 类型 | 说明 |
|------|------|------|
| `agent_id` | `char *` | Agent ID |
| `session_id` | `char *` | 会话 ID |
| `trace_id` | `char *` | 追踪 ID |
| `parent_span_id` | `char *` | 父 Span ID |
| `timestamp` | `airy_timestamp_t` | 时间戳 |
| `priority` | `airy_priority_t` | 优先级 |
| `user_id` | `char *` | 用户 ID |
| `project_id` | `char *` | 项目 ID |
| `user_data` | `void *` | 用户自定义数据 |

此外提供 `airy_session_config_t`（会话配置）与 `airy_session_info_t`（会话统计信息：任务数、记忆数、token 用量、成本等）。

### Agent 类型

#### airy_agent_contract_t — Agent 契约（完整元数据）

| 字段 | 类型 | 说明 |
|------|------|------|
| `schema_version` | `char *` | 契约版本 |
| `agent_id` | `char *` | Agent ID |
| `agent_name` | `char *` | Agent 名称 |
| `version` | `char *` | Agent 版本 |
| `role` | `char *` | 角色分类 |
| `description` | `char *` | 描述 |
| `capabilities` | `airy_capability_t *` | 能力列表 |
| `capability_count` | `size_t` | 能力数量 |
| `models` | `airy_models_t` | 模型配置 |
| `required_permissions` | `char **` | 所需权限 |
| `permission_count` | `size_t` | 权限数量 |
| `cost` | `airy_cost_profile_t` | 成本概览 |
| `trust` | `airy_trust_metrics_t` | 信任指标 |
| `extensions` | `char *` | 扩展字段（JSON） |

配套类型：`airy_agent_level_t`（COMMUNITY / VERIFIED / OFFICIAL）、`airy_capability_t`（能力声明）、`airy_cost_profile_t`、`airy_trust_metrics_t`。

### 可观测性类型

#### airy_metric_t — 指标数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | `char *` | 指标名称 |
| `type` | `airy_metric_type_t` | 指标类型（COUNTER / GAUGE / HISTOGRAM / SUMMARY） |
| `description` | `char *` | 描述 |
| `unit` | `char *` | 单位 |
| `value` | `double` | 当前值 |
| `labels` | `char **` | 标签键值对 |
| `label_count` | `size_t` | 标签数量 |
| `timestamp` | `airy_timestamp_t` | 时间戳 |

#### airy_span_t — Span 数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `trace_id` | `char *` | 追踪 ID |
| `span_id` | `char *` | Span ID |
| `parent_span_id` | `char *` | 父 Span ID |
| `name` | `char *` | Span 名称 |
| `kind` | `airy_span_kind_t` | Span 类型（INTERNAL / CLIENT / SERVER / PRODUCER / CONSUMER） |
| `start_time` | `airy_timestamp_t` | 开始时间 |
| `end_time` | `airy_timestamp_t` | 结束时间 |
| `status` | `airy_span_status_t` | Span 状态（UNSET / OK / ERROR） |
| `status_message` | `char *` | 状态消息 |
| `attributes` | `char **` | 属性键值对 |
| `attribute_count` | `size_t` | 属性数量 |
| `events` | `char *` | 事件列表（JSON） |

此外提供 `airy_telemetry_t`（指标 + Span + 日志聚合）。日志级别枚举统一使用共享契约头 `include/airymax/log_types.h` 的 `airy_log_level`（DEBUG..FATAL 五级），本模块不再重复定义。

### IPC 类型

#### airy_ipc_type_t — IPC 通道类型

| 枚举值 | 说明 |
|------|------|
| `AIRY_IPC_PIPE` | 管道 |
| `AIRY_IPC_SOCKET` | Unix Socket / Named Pipe |
| `AIRY_IPC_SHM` | 共享内存 |
| `AIRY_IPC_MQ` | 消息队列 |
| `AIRY_IPC_RPC` | RPC 调用 |

同时提供 `airy_ipc_flag_t`（NONE / NONBLOCK / PRIORITY / BROADCAST）与 `airy_ipc_config_t`（通道配置）。

### 网络类型

#### airy_protocol_t — 协议类型

| 枚举值 | 说明 |
|------|------|
| `AIRY_PROTO_TCP` | TCP 协议 |
| `AIRY_PROTO_UDP` | UDP 协议 |
| `AIRY_PROTO_HTTP` | HTTP 协议 |
| `AIRY_PROTO_HTTPS` | HTTPS 协议 |
| `AIRY_PROTO_WS` | WebSocket 协议 |
| `AIRY_PROTO_WSS` | WebSocket Secure 协议 |

#### airy_endpoint_t — 连接端点

| 字段 | 类型 | 说明 |
|------|------|------|
| `host` | `char *` | 主机名或 IP |
| `port` | `uint16_t` | 端口号 |
| `protocol` | `airy_protocol_t` | 协议类型 |
| `path` | `char *` | 路径（用于 HTTP/WebSocket） |

此外提供 `airy_conn_state_t`（连接状态机）、`airy_conn_config_t`（超时、重试、TLS 配置）、`airy_http_request_t` / `airy_http_response_t`。

## 跨模块规范类型（独立头文件）

### sanitize_level.h — 输入净化级别

| 枚举值 | 说明 |
|------|------|
| `SANITIZE_LEVEL_STRICT` | 最大净化，拒绝任何可疑内容 |
| `SANITIZE_LEVEL_NORMAL` | 平衡净化，适用于典型 Agent 交互 |
| `SANITIZE_LEVEL_RELAXED` | 最小净化，适用于受信任的内部通道 |

### cupolas_vault_cred_type.h — 凭据类型

| 枚举值 | 值 | 说明 |
|------|------|------|
| `CUPOLAS_VAULT_CRED_PASSWORD` | 1 | 密码 |
| `CUPOLAS_VAULT_CRED_TOKEN` | 2 | 令牌（API Key、OAuth Token） |
| `CUPOLAS_VAULT_CRED_KEY` | 3 | 密钥（私钥） |
| `CUPOLAS_VAULT_CRED_CERTIFICATE` | 4 | 证书 |
| `CUPOLAS_VAULT_CRED_SECRET` | 5 | 通用密钥 |
| `CUPOLAS_VAULT_CRED_NOTE` | 6 | 安全笔记 |

### cupolas_signer_info.h — 代码签名者信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `subject_cn` | `char *` | 主体通用名称 |
| `subject_org` | `char *` | 主体组织 |
| `subject_ou` | `char *` | 主体组织单元 |
| `issuer_cn` | `char *` | 签发者通用名称 |
| `serial_number` | `char *` | 序列号 |
| `key_id` | `char *` | 密钥标识 |
| `algorithm` | `char *` | 算法 |
| `not_before` | `uint64_t` | 有效期起始 |
| `not_after` | `uint64_t` | 有效期截止 |
| `is_ca` | `bool` | 是否为 CA |
| `key_usage` | `uint32_t` | 密钥用途 |

## 服务边界类型（独立头文件）

以下头文件定义跨模块边界传递的最小类型集合，边界两侧（调用方与服务实现方）均只包含本处头文件，服务生命周期入口仍留在各服务的实现仓库中。

### llm_service_types.h — LLM 服务边界

| 类型 | 说明 |
|------|------|
| `llm_message_t` | 对话消息（role/content，含推理轨迹 `reasoning_content` 与函数调用 `tool_call_id` / `tool_calls_json` 字段） |
| `llm_request_config_t` | 请求配置（model、messages、temperature、top_p、max_tokens、stream、stop、penalty、tools JSON） |
| `llm_response_t` | 响应（choices、token 用量含 reasoning_tokens、cost_usd、finish_reason） |
| `llm_stream_callback_t` | 流式输出回调 |

### tool_service_types.h — 工具服务边界

| 类型 | 说明 |
|------|------|
| `tool_access_t` | 工具访问类型（READ 可并行 / WRITE 串行互斥；零值默认 WRITE） |
| `tool_param_t` | 参数定义（JSON Schema 字符串 + 是否必填） |
| `tool_metadata_t` | 工具元数据（id、name、executable、params、timeout、cacheable、access、permission_rule） |
| `tool_execute_request_t` | 执行请求（tool_id、params_json、stream、agent_id） |
| `tool_result_class_t` | 失败分级（SUCCESS / FATAL / RESPOND_TO_MODEL / NORMAL_FAIL） |
| `tool_result_t` | 执行结果（success、output、error、exit_code、duration_ms、failure_class） |
| `tool_stream_callback_t` | 流式输出回调（区分 stdout / stderr） |

### tool_approval_types.h — 工具审批边界

| 类型 | 说明 |
|------|------|
| `tool_approval_result_t` | 审批结果（ALLOWED / DENY / SANITIZED / PENDING_AUDIT） |
| `tool_approval_config_t` | 审批配置（agent_id、护栏链接开关、审计开关、权限规则） |
| `tool_approval_detail_t` | 审批明细（决策、原因、净化后参数、各检查环节通过状态） |

### safety_guard_types.h — SafetyGuard 边界

| 类型 | 说明 |
|------|------|
| `safety_event_type_t` | 安全事件类型（访问请求、资源分配、数据流、执行起止、策略变更、配额超限、违规检测、紧急停止） |
| `safety_decision_t` | 决策结果（ALLOW / DENY / CONDITIONAL / DEFER / ABORT） |
| `safety_severity_t` | 严重度（INFO / WARNING / ERROR / CRITICAL / FATAL） |
| `safety_event_t` | 事件载荷（类型、subject/action/resource、上下文、时间戳、标志） |
| `safety_result_t` | 决策载荷（决策、原因、严重度、条件、修改后上下文） |

## 辅助宏定义

| 宏 | 说明 |
|------|------|
| `AIRY_ARRAY_SIZE(arr)` | 数组元素数量计算 |
| `AIRY_MIN(a, b)` | 取最小值 |
| `AIRY_MAX(a, b)` | 取最大值 |
| `AIRY_ALIGN_UP(x, align)` | 向上对齐 |
| `AIRY_STRINGIFY(x)` / `AIRY_TOSTRING(x)` | 字符串化 |
| `AIRY_CONCAT(a, b)` / `AIRY_CONCAT3(a, b, c)` | 符号连接 |
| `AIRY_MAKE_VERSION(maj, min, pat)` | 版本号打包 |
| `AIRY_VERSION_MAJOR(v)` | 提取主版本号 |
| `AIRY_VERSION_MINOR(v)` | 提取次版本号 |
| `AIRY_VERSION_PATCH(v)` | 提取补丁版本号 |
| `AIRY_MS_TO_NS(ms)` | 毫秒转纳秒 |
| `AIRY_SEC_TO_MS(s)` | 秒转毫秒 |
| `AIRY_SEC_TO_NS(s)` | 秒转纳秒 |

## 使用示例

```c
#include "types.h"

/* === 使用通用结果类型 === */
airy_result_t result = { .code = AIRY_SUCCESS, .message = "OK" };
if (result.code == AIRY_SUCCESS) {
    /* result.message 携带描述文本，交由上层日志出口输出 */
}

/* === 定义任务配置 === */
airy_task_config_t config = {
    .input = "Analyze the codebase",
    .input_len = 20,
    .timeout_ms = 30000,
    .priority = AIRY_PRIORITY_HIGH,
    .type = AIRY_TASKTYPE_ONESHOT,
    .agent_id = NULL,  /* 自动选择 */
};

/* === 使用执行上下文 === */
airy_context_t ctx = {
    .agent_id = "agent-001",
    .session_id = "session-abc",
    .trace_id = "tr-0123456789abcdef",
    .priority = AIRY_PRIORITY_NORMAL,
};

/* === 使用辅助宏 === */
int items[] = {1, 2, 3, 4, 5};
size_t count = AIRY_ARRAY_SIZE(items);  /* 5 */

uint32_t version = AIRY_MAKE_VERSION(1, 2, 3);
/* AIRY_VERSION_MAJOR(version) == 1，MINOR == 2，PATCH == 3 */

/* === 使用规范类型 === */
#include "sanitize_level.h"
sanitize_level_t level = SANITIZE_LEVEL_NORMAL;
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `airy_types.h`（commons/include/） | 引入共享契约类型（含 `airy_err_t` 与 `AIRY_E*` 错误码） |
| `platform.h`（commons/platform/include/） | 平台类型定义（socket 句柄等） |
| `stdbool.h` / `stddef.h` / `stdint.h` | C 标准库类型 |

服务边界头（`llm_service_types.h`、`tool_service_types.h`、`safety_guard_types.h`、`tool_approval_types.h`）仅依赖 C 标准库，可独立包含。

## 构建与链接

本模块为 header-only，`commons/utils/types/` 目录已作为公有 include 路径随 `airy_common` 目标导出，直接包含对应头文件即可，无需单独编译目标。

---

© 2025-2026 SPHARX Ltd.

SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0（双许可，任选其一遵守，完整文本见仓库根 [LICENSE](../../LICENSE)）
