/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file types.h
 * @brief AgentRT unified type definitions: core basic types.
 *
 * @details
 * This file defines all core data types used across the AgentRT system.
 * Follows the ARCHITECTURAL_PRINCIPLES.md design principles:
 * - K-2 interface contracts: every type has clear semantics and ownership
 *   rules
 * - E-5 semantic naming: type names precisely express their purpose
 *
 * Type categories:
 * 1. Basic types: error codes, state enums, result types
 * 2. Task types: task state, priority, results
 * 3. Memory types: memory layers, storage structures
 * 4. Session types: session state, context
 * 5. Agent types: agent contract, capability definitions
 * 6. Observability types: metrics, traces, logs
 *
 * @note Thread safety: the types defined here are value types or immutable
 *       types; thread-safe
 * @see ARCHITECTURAL_PRINCIPLES.md K-2 interface contract principle
 * @see syscall_api_contract.md syscall API contract
 * @see agent_contract.md agent contract specification
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

/*
 * Part 1: Basic type definitions
 */

/**
 * @defgroup BasicTypes Basic types
 * @brief Basic data types used system-wide
 * @{
 */

/**
 * @brief Error code type
 * @details S-1 收敛 (2026-08-14): airy_err_t 唯一权威为 [SC] airymax/error.h
 *          （typedef __s32 airy_err_t，经 airy_types.h 引入），此处不再重复定义。
 *          A-UEF 语义：宏为正幅值，函数返回 -AIRY_E* 产生负值错误。
 */
#define AIRY_SUCCESS 0

/**
 * @brief Generic error code definitions
 *
 * S-1 收敛 (2026-08-14): AIRY_E* 错误码不再本地定义（原 #ifndef 守卫版本
 * 依赖 airy_types.h 的 POSIX 负值优先，且值域与 [SC] 正幅值空间不一致）。
 * 唯一权威为 [SC] airymax/error.h（正幅值 10 子空间 + AIRY_FAULT_* Fault 码，
 * 返回 -AIRY_E*），经 airy_types.h 引入。用户态扩展码 AIRY_ERR_* 权威源为
 * commons/utils/error/include/error_codes.h。
 */

/**
 * @brief Timestamp type (nanoseconds)
 * @details Unix timestamp with nanosecond precision
 */
typedef uint64_t airy_timestamp_t;

/**
 * @brief Millisecond time type
 */
typedef uint64_t airy_millis_t;

/**
 * @brief Unique identifier type
 * @details Used for task_id, session_id, agent_id, etc.
 */
typedef char airy_uuid_t[37];

/**
 * @brief Priority enumeration
 */
typedef enum {
    AIRY_PRIORITY_LOW = 0,
    AIRY_PRIORITY_NORMAL = 1,
    AIRY_PRIORITY_HIGH = 2,
    AIRY_PRIORITY_CRITICAL = 3
} airy_priority_t;

/**
 * @brief Generic result type
 * @details Used to return operation results and error information
 */
typedef struct {
    airy_err_t code;
    const char *message;
    const char *detail;
} airy_result_t;

/** @} */ /* end of BasicTypes */
/*
 * Part 2: Task type definitions
 */

/**
 * @defgroup TaskTypes Task types
 * @brief Data types related to task management
 * @{
 */

/**
 * @brief Task state enumeration
 * @details Defines the task lifecycle states
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
 * @brief Task type enumeration
 */
typedef enum {
    AIRY_TASKTYPE_ONESHOT = 0,
    AIRY_TASKTYPE_RECURRING = 1,
    AIRY_TASKTYPE_CONDITIONAL = 2
} airy_task_type_t;

/**
 * @brief Task handle type
 * @details Used to reference task instances
 */
#ifndef AIRY_TASK_T_DEFINED
#define AIRY_TASK_T_DEFINED
typedef struct airy_task airy_task_t;
#endif

/**
 * @brief Task configuration structure
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
 * @brief Task result structure
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
/*
 * Part 3: Memory type definitions
 */

/**
 * @defgroup MemoryTypes Memory types
 * @brief Data types related to memory management
 * @{
 */

/**
 * @brief Memory layer enumeration
 * @details Four-layer memory hierarchy
 */
typedef enum {
    AIRY_MEM_LAYER1_RAW = 0,
    AIRY_MEM_LAYER2_WORKING = 1,
    AIRY_MEM_LAYER3_EPISODIC = 2,
    AIRY_MEM_LAYER4_SEMANTIC = 3
} airy_memory_layer_t;

/**
 * @brief Memory type enumeration
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
 * @brief Memory handle type - forward declaration (defined in
 *        provider.h)
 */
struct airy_memory;
typedef struct airy_memory airy_memory_t;

/**
 * @brief Memory entry structure
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
 * @brief Memory search configuration
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
 * @brief Memory search results
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
/*
 * Part 4: Session type definitions
 */

/**
 * @defgroup SessionTypes Session types
 * @brief Data types related to session management
 * @{
 */

/**
 * @brief Session state enumeration
 */
typedef enum {
    AIRY_SESSION_ACTIVE = 0,
    AIRY_SESSION_IDLE = 1,
    AIRY_SESSION_CLOSED = 2,
    AIRY_SESSION_EXPIRED = 3
} airy_session_status_t;

/**
 * @brief Session handle type
 */
typedef struct airy_session *airy_session_t;

/**
 * @brief Session configuration structure
 */
typedef struct {
    const char *user_id;
    const char *project_id;
    const char *context;
    uint32_t ttl_seconds;
    airy_priority_t priority;
} airy_session_config_t;

/**
 * @brief Session information structure
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
 * @brief Execution context structure
 * @details Carries context information along the request chain
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
/*
 * Part 5: Agent type definitions
 */

/**
 * @defgroup AgentTypes Agent types
 * @brief Data types related to agent contracts and capabilities
 * @{
 */

/**
 * @brief Agent maintenance level enumeration
 */
typedef enum {
    AIRY_AGENT_COMMUNITY = 0,
    AIRY_AGENT_VERIFIED = 1,
    AIRY_AGENT_OFFICIAL = 2
} airy_agent_level_t;

#ifndef AIRY_CAPABILITY_T_DEFINED
#define AIRY_CAPABILITY_T_DEFINED
/**
 * @brief Agent capability structure
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
 * @brief Agent model configuration
 */
typedef struct {
    char *system1;
    char *system2;
} airy_models_t;

/**
 * @brief Agent cost overview
 */
typedef struct {
    uint32_t token_per_task_avg;
    double api_cost_per_task;
    airy_agent_level_t level;
} airy_cost_profile_t;

/**
 * @brief Agent trust metrics
 */
typedef struct {
    uint32_t install_count;
    float rating;
    bool verified_provider;
    char *last_audit;
} airy_trust_metrics_t;

/**
 * @brief Agent contract structure
 * @details Complete agent metadata definition
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
/*
 * Part 6: Observability type definitions
 */

/**
 * @defgroup ObservabilityTypes Observability types
 * @brief Data types related to metrics, traces, and logs
 * @{
 */

/**
 * @brief Log level enumeration
 *
 * A-ULP SSoT (S-2 收敛, 2026-08-14): 5 级日志枚举的唯一权威源为
 * [SC] 共享契约头 airymax/log_types.h 的 enum airy_log_level
 * （AIRY_LOG_DEBUG=0 .. AIRY_LOG_FATAL=4）。用户态类型别名由
 * utils/logging/include/svc_logger.h 提供（typedef log_level_t
 * airy_log_level_t），数值与 [SC] 严格一致，此处不再重复定义。
 */

/**
 * @brief Metric type enumeration
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
 * @brief Span type enumeration
 */
typedef enum {
    AIRY_SPAN_INTERNAL = 0,
    AIRY_SPAN_CLIENT = 1,
    AIRY_SPAN_SERVER = 2,
    AIRY_SPAN_PRODUCER = 3,
    AIRY_SPAN_CONSUMER = 4
} airy_span_kind_t;

/**
 * @brief Span state enumeration
 */
typedef enum { AIRY_SPAN_UNSET = 0, AIRY_SPAN_OK = 1, AIRY_SPAN_ERROR = 2 } airy_span_status_t;

/**
 * @brief Metric data structure
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
 * @brief Span data structure
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
 * @brief Telemetry data structure
 */
typedef struct {
    airy_metric_t *metrics;
    size_t metric_count;
    airy_span_t *spans;
    size_t span_count;
    char *logs;
} airy_telemetry_t;

/** @} */ /* end of ObservabilityTypes */
/*
 * Part 7: IPC type definitions
 */

/**
 * @defgroup IPCTypes IPC types
 * @brief Data types related to inter-process communication
 * @{
 */

/**
 * @brief IPC channel type enumeration
 */
typedef enum {
    AIRY_IPC_PIPE = 0,
    AIRY_IPC_SOCKET = 1, /**< Unix Socket / Named Pipe */
    AIRY_IPC_SHM = 2,
    AIRY_IPC_MQ = 3,
    AIRY_IPC_RPC = 4
} airy_ipc_type_t;

/**
 * @brief IPC message flags
 */
typedef enum {
    AIRY_IPC_FLAG_NONE = 0,
    AIRY_IPC_FLAG_NONBLOCK = 1,
    AIRY_IPC_FLAG_PRIORITY = 2,
    AIRY_IPC_FLAG_BROADCAST = 4
} airy_ipc_flag_t;


/**
 * @brief IPC channel handle type
 * @note Kernel-level IPC channel type; full definition in
 *       corekern/include/ipc.h. Application layers should use
 *       ipc_channel_t from commons/utils/ipc/include/ipc_common.h
 */

/**
 * @brief IPC channel configuration
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
/*
 * Part 8: Network type definitions
 */

/**
 * @defgroup NetworkTypes Network types
 * @brief Data types related to network communication
 * @{
 */

/**
 * @brief Protocol type enumeration
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
 * @brief Connection state enumeration
 */
typedef enum {
    AIRY_CONN_DISCONNECTED = 0,
    AIRY_CONN_CONNECTING = 1,
    AIRY_CONN_CONNECTED = 2,
    AIRY_CONN_CLOSING = 3,
    AIRY_CONN_ERROR = 4
} airy_conn_state_t;

/**
 * @brief Socket handle type
 * Defined in platform.h
 */
/* typedef struct airy_socket* airy_sock_t; */

/**
 * @brief Connection endpoint structure
 */
typedef struct {
    char *host;
    uint16_t port;
    airy_protocol_t protocol;
    char *path;
} airy_endpoint_t;

/**
 * @brief Connection configuration structure
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
 * @brief HTTP request structure
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
 * @brief HTTP response structure
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
/*
 * Part 9: Helper macro definitions
 */

/**
 * @defgroup HelperMacros Helper macros
 * @brief Common helper macro definitions
 * @{
 */

/**
 * @brief Array element count
 */
#define AIRY_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief Minimum-value macro
 */
#define AIRY_MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Maximum-value macro
 */
#define AIRY_MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Alignment macro
 */
#define AIRY_ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/**
 * @brief Stringification macros
 */
#define AIRY_STRINGIFY(x) #x
#define AIRY_TOSTRING(x) AIRY_STRINGIFY(x)

/**
 * @brief Concatenation macros
 */
#define AIRY_CONCAT(a, b) a##b
#define AIRY_CONCAT3(a, b, c) a##b##c

/**
 * @brief Version number parsing macros
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
 * @brief Time conversion macros
 */
#define AIRY_MS_TO_NS(ms) ((uint64_t)(ms) * 1000000ULL)
#define AIRY_SEC_TO_MS(s) ((uint64_t)(s) * 1000ULL)
#define AIRY_SEC_TO_NS(s) ((uint64_t)(s) * 1000000000ULL)

/** @} */ /* end of HelperMacros */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TYPES_H */
