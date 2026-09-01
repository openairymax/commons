// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file airy_run_stream.h
 * @brief agent.run_stream 流式事件帧协议 v1 schema 单一权威（SSoT）。
 *
 * 依据 0.1.9 架构改进方案 §2.4（M1-1d）：agent_d 引擎以流式事件向
 * gateway 推送，gateway 纯翻译为 SSE 帧。本头定义事件信封的字段
 * 名、事件类型枚举与负载键名，是 C 侧（agent_d 引擎事件源 / gateway
 * SSE 编码器）的唯一权威；sdk-rust 解码器（sdk/sdk-rust）与本头
 * 共引，保证编码端与解码端字段一致（Unify Design SSoT）。
 *
 * SSoT 规则：
 *   - 事件类型字符串（AIRY_RS_TYPE_*）为 wire 上的唯一标识，禁止
 *     在别处手写字符串字面量；
 *   - 信封字段名（AIRY_RS_K_*）为 wire JSON 键，禁止在别处硬编码；
 *   - 新增事件类型只允许追加，禁止修改既有类型的键名（只加不删、
 *     只弱化不收紧校验）。
 */

#ifndef AIRY_RT_RUN_STREAM_H
#define AIRY_RT_RUN_STREAM_H

#define AIRY_RS_VERSION 1

/* ---- 事件信封字段名（wire JSON 键，编码端/解码端共用） ---- */
#define AIRY_RS_K_V         "v"
#define AIRY_RS_K_TYPE      "type"
#define AIRY_RS_K_ID        "id"
#define AIRY_RS_K_RUN_ID    "run_id"
#define AIRY_RS_K_SESSION   "session_id"
#define AIRY_RS_K_TS        "ts"
#define AIRY_RS_K_EPOCH     "epoch"
#define AIRY_RS_K_DATA      "data"

/* ---- 事件类型（wire 上的 type 值，见方案 §2.4.3 分层枚举） ---- */

/* control 层 */
#define AIRY_RS_TYPE_RUN_START "run_start"
#define AIRY_RS_TYPE_RUN_END   "run_end"
#define AIRY_RS_TYPE_ERROR     "error"

/* cognition 层 */
#define AIRY_RS_TYPE_PLAN      "plan"

/* execution 层 */
#define AIRY_RS_TYPE_TOOL_START "tool_start"
#define AIRY_RS_TYPE_TOOL_END   "tool_end"
#define AIRY_RS_TYPE_TOOL_DELTA "tool_delta"

/* outcome 层 */
#define AIRY_RS_TYPE_TOKEN_DELTA "token_delta"
#define AIRY_RS_TYPE_MESSAGE     "message"

/* ---- 各事件类型 data 负载键名 ---- */

/* run_start */
#define AIRY_RS_K_PROMPT     "prompt"
#define AIRY_RS_K_AGENT      "agent"
#define AIRY_RS_K_MODEL      "model"

/* run_end */
#define AIRY_RS_K_STATUS     "status"
#define AIRY_RS_K_DURATION   "duration_ms"
#define AIRY_RS_K_USE_TICKS  "use_ticks"

/* error */
#define AIRY_RS_K_CODE       "code"
#define AIRY_RS_K_MSG        "message"
#define AIRY_RS_K_DETAIL     "detail"
#define AIRY_RS_K_RECOVER    "recoverable"
#define AIRY_RS_K_SUGGEST    "suggest_action"

/* plan */
#define AIRY_RS_K_PLAN       "plan"
#define AIRY_RS_K_STEPS      "steps"
#define AIRY_RS_K_STEP_ID    "id"
#define AIRY_RS_K_STEP_TITLE "title"

/* tool_start */
#define AIRY_RS_K_TOOL       "tool"
#define AIRY_RS_K_TOOL_ID    "tool_id"
#define AIRY_RS_K_ARGS       "args_preview"

/* tool_end */
#define AIRY_RS_K_TRUNCATED  "truncated"
#define AIRY_RS_K_RESULT_HASH "result_hash"

/* token_delta */
#define AIRY_RS_K_DELTA      "delta"
#define AIRY_RS_K_FINISH     "finish_reason"

/* message */
#define AIRY_RS_K_ROLE       "role"
#define AIRY_RS_K_CONTENT    "content"
#define AIRY_RS_K_REASONING  "reasoning"

#endif /* AIRY_RT_RUN_STREAM_H */
