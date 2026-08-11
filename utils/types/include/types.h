/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file types.h
 * @brief AgentRT 统一类型定义 - 核心基础类型
 *
 * @details
 * 本文件定义了 AgentRT 系统范围内使用的所有核心数据类型。
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的设计原则：
 * - K-2 接口契约化：所有类型都有明确的语义和所有权规则
 * - E-5 命名语义化：类型名称精确表达其用途
 *
 * 类型分类：
 * 1. 基础类型：错误码、状态枚举、结果类型
 * 2. 任务类型：任务状态、优先级、结果
 * 3. 记忆类型：记忆层级、存储结构
 * 4. 会话类型：会话状态、上下文
 * 5. Agent类型：Agent契约、能力定义
 * 6. 可观测性类型：指标、追踪、日志
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-04-03
 * @version 0.1.0
 *
 * @note 线程安全：本文件定义的类型均为值类型或不可变类型，线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md K-2 接口契约化原则
 * @see syscall_api_contract.md 系统调用 API 契约
 * @see agent_contract.md Agent 契约规范
 */

#ifndef AIRY_RT_TYPES_H
#define AIRY_RT_TYPES_H

#include "../../../include/airy_types.h"
#include "../../../platform/include/platform.h"
#include "../../error/include/error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 第一部分：基础类型定义
 * ============================================================================ */

/**
 * @defgroup BasicTypes 基础类型
 * @brief 系统范围内使用的基础数据类型
 * @{
 */

/**
 * @brief 错误码类型
 * @details 所有错误码为负值，成功为0。遵循 syscall_api_contract.md 规范。
 */
typedef int32_t airy_err_t;

/**
 * @brief 成功返回值
 */
#define AIRY_SUCCESS 0

/**
 * @brief 通用错误码定义
 *
 * v4.0 SSoT 修复：所有与 airy_types.h 重复的宏均以 #ifndef 保护，确保
 * airy_types.h（本文件 line 36 已先行 include）的 POSIX 权威值优先生效。
 * types.h 原先的无条件 #define 覆盖了 airy_types.h 的 POSIX 值（如将
 * AIRY_EINVAL 从 -22 覆盖为 -1），破坏 v3.0 SSoT 统一收敛。现通过 #ifndef
 * 让权威源生效；仅 AIRY_ENOTFOUND 为 airy_types.h 未定义的补充宏。
 */
#ifndef AIRY_EINVAL
#define AIRY_EINVAL (-1)
#endif
#ifndef AIRY_ENOMEM
#define AIRY_ENOMEM (-2)
#endif
#ifndef AIRY_EBUSY
#define AIRY_EBUSY (-3)
#endif
#ifndef AIRY_ENOENT
#define AIRY_ENOENT (-4)
#endif
#ifndef AIRY_EPERM
#define AIRY_EPERM (-5)
#endif
#ifndef AIRY_ETIMEDOUT
#define AIRY_ETIMEDOUT (-6)
#endif
#ifndef AIRY_EIO
#define AIRY_EIO (-7)
#endif
#ifndef AIRY_EEXIST
#define AIRY_EEXIST (-8)
#endif
#ifndef AIRY_ENOTINIT
#define AIRY_ENOTINIT (-9)
#endif
#ifndef AIRY_ECANCELLED
#define AIRY_ECANCELLED (-10)
#endif
#ifndef AIRY_ENOTSUP
#define AIRY_ENOTSUP (-11)
#endif
#ifndef AIRY_EOVERFLOW
#define AIRY_EOVERFLOW (-12)
#endif
#ifndef AIRY_EPROTO
#define AIRY_EPROTO (-13)
#endif
#ifndef AIRY_ENOTCONN
#define AIRY_ENOTCONN (-14)
#endif
#ifndef AIRY_ECONNRESET
#define AIRY_ECONNRESET (-15)
#endif
#ifndef AIRY_ENOSYS
#define AIRY_ENOSYS (-16)
#endif
#ifndef AIRY_EFAIL
#define AIRY_EFAIL (-17)
#endif
#ifndef AIRY_ENOTFOUND
#define AIRY_ENOTFOUND (-18)
#endif
#ifndef AIRY_EPLATFORM
#define AIRY_EPLATFORM (-27)
#endif
#ifndef AIRY_EPROTONOSUPPORT
#define AIRY_EPROTONOSUPPORT (-28)
#endif
#ifndef AIRY_ESERVICE
#define AIRY_ESERVICE (-29)
#endif
#ifndef AIRY_EUNKNOWN
#define AIRY_EUNKNOWN (-99)
#endif

/**
 * @brief 时间戳类型（纳秒）
 * @details 使用 Unix 时间戳，纳秒精度
 */
typedef uint64_t airy_timestamp_t;

/**
 * @brief 毫秒时间类型
 */
typedef uint64_t airy_millis_t;

/**
 * @brief 唯一标识符类型
 * @details 用于 task_id, session_id, agent_id 等标识符
 */
typedef char airy_uuid_t[37];

/**
 * @brief 优先级枚举
 */
typedef enum {
    AIRY_PRIORITY_LOW = 0,
    AIRY_PRIORITY_NORMAL = 1,
    AIRY_PRIORITY_HIGH = 2,
    AIRY_PRIORITY_CRITICAL = 3
} airy_priority_t;

/**
 * @brief 通用结果类型
 * @details 用于返回操作结果和错误信息
 */
typedef struct {
    airy_err_t code;
    const char *message;
    const char *detail;
} airy_result_t;

/** @} */ /* end of BasicTypes */
/* ============================================================================
 * 第二部分：任务类型定义
 * ============================================================================ */

/**
 * @defgroup TaskTypes 任务类型
 * @brief 任务管理相关的数据类型
 * @{
 */

/**
 * @brief 任务状态枚举
 * @details 定义任务的生命周期状态
 */
#ifndef AIRY_TASK_STATUS_T_DEFINED
#define AIRY_TASK_STATUS_T_DEFINED
typedef enum {
    AIRY_TASK_PENDING = 0,
    AIRY_TASK_RUNNING = 1,
    AIRY_TASK_SUCCEEDED = 2,
    AIRY_TASK_FAILED = 3,
    AIRY_TASK_CANCELLED = 4,
    AIRY_TASK_TIMEOUT = 5,
    AIRY_TASK_RETRYING = 6
} airy_task_status_t;


#define TASK_STATUS_PENDING AIRY_TASK_PENDING
#define TASK_STATUS_RUNNING AIRY_TASK_RUNNING
#define TASK_STATUS_SUCCEEDED AIRY_TASK_SUCCEEDED
#define TASK_STATUS_FAILED AIRY_TASK_FAILED
#define TASK_STATUS_CANCELLED AIRY_TASK_CANCELLED
#define TASK_STATUS_TIMEOUT AIRY_TASK_TIMEOUT
#define TASK_STATUS_RETRYING AIRY_TASK_RETRYING
#endif

/**
 * @brief 任务类型枚举
 */
typedef enum {
    AIRY_TASKTYPE_ONESHOT = 0,
    AIRY_TASKTYPE_RECURRING = 1,
    AIRY_TASKTYPE_CONDITIONAL = 2
} airy_task_type_t;

/**
 * @brief 任务句柄类型
 * @details 用于引用任务实例
 */
#ifndef AIRY_TASK_T_DEFINED
#define AIRY_TASK_T_DEFINED
typedef struct airy_task airy_task_t;
#endif

/**
 * @brief 任务配置结构
 */
typedef struct {
    const char *input;
    size_t input_len;
    uint32_t timeout_ms;
    airy_priority_t priority;
    airy_task_type_t type;
    const char *agent_id;
    const char *session_id;
    const char *parent_task_id;
} airy_task_config_t;

/**
 * @brief 任务结果结构
 */
typedef struct {
    char *task_id;
    airy_task_status_t status;
    char *output;
    size_t output_len;
    airy_timestamp_t start_time;
    airy_timestamp_t end_time;
    uint32_t tokens_used;
    double cost_usd;
    airy_err_t error_code;
    char *error_message;
} airy_task_result_t;

/** @} */ /* end of TaskTypes */
/* ============================================================================
 * 第三部分：记忆类型定义
 * ============================================================================ */

/**
 * @defgroup MemoryTypes 记忆类型
 * @brief 记忆管理相关的数据类型
 * @{
 */

/**
 * @brief 记忆层级枚举
 * @details 四层记忆卷载结构
 */
typedef enum {
    AIRY_MEM_LAYER1_RAW = 0,
    AIRY_MEM_LAYER2_WORKING = 1,
    AIRY_MEM_LAYER3_EPISODIC = 2,
    AIRY_MEM_LAYER4_SEMANTIC = 3
} airy_memory_layer_t;

/**
 * @brief 记忆类型枚举
 */
#ifndef AIRY_MEMORY_TYPE_T_DEFINED
#define AIRY_MEMORY_TYPE_T_DEFINED
typedef enum {
    AIRY_MEMTYPE_TEXT = 0,
    AIRY_MEMTYPE_EMBEDDING = 1,
    AIRY_MEMTYPE_STRUCTURED = 2,
    AIRY_MEMTYPE_BINARY = 3
} airy_memory_type_t;
#endif

/**
 * @brief 记忆句柄类型 - forward declaration (defined in memory_provider.h)
 */
struct airy_memory;
typedef struct airy_memory airy_memory_t;

/**
 * @brief 记忆条目结构
 */
typedef struct {
    char *memory_id;
    airy_memory_layer_t layer;
    airy_memory_type_t type;
    char *content;
    size_t content_len;
    float *embedding;
    size_t embedding_dim;
    float importance;
    float decay_rate;
    uint32_t access_count;
    airy_timestamp_t created_at;
    airy_timestamp_t last_access;
    char *session_id;
    char *task_id;
    char **tags;
    size_t tag_count;
} airy_memory_entry_t;

/**
 * @brief 记忆搜索配置
 */
typedef struct {
    const char *query;
    size_t query_len;
    airy_memory_layer_t layer;
    uint32_t top_k;
    float threshold;
    const char **tags;
    size_t tag_count;
} airy_memory_search_t;

/**
 * @brief 记忆搜索结果
 */
#ifndef AIRY_MEMORY_RESULT_T_DEFINED
#define AIRY_MEMORY_RESULT_T_DEFINED
typedef struct {
    airy_memory_entry_t *entries;
    size_t count;
    float *scores;
} airy_memory_result_t;
#endif

/** @} */ /* end of MemoryTypes */
/* ============================================================================
 * 第四部分：会话类型定义
 * ============================================================================ */

/**
 * @defgroup SessionTypes 会话类型
 * @brief 会话管理相关的数据类型
 * @{
 */

/**
 * @brief 会话状态枚举
 */
typedef enum {
    AIRY_SESSION_ACTIVE = 0,
    AIRY_SESSION_IDLE = 1,
    AIRY_SESSION_CLOSED = 2,
    AIRY_SESSION_EXPIRED = 3
} airy_session_status_t;

/**
 * @brief 会话句柄类型
 */
typedef struct airy_session *airy_session_t;

/**
 * @brief 会话配置结构
 */
typedef struct {
    const char *user_id;
    const char *project_id;
    const char *context;
    uint32_t ttl_seconds;
    airy_priority_t priority;
} airy_session_config_t;

/**
 * @brief 会话信息结构
 */
typedef struct {
    char *session_id;
    airy_session_status_t status;
    char *user_id;
    char *project_id;
    airy_timestamp_t created_at;
    airy_timestamp_t last_active;
    uint32_t ttl_seconds; /**< TTL */
    uint32_t task_count;
    uint32_t memory_count;
    uint64_t tokens_used;
    double cost_usd;
} airy_session_info_t;

/**
 * @brief 执行上下文结构
 * @details 用于传递请求链路的上下文信息
 */
typedef struct {
    char *agent_id; /**< Agent ID */
    char *session_id;
    char *trace_id;
    char *parent_span_id;
    airy_timestamp_t timestamp;
    airy_priority_t priority;
    char *user_id;
    char *project_id;
    void *user_data;
} airy_context_t;

/** @} */ /* end of SessionTypes */
/* ============================================================================
 * 第五部分：Agent 类型定义
 * ============================================================================ */

/**
 * @defgroup AgentTypes Agent 类型
 * @brief Agent 契约和能力相关的数据类型
 * @{
 */

/**
 * @brief Agent 维护级别枚举
 */
typedef enum {
    AIRY_AGENT_COMMUNITY = 0,
    AIRY_AGENT_VERIFIED = 1,
    AIRY_AGENT_OFFICIAL = 2
} airy_agent_level_t;

#ifndef AIRY_CAPABILITY_T_DEFINED
#define AIRY_CAPABILITY_T_DEFINED
/**
 * @brief Agent 能力结构
 */
typedef struct {
    char *name;
    char *description;
    char *input_schema;
    char *output_schema;
    uint32_t estimated_tokens;
    uint32_t avg_duration_ms;
    float success_rate;
} airy_capability_t;
#endif

/**
 * @brief Agent 模型配置
 */
typedef struct {
    char *system1;
    char *system2;
} airy_models_t;

/**
 * @brief Agent 成本概览
 */
typedef struct {
    uint32_t token_per_task_avg;
    double api_cost_per_task;
    airy_agent_level_t level;
} airy_cost_profile_t;

/**
 * @brief Agent 信任指标
 */
typedef struct {
    uint32_t install_count;
    float rating;
    bool verified_provider;
    char *last_audit;
} airy_trust_metrics_t;

/**
 * @brief Agent 契约结构
 * @details 完整的 Agent 元数据定义
 */
typedef struct {
    char *schema_version;
    char *agent_id; /**< Agent ID */
    char *agent_name;
    char *version;
    char *role;
    char *description;
    airy_capability_t *capabilities;
    size_t capability_count;
    airy_models_t models;
    char **required_permissions;
    size_t permission_count;
    airy_cost_profile_t cost;
    airy_trust_metrics_t trust;
    char *extensions;
} airy_agent_contract_t;

/** @} */ /* end of AgentTypes */
/* ============================================================================
 * 第六部分：可观测性类型定义
 * ============================================================================ */

/**
 * @defgroup ObservabilityTypes 可观测性类型
 * @brief 指标、追踪、日志相关的数据类型
 * @{
 */

/**
 * @brief 日志级别枚举
 */
#ifndef AIRY_LOG_LEVEL_T_DEFINED
#define AIRY_LOG_LEVEL_T_DEFINED
typedef enum {
    AIRY_LOG_LEVEL_DEBUG_E = 0,
    AIRY_LOG_LEVEL_INFO_E = 1,
    AIRY_LOG_LEVEL_WARN_E = 2,
    AIRY_LOG_LEVEL_ERROR_E = 3,
    AIRY_LOG_LEVEL_FATAL_E = 4
} airy_log_level_t;
#endif

/**
 * @brief 指标类型枚举
 */
#ifndef AIRY_METRIC_TYPE_T_DEFINED
#define AIRY_METRIC_TYPE_T_DEFINED
typedef enum {
    AIRY_METRIC_COUNTER_E = 0,
    AIRY_METRIC_GAUGE_E = 1,
    AIRY_METRIC_HISTOGRAM_E = 2,
    AIRY_METRIC_SUMMARY_E = 3
} airy_metric_type_t;
#endif

/**
 * @brief Span 类型枚举
 */
typedef enum {
    AIRY_SPAN_INTERNAL = 0,
    AIRY_SPAN_CLIENT = 1,
    AIRY_SPAN_SERVER = 2,
    AIRY_SPAN_PRODUCER = 3,
    AIRY_SPAN_CONSUMER = 4
} airy_span_kind_t;

/**
 * @brief Span 状态枚举
 */
typedef enum { AIRY_SPAN_UNSET = 0, AIRY_SPAN_OK = 1, AIRY_SPAN_ERROR = 2 } airy_span_status_t;

/**
 * @brief 指标数据结构
 */
typedef struct {
    char *name;
    airy_metric_type_t type;
    char *description;
    char *unit;
    double value;
    char **labels;
    size_t label_count;
    airy_timestamp_t timestamp;
} airy_metric_t;

/**
 * @brief Span 数据结构
 */
typedef struct {
    char *trace_id;
    char *span_id; /**< Span ID */
    char *parent_span_id;
    char *name;
    airy_span_kind_t kind;
    airy_timestamp_t start_time;
    airy_timestamp_t end_time;
    airy_span_status_t status;
    char *status_message;
    char **attributes;
    size_t attribute_count;
    char *events;
} airy_span_t;

/**
 * @brief 遥测数据结构
 */
typedef struct {
    airy_metric_t *metrics;
    size_t metric_count;
    airy_span_t *spans;
    size_t span_count;
    char *logs;
} airy_telemetry_t;

/** @} */ /* end of ObservabilityTypes */
/* ============================================================================
 * 第七部分：IPC 类型定义
 * ============================================================================ */

/**
 * @defgroup IPCTypes IPC 类型
 * @brief 进程间通信相关的数据类型
 * @{
 */

/**
 * @brief IPC 通道类型枚举
 */
typedef enum {
    AIRY_IPC_PIPE = 0,
    AIRY_IPC_SOCKET = 1, /**< Unix Socket / Named Pipe */
    AIRY_IPC_SHM = 2,
    AIRY_IPC_MQ = 3,
    AIRY_IPC_RPC = 4
} airy_ipc_type_t;

/**
 * @brief IPC 消息标志
 */
typedef enum {
    AIRY_IPC_FLAG_NONE = 0,
    AIRY_IPC_FLAG_NONBLOCK = 1,
    AIRY_IPC_FLAG_PRIORITY = 2,
    AIRY_IPC_FLAG_BROADCAST = 4
} airy_ipc_flag_t;


/**
 * @brief IPC 通道句柄类型
 * @note 内核级IPC通道类型，完整定义见corekern/include/ipc.h
 *       应用层应使用commons/utils/ipc/include/ipc_common.h中的ipc_channel_t
 */

/**
 * @brief IPC 通道配置
 */
typedef struct {
    airy_ipc_type_t type;
    const char *name;
    uint32_t buffer_size;
    uint32_t max_message_size;
    uint32_t timeout_ms;
    bool nonblocking;
} airy_ipc_config_t;

/** @} */ /* end of IPCTypes */
/* ============================================================================
 * 第八部分：网络类型定义
 * ============================================================================ */

/**
 * @defgroup NetworkTypes 网络类型
 * @brief 网络通信相关的数据类型
 * @{
 */

/**
 * @brief 协议类型枚举
 */
typedef enum {
    AIRY_PROTO_TCP = 0,
    AIRY_PROTO_UDP = 1,
    AIRY_PROTO_HTTP = 2,
    AIRY_PROTO_HTTPS = 3,
    AIRY_PROTO_WS = 4,
    AIRY_PROTO_WSS = 5
} airy_protocol_t;

/**
 * @brief 连接状态枚举
 */
typedef enum {
    AIRY_CONN_DISCONNECTED = 0,
    AIRY_CONN_CONNECTING = 1,
    AIRY_CONN_CONNECTED = 2,
    AIRY_CONN_CLOSING = 3,
    AIRY_CONN_ERROR = 4
} airy_conn_state_t;

/**
 * @brief Socket 句柄类型
 * 定义在 platform.h 中
 */
/* typedef struct airy_socket* airy_sock_t; */

/**
 * @brief 连接端点结构
 */
typedef struct {
    char *host;
    uint16_t port;
    airy_protocol_t protocol;
    char *path;
} airy_endpoint_t;

/**
 * @brief 连接配置结构
 */
typedef struct {
    airy_endpoint_t remote;
    uint32_t timeout_ms;
    uint32_t read_timeout_ms;
    uint32_t write_timeout_ms;
    uint32_t max_retries;
    uint32_t retry_delay_ms;
    bool keepalive;
    bool verify_ssl;
    char *ssl_cert_path;
    char *ssl_key_path;
} airy_conn_config_t;

/**
 * @brief HTTP 请求结构
 */
typedef struct {
    const char *method;
    const char *path;
    const char **headers;
    size_t header_count;
    const void *body;
    size_t body_len;
    uint32_t timeout_ms;
} airy_http_request_t;

/**
 * @brief HTTP 响应结构
 */
typedef struct {
    int status_code;
    char **headers;
    size_t header_count;
    void *body;
    size_t body_len;
    airy_err_t error;
} airy_http_response_t;

/** @} */ /* end of NetworkTypes */
/* ============================================================================
 * 第九部分：辅助宏定义
 * ============================================================================ */

/**
 * @defgroup HelperMacros 辅助宏
 * @brief 常用的辅助宏定义
 * @{
 */

/**
 * @brief 数组元素数量计算
 */
#define AIRY_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief 最小值宏
 */
#define AIRY_MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief 最大值宏
 */
#define AIRY_MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief 对齐宏
 */
#define AIRY_ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/**
 * @brief 字符串化宏
 */
#define AIRY_STRINGIFY(x) #x
#define AIRY_TOSTRING(x) AIRY_STRINGIFY(x)

/**
 * @brief 连接宏
 */
#define AIRY_CONCAT(a, b) a##b
#define AIRY_CONCAT3(a, b, c) a##b##c

/**
 * @brief 版本号解析宏
 */
#ifndef AIRY_VERSION_MAJOR
#define AIRY_VERSION_MAJOR(v) (((v) >> 24) & 0xFF)
#endif
#ifndef AIRY_VERSION_MINOR
#define AIRY_VERSION_MINOR(v) (((v) >> 16) & 0xFF)
#endif
#ifndef AIRY_VERSION_PATCH
#define AIRY_VERSION_PATCH(v) (((v) >> 8) & 0xFF)
#endif
#ifndef AIRY_MAKE_VERSION
#define AIRY_MAKE_VERSION(maj, min, pat) (((maj) << 24) | ((min) << 16) | ((pat) << 8))
#endif

/**
 * @brief 时间转换宏
 */
#define AIRY_MS_TO_NS(ms) ((uint64_t)(ms) * 1000000ULL)
#define AIRY_SEC_TO_MS(s) ((uint64_t)(s) * 1000ULL)
#define AIRY_SEC_TO_NS(s) ((uint64_t)(s) * 1000000000ULL)

/** @} */ /* end of HelperMacros */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TYPES_H */
