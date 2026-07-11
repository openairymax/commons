/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file airy_defaults.h
 * @brief AgentRT 全局共享默认值集中定义
 *
 * P0.17 阶段 1：从 daemons/common/include/daemon_defaults.h 迁移至
 * commons/include/airy_defaults.h，消除 atoms→daemons 编译期反向依赖
 * （IRON-6: 禁止跨层耦合残留）。宏命名保持 AIRY_ 前缀，表明其全局
 * 共享语义，而非 daemon 专属。
 *
 * 将分散在各模块中的硬编码超时/重试/缓冲区/端口等默认值统一到此处，
 * 便于全局调整策略和运维配置。
 */

#ifndef AIRY_RT_DEFAULTS_H
#define AIRY_RT_DEFAULTS_H

/* ========== 超时默认值 ========== */

#define AIRY_DEFAULT_TIMEOUT_MS 30000
#define AIRY_DEFAULT_TIMEOUT_SEC 30
#define AIRY_SHUTDOWN_TIMEOUT_MS 5000
#define AIRY_HEALTHCHECK_INTERVAL_MS 5000
#define AIRY_HEARTBEAT_INTERVAL_MS 30000
#define AIRY_SOCKET_ACCEPT_TIMEOUT_MS 5000
#define AIRY_CONNECT_TIMEOUT_MS 10000

/* ========== 重试默认值 ========== */

#define AIRY_DEFAULT_MAX_RETRIES 3
#define AIRY_DEFAULT_RETRY_DELAY_MS 100
#define AIRY_DEFAULT_BACKOFF_FACTOR 2
#define AIRY_DEFAULT_JITTER_RATIO 10

/* ========== 缓冲区大小 ========== */

#define AIRY_DEFAULT_RECV_BUFFER 65536
#define AIRY_DEFAULT_COMMAND_BUFFER 4096
#define AIRY_DEFAULT_OUTPUT_BUFFER 4096
#define AIRY_MAX_REQUEST_SIZE_HTTP (10 * 1024 * 1024)
#define AIRY_MAX_REQUEST_SIZE_WS (10 * 1024 * 1024)
#define AIRY_MAX_REQUEST_SIZE_STDIO (1 * 1024 * 1024)

/* ========== 并发/线程 ========== */

#define AIRY_DEFAULT_MAX_WORKERS 4
#define AIRY_DEFAULT_MAX_CLIENTS 64
#define AIRY_DEFAULT_MAX_CONCURRENT 1000
#define AIRY_DEFAULT_THREAD_POOL_SIZE 8

/* ========== 缓存 ========== */

#define AIRY_DEFAULT_CACHE_CAPACITY 1024
#define AIRY_DEFAULT_CACHE_TTL_SEC 3600

/* ========== 端口/路径 ========== */

#define AIRY_DEFAULT_HTTP_PORT 8080
#define AIRY_DEFAULT_WS_PORT 8081
#define AIRY_DEFAULT_TOOL_PORT 8082
#define AIRY_DEFAULT_LLM_SOCK_PATH AIRY_RUNTIME_DIR "/llm.sock"
#define AIRY_DEFAULT_TOOL_SOCK_PATH AIRY_RUNTIME_DIR "/tool.sock"

/* ========== 安全/认证 ========== */

#define AIRY_DEFAULT_TOKEN_TTL_SEC 3600
#define AIRY_DEFAULT_REFRESH_THRESHOLD 300
#define AIRY_DEFAULT_RPS_LIMIT 100
#define AIRY_DEFAULT_BURST_SIZE 20

/* ========== 熔断器 ========== */

#define AIRY_CB_FAILURE_THRESHOLD 5
#define AIRY_CB_SUCCESS_THRESHOLD 3
#define AIRY_CB_HALF_OPEN_MAX 1
#define AIRY_CB_WINDOW_SIZE_MS 60000
#define AIRY_CB_SLOW_CALL_MS 5000
#define AIRY_CB_SLOW_CALL_RATE_PCT 50
#define AIRY_CB_FAILURE_RATE_PCT 50
#define AIRY_CB_TIMEOUT_MS 30000

/* ========== API恢复 ========== */

#define AIRY_API_REC_MAX_RETRY 5
#define AIRY_API_REC_BASE_DELAY_MS 200
#define AIRY_API_REC_BACKOFF_FACTOR 2.0f
#define AIRY_API_REC_JITTER_PCT 10
#define AIRY_API_REC_HEALTH_DECAY 0.9f
#define AIRY_API_REC_HEALTH_PENALTY 0.3f
#define AIRY_API_REC_HEALTH_MIN 0.2f
#define AIRY_API_REC_CONSECUTIVE_DISABLE 5

/* ========== 监控/告警 ========== */

#define AIRY_MONITOR_INTERVAL_MS 30000
#define AIRY_ALERT_EVAL_INTERVAL_MS 10000
#define AIRY_ALERT_COOLDOWN_MS 60000
#define AIRY_ALERT_ESCALATION_MS 300000
#define AIRY_ALERT_MAX_NOTIFICATIONS 10

/* ========== 配置 ========== */

#define AIRY_CONFIG_WATCH_INTERVAL_MS 5000
#define AIRY_VAULT_AUTO_LOCK_SEC 300
#define AIRY_VAULT_MAX_RETRIES 3
#define AIRY_VAULT_MAX_CHAIN_DEPTH 5

#endif /* AIRY_RT_DEFAULTS_H */
