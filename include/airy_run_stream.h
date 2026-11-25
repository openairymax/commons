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
/* C-15：统一任务编排入口的任务生命周期帧。
 * - step：单步计划状态（step_index / step_status / title）；
 * - task_state：task 一级状态（state / mode / mode_reason / task_id）；
 * - plan_unavailable：计划不可得时的显式降级（携带 reason，禁止静默降级）。 */
#define AIRY_RS_TYPE_STEP             "step"
#define AIRY_RS_TYPE_TASK_STATE       "task_state"
#define AIRY_RS_TYPE_PLAN_UNAVAILABLE "plan_unavailable"
/* C-16：本轮工具目录快照（catalog_source / tools / available / denied）。
 * 静态 bootstrap 与实时目录必须可区分，前端据此提示降级。 */
#define AIRY_RS_TYPE_CATALOG   "catalog"

/* execution 层 */
#define AIRY_RS_TYPE_TOOL_START "tool_start"
#define AIRY_RS_TYPE_TOOL_END   "tool_end"
#define AIRY_RS_TYPE_TOOL_DELTA "tool_delta"

/* outcome 层 */
#define AIRY_RS_TYPE_TOKEN_DELTA "token_delta"
#define AIRY_RS_TYPE_MESSAGE     "message"

/* ---- 各事件类型 data 负载键名 ---- */

/* catalog（C-16 工具目录协商） */
#define AIRY_RS_K_CATALOG_SOURCE "catalog_source"
#define AIRY_RS_K_TOOLS          "tools"
#define AIRY_RS_K_AVAILABLE      "available"
#define AIRY_RS_K_DENIED         "denied"
#define AIRY_RS_K_DENIED_REASON  "denied_reason"

/* catalog_source 取值：live = list_tools 实时目录；static = 内置 SSoT 回落 */
#define AIRY_RS_CATALOG_SOURCE_LIVE   "live"
#define AIRY_RS_CATALOG_SOURCE_STATIC "static"

/* run_start */
#define AIRY_RS_K_PROMPT     "prompt"
#define AIRY_RS_K_AGENT      "agent"
#define AIRY_RS_K_MODEL      "model"

/* run_end */
#define AIRY_RS_K_STATUS     "status"
#define AIRY_RS_K_DURATION   "duration_ms"
#define AIRY_RS_K_USE_TICKS  "use_ticks"
/* 完整工具轨迹（tool/arguments/result/ok），与 RPC result.tool_trace 同构；
 * 流式客户端无法从 tool_end 的摘要还原参数与结果，故终态帧整体回传一次。 */
#define AIRY_RS_K_TOOL_TRACE "tool_trace"
/* 预算协商与续跑（C-14）：轮次计量进终态帧；Paused 时 resume_token 必随
 * run_end 与 error 两处下发，客户端才不必重发全量历史。 */
#define AIRY_RS_K_ROUNDS_USED  "rounds_used"
#define AIRY_RS_K_ROUNDS_LIMIT "rounds_limit"
#define AIRY_RS_K_RESUME_TOKEN "resume_token"
/* 停止原因（N-7）：一次 run 为什么结束。done / cancelled 之外一律复用
 * AIRY_RS_REASON_* 词表，run_end / result / error 三处必须同源；
 * rounds_limit 只表示客户端或策略显式给了轮次上限并到点。 */
#define AIRY_RS_K_STOP_REASON  "stop_reason"
#define AIRY_RS_STOP_DONE      "done"
#define AIRY_RS_STOP_CANCELLED "cancelled"
#define AIRY_RS_STOP_ROUNDS    "rounds_limit"

/* error */
#define AIRY_RS_K_CODE       "code"
#define AIRY_RS_K_MSG        "message"
#define AIRY_RS_K_DETAIL     "detail"
#define AIRY_RS_K_RECOVER    "recoverable"
#define AIRY_RS_K_SUGGEST    "suggest_action"
#define AIRY_RS_K_ERROR_CODE "error_code"

/* 失败载荷（D-10）：run_end 与 error 两处同形下发。分层只补字段不改 code，
 * 客户端据 origin/retryable/next_action 决定重试、续跑还是人工介入。 */
#define AIRY_RS_K_FAILURE      "failure"
#define AIRY_RS_K_ORIGIN       "origin"
#define AIRY_RS_K_RETRYABLE    "retryable"
#define AIRY_RS_K_BUDGET_USED  "budget_used"
#define AIRY_RS_K_BUDGET_TOTAL "budget_total"
#define AIRY_RS_K_NODE         "affected_node"
#define AIRY_RS_K_NEXT_ACTION  "next_action"

/* 失败始发层：tool = 工具执行，agent = 编排/预算，gateway = 准入与转发，
 * provider = 模型流，config = 部署配置缺失/非法。 */
#define AIRY_RS_ORIGIN_TOOL     "tool"
#define AIRY_RS_ORIGIN_AGENT    "agent"
#define AIRY_RS_ORIGIN_GATEWAY  "gateway"
#define AIRY_RS_ORIGIN_PROVIDER "provider"
#define AIRY_RS_ORIGIN_CONFIG   "config"

/* 机器可读失败原因（稳定枚举，新增只增不改）。 */
#define AIRY_RS_REASON_BUDGET   "budget_exhausted"
#define AIRY_RS_REASON_TOOL     "tool_fuse"
#define AIRY_RS_REASON_DENIED   "approval_denied"
#define AIRY_RS_REASON_EMPTY    "empty_result"
#define AIRY_RS_REASON_PROVIDER "provider_unreachable"
#define AIRY_RS_REASON_AGENT    "agent_error"
/* 未配置可用模型（model.yaml 缺失/无生效条目，且未给 AIRY_AGENT_MODEL）：
 * 属部署配置问题，重试无效，必须按提示补配置。 */
#define AIRY_RS_REASON_CONFIG   "config_missing"
/* 模型服务可达但拒绝了请求（鉴权失败、模型名不存在、配额/限流、5xx）：
 * 与 provider_unreachable 区分——0.1.19 §3.7.2 N1 要求只有传输类失败
 * 才能对用户说「服务不可达」，否则会把鉴权错误误导成网络问题。 */
#define AIRY_RS_REASON_PROVIDER_ERR "provider_error"

/* 建议动作：retry 原样重发；resume 凭 resume_token 续跑；inspect 人工排障；
 * adjust 按判据逐项修复产物后重新提交（C-17，确定性失败不可原样重试）。 */
#define AIRY_RS_NEXT_RETRY   "retry"
#define AIRY_RS_NEXT_RESUME  "resume"
#define AIRY_RS_NEXT_INSPECT "inspect"
#define AIRY_RS_NEXT_ADJUST  "adjust"

/* 完成判据复核（C-17）：verifier 由 tool_d 在独立上下文只读执行，逐项回传
 * {type, expect, actual, pass}。状态 unverified 表示复核未能取证（verifier
 * 不可用），既非通过也非失败，客户端不得当作 completed 展示。 */
#define AIRY_RS_K_VERIFICATION   "verification"
#define AIRY_RS_ORIGIN_VERIFIER  "verifier"
#define AIRY_RS_REASON_UNMET     "criteria_unmet"
#define AIRY_RS_REASON_UNVERIFIED "verifier_unavailable"
#define AIRY_RS_STATE_UNVERIFIED "unverified"
#define AIRY_RS_STATE_NEEDS_CRITERIA "needs_criteria"

/* usage（D-11）：run_end/result/run_result 统一结构化计量，source 区分
 * provider 实报与估算。cost 与 token 同对象下发，避免两套平铺键各说各话。 */
#define AIRY_RS_K_USAGE       "usage"
#define AIRY_RS_K_USAGE_PROMPT "prompt"
#define AIRY_RS_K_USAGE_COMP  "completion"
#define AIRY_RS_K_USAGE_TOTAL "total"
#define AIRY_RS_K_USAGE_COST  "cost"
#define AIRY_RS_K_USAGE_SRC   "source"
#define AIRY_RS_USAGE_PROVIDER "provider"
#define AIRY_RS_USAGE_ESTIMATE "estimated"

/* plan */
#define AIRY_RS_K_PLAN       "plan"
#define AIRY_RS_K_STEPS      "steps"
#define AIRY_RS_K_STEP_ID    "id"
#define AIRY_RS_K_STEP_TITLE "title"

/* task.submit / task_state / step / plan_unavailable（C-15） */
#define AIRY_RS_K_TASK_ID          "task_id"
#define AIRY_RS_K_PLAN_ID          "plan_id"
#define AIRY_RS_K_MODE             "mode"
#define AIRY_RS_K_MODE_REASON      "mode_reason"
#define AIRY_RS_K_GCCP_CONFIRMED   "gccp_confirmed"
#define AIRY_RS_K_GCCP_MISSING     "gccp_missing"
#define AIRY_RS_K_SUCCESS_CRITERIA "success_criteria"
#define AIRY_RS_K_STATE            "state"
#define AIRY_RS_K_STEP_INDEX       "step_index"
#define AIRY_RS_K_STEP_STATUS      "step_status"
#define AIRY_RS_K_STEP_COUNT       "step_count"
/* step 帧的子目标文本（0-based step_index；文本取自计划步骤 title/goal/action） */
#define AIRY_RS_K_STEP             "step"
#define AIRY_RS_K_REASON           "reason"
/* 取消追溯（D-13 / E-20）：cancelled 终态必须能回答「谁在何时取消的」，
 * 只留 state=cancelled 会让跨客户端、看板与回放无法区分用户取消与关机。 */
#define AIRY_RS_K_CANCELLED_BY     "cancelled_by"
#define AIRY_RS_K_CANCELLED_AT     "cancelled_at"

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

/* run_end 分段耗时（与 duration_ms 总耗时并列，均为毫秒） */
#define AIRY_RS_K_THINK_MS   "think_ms"
#define AIRY_RS_K_LLM_MS     "llm_ms"
#define AIRY_RS_K_TOOL_MS    "tool_ms"

#endif /* AIRY_RT_RUN_STREAM_H */
