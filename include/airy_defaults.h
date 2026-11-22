/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file airy_defaults.h
 * @brief Centralized definitions of AgentRT global shared defaults.
 *
 * P0.17 phase 1: migrated from daemons/common/include/daemon_defaults.h to
 * commons/include/airy_defaults.h, removing the compile-time reverse
 * dependency atoms->daemons (IRON-6: no cross-layer coupling residue).
 * Macros keep the AIRY_ prefix to signal global shared semantics rather
 * than daemon-specific ones.
 *
 * Hard-coded timeout/retry/buffer/port defaults scattered across modules
 * are unified here for global policy tuning and ops configuration.
 */

#ifndef AIRY_RT_DEFAULTS_H
#define AIRY_RT_DEFAULTS_H


#define AIRY_DEFAULT_TIMEOUT_MS 30000
#define AIRY_DEFAULT_TIMEOUT_SEC 30
/* think/orchestrate 管线超时 SSoT：think_d 服务端（svc.c 内置缺省）与
 * airy_cli 客户端（/orch 等待预算量子）共同默认；运行时被 model.yaml
 * [think] timeout_ms / env AIRY_THINK_TIMEOUT_MS 覆盖。 */
#define AIRY_THINK_TIMEOUT_DEFAULT_MS 120000
#define AIRY_SHUTDOWN_TIMEOUT_MS 5000
#define AIRY_HEALTHCHECK_INTERVAL_MS 5000
#define AIRY_HEARTBEAT_INTERVAL_MS 30000
#define AIRY_SOCKET_ACCEPT_TIMEOUT_MS 5000
#define AIRY_CONNECT_TIMEOUT_MS 10000


#define AIRY_DEFAULT_MAX_RETRIES 3
#define AIRY_DEFAULT_RETRY_DELAY_MS 100
#define AIRY_DEFAULT_BACKOFF_FACTOR 2
#define AIRY_DEFAULT_JITTER_RATIO 10

/* 生成采样默认 SSoT：请求未显式设置、模型条目也未声明时由 daemon
 * （envelope 唯一写入点）兜底。客户端不得复刻该字面量。 */
#define AIRY_LLM_DEFAULT_TEMPERATURE 0.7


#define AIRY_DEFAULT_RECV_BUFFER 65536
#define AIRY_DEFAULT_COMMAND_BUFFER 4096
#define AIRY_DEFAULT_OUTPUT_BUFFER 4096
#define AIRY_MAX_REQUEST_SIZE_HTTP (10 * 1024 * 1024)
#define AIRY_MAX_REQUEST_SIZE_WS (10 * 1024 * 1024)
#define AIRY_MAX_REQUEST_SIZE_STDIO (1 * 1024 * 1024)


#define AIRY_DEFAULT_MAX_WORKERS 4
#define AIRY_DEFAULT_MAX_CLIENTS 64
#define AIRY_DEFAULT_MAX_CONCURRENT 1000
#define AIRY_DEFAULT_THREAD_POOL_SIZE 8


#define AIRY_DEFAULT_CACHE_CAPACITY 1024
#define AIRY_DEFAULT_CACHE_TTL_SEC 3600


/* AirymaxRT 私有端口带 SSoT（唯一权威定义）。
 *
 * 受辖带 AIRY_PORT_BAND_MIN..AIRY_PORT_BAND_MAX（2026-2100）；全工程
 * 所有 IPC 监听口与客户端默认口统一登记于此，消费者（daemon
 * .manifest、平台端点表、适配器）一律引用本表，禁止复刻端口字面量。
 * 段位规划与治理规则见 docs 台账 §65①。对外映射保留行业惯例：
 * 8080->AIRY_PORT_GATEWAY_HTTP、8081->AIRY_PORT_GATEWAY_WS。 */
#define AIRY_PORT_BAND_MIN 2026
#define AIRY_PORT_BAND_MAX 2100

#define AIRY_PORT_KERNEL_IPC 2026
#define AIRY_PORT_GATEWAY_HTTP 2027
#define AIRY_PORT_GATEWAY_WS 2028
#define AIRY_PORT_GATEWAY_ADMIN 2029
#define AIRY_PORT_GATEWAY_HTTP2 2030
#define AIRY_PORT_KERNEL_IPC_TCP 2031

#define AIRY_PORT_LLM_D 2032
#define AIRY_PORT_TOOL_D 2033
#define AIRY_PORT_MARKET_D 2034
#define AIRY_PORT_SCHED_D 2035
#define AIRY_PORT_MEM_D 2036
#define AIRY_PORT_AGENT_D 2037
#define AIRY_PORT_A2A_D 2038
#define AIRY_PORT_MATHS_D 2039
#define AIRY_PORT_CUPOLAS_D 2040
#define AIRY_PORT_THINK_D 2041
#define AIRY_PORT_HOOK 2042
#define AIRY_PORT_CHANNEL_D 2043
#define AIRY_PORT_MONIT_D 2044
#define AIRY_PORT_NOTIFY_D 2045
#define AIRY_PORT_SUPERVISOR_D 2046

/* 本地推理端点目录 SSoT（外部生态惯例口，不属上列受辖带）。
 *
 * 三处「本地」语义不同，禁止互相顶替：providers 适配器缺省走
 * vLLM/OpenAI 兼容口 8080；platform_gpu 端点探测缺省平台加速服务口
 * 8000；自托管 llama-server 用 11435，Ollama 保留 11434。TUI 与
 * Python 侧以同值常量引用，CI 值一致性门禁逐项比对四组字面量。 */
#define AIRY_EP_LOCAL_PROVIDERS_PORT 8080
#define AIRY_EP_LOCAL_PLATFORM_PORT 8000
#define AIRY_EP_SELFHOST_PORT 11435
#define AIRY_EP_OLLAMA_PORT 11434

#define AIRY_EP_STR_(x) #x
#define AIRY_EP_STR(x) AIRY_EP_STR_(x)
#define AIRY_EP_LOCAL_PROVIDERS_URL \
    "http://127.0.0.1:" AIRY_EP_STR(AIRY_EP_LOCAL_PROVIDERS_PORT) "/v1"

#define AIRY_DEFAULT_TOKEN_TTL_SEC 3600
#define AIRY_DEFAULT_REFRESH_THRESHOLD 300
#define AIRY_DEFAULT_RPS_LIMIT 100
#define AIRY_DEFAULT_BURST_SIZE 20


#define AIRY_CB_FAILURE_THRESHOLD 5
#define AIRY_CB_SUCCESS_THRESHOLD 3
#define AIRY_CB_HALF_OPEN_MAX 1
#define AIRY_CB_WINDOW_SIZE_MS 60000
#define AIRY_CB_SLOW_CALL_MS 5000
#define AIRY_CB_SLOW_CALL_RATE_PCT 50
#define AIRY_CB_FAILURE_RATE_PCT 50
#define AIRY_CB_TIMEOUT_MS 30000


#define AIRY_API_REC_MAX_RETRY 5
#define AIRY_API_REC_BASE_DELAY_MS 200
#define AIRY_API_REC_BACKOFF_FACTOR 2.0f
#define AIRY_API_REC_JITTER_PCT 10
#define AIRY_API_REC_HEALTH_DECAY 0.9f
#define AIRY_API_REC_HEALTH_PENALTY 0.3f
#define AIRY_API_REC_HEALTH_MIN 0.2f
#define AIRY_API_REC_CONSECUTIVE_DISABLE 5


#define AIRY_MONITOR_INTERVAL_MS 30000
#define AIRY_ALERT_EVAL_INTERVAL_MS 10000
#define AIRY_ALERT_COOLDOWN_MS 60000
#define AIRY_ALERT_ESCALATION_MS 300000
#define AIRY_ALERT_MAX_NOTIFICATIONS 10


#define AIRY_CONFIG_WATCH_INTERVAL_MS 5000
#define AIRY_VAULT_AUTO_LOCK_SEC 300
#define AIRY_VAULT_MAX_RETRIES 3
#define AIRY_VAULT_MAX_CHAIN_DEPTH 5

#endif /* AIRY_RT_DEFAULTS_H */
