/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * llm_service_types.h - Canonical LLM service boundary Type Definitions
 *
 * Single source of truth for the LLM request/response contract that
 * crosses the atoms <-> llm_d boundary. atoms/coreloopthree builds the
 * request and interprets the response; daemons/llm_d implements the
 * service. Both sides MUST include this file instead of defining the
 * types locally (atoms must not include an upper-layer header
 * directly).
 *
 * Only the boundary-crossing subset lives here. The service lifecycle
 * entry points (llm_service_create/destroy/stats/list_models/embeddings)
 * stay in daemons/llm_d/include/llm_service.h, which includes this
 * header.
 */

#ifndef AIRY_RT_LLM_SERVICE_TYPES_H
#define AIRY_RT_LLM_SERVICE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llm_service llm_service_t;

typedef struct {
    const char *role;
    const char *content;
    /* Reasoning trace (reasoning models, e.g. DeepSeek-R1 / Kimi-K2):
     * - Request side: an assistant message produced by a reasoning model must
     *   echo its reasoning_content when the turn is re-sent; DeepSeek and
     *   Kimi reject a tool-loop request with HTTP 400 if the field is
     *   dropped between turns.
     * - Response side: choices[].reasoning_content carries the reasoning
     *   trace of the current completion.
     * NULL when the message/choice has no reasoning. */
    const char *reasoning_content;
    /* Function calling (OpenAI-compatible):
     * - role="tool" messages carry tool_call_id (matching the assistant's
     *   tool_call id)
     * - role="assistant" messages may carry tool_calls (JSON array string
     *   with id/type/function.name/function.arguments) */
    const char *tool_call_id;
    const char *tool_calls_json;
} llm_message_t;

/* C11 endpoint capability bits: which optional request fields the endpoint
 * family accepts. Derived statically from model.yaml endpoint_kind at config
 * build time; stamped onto the request by the provider driver. Unknown kind
 * => all-false (fail-safe: never send a field the endpoint may reject). */
typedef struct {
    bool accepts_seed;
    bool accepts_grammar;
    bool accepts_num_ctx;
    bool accepts_response_format;
} local_caps_t;

/* response_format values for the OpenAI-compatible wire field. */
#define LLM_RF_TEXT 0u         /* absent: field not sent */
#define LLM_RF_JSON_OBJECT 1u  /* {"type":"json_object"} */

typedef struct {
    const char *model;
    const llm_message_t *messages;
    size_t message_count;
    float temperature;
    float top_p;
    int max_tokens;
    int stream;
    const char **stop;
    size_t stop_count;
    double presence_penalty;
    double frequency_penalty;
    /* JSON string of the OpenAI tools array (function-calling tool
     * definitions, e.g. [{"type":"function","function":{"name":"fs_read",
     * "parameters":{...}}}]) */
    const char *tools_json;
    /* B5-3 缓存准入声明：仅当调用方显式声明 cacheable=1 时，
     * 请求才允许读写语义缓存（读、写两侧同源同判，缺省 0 = fail-closed）。 */
    int cacheable;
    void *user_data;
    /* C11 确定性采样与结构化输出字段（尾部追加，无 ABI 破坏）。
     * 零值 = 未设置 = 不发送：旧调用路径的请求体字节级不变。
     * 实际下发另受 caps 能力位门控（由 provider driver 盖印）。 */
    uint32_t seed;           /* 确定性种子，0 = 不发送 */
    bool temperature_set;    /* true = temperature 为显式设置（含 0.0） */
    const char *grammar;     /* GBNF 语法串，NULL = 不发送 */
    uint8_t response_format; /* LLM_RF_*，0 = 不发送 */
    uint32_t num_ctx;        /* 上下文窗口，0 = 不发送 */
    local_caps_t caps;       /* driver 从 provider 配置盖印，调用方勿自填 */
} llm_request_config_t;

/* Endpoint-reported per-request evaluation timings, carried in the
 * llama.cpp-compatible top-level "timings" object (prompt_n / prompt_ms /
 * predicted_n / predicted_ms / cache_n). prefill_* and decode_* are the
 * G47 column split: prompt evaluation vs token generation. kv_cache_tokens
 * counts tokens reused from the server-side KV slot cache — distinct from
 * cached_tokens, which is the provider billing detail inside prompt_tokens.
 * All zero when the endpoint does not report timings. */
typedef struct {
    uint32_t prefill_tokens;
    uint32_t decode_tokens;
    uint32_t kv_cache_tokens;
    double prefill_ms;
    double decode_ms;
} llm_timings_t;

typedef struct {
    char *id;
    char *model;
    llm_message_t *choices;
    size_t choice_count;
    uint64_t created;
    uint32_t prompt_tokens;
    uint32_t completion_tokens;
    uint32_t total_tokens;
    /* Thinking (reasoning) tokens reported by the upstream usage block
     * (e.g. DeepSeek/OpenAI completion_tokens_details.reasoning_tokens).
     * Zero when the upstream does not report it. */
    uint32_t reasoning_tokens;
    /* Prompt tokens served from the provider-side prefix cache
     * (usage.prompt_tokens_details.cached_tokens, or the Messages line's
     * cache_read_input_tokens). Included in prompt_tokens; kept separate
     * so cache hit-rate stays observable. Zero when not reported. */
    uint32_t cached_tokens;
    /* Per-request prefill/decode measurement echo from the endpoint
     * (llama.cpp top-level "timings"). Zero when not reported. */
    llm_timings_t timings;
    double cost_usd;
    /* Completion outcome in the canonical vocabulary defined below.
     * Every provider normalizes its native marker with
     * llm_finish_reason_norm() before returning, so consumers
     * (llm_d / gateway / cli) never branch on provider-specific strings. */
    char *finish_reason;
} llm_response_t;

/**
 * @brief Release an llm_response_t and every field it owns.
 *
 * The response object crosses the atoms <-> llm_d boundary, so its
 * destructor is part of the boundary contract rather than a private
 * detail of one side: llm_d's rpc edge, the provider adapters and the
 * atoms-side consumers all release through this one entry point instead
 * of hand-rolling the field walk. The single definition lives in
 * daemons/llm_d/src/rpc/service.c.
 *
 * @param resp Response to release (NULL is a no-op)
 */
void llm_response_free(llm_response_t *resp);

typedef void (*llm_stream_callback_t)(const char *chunk, void *user_data);

/* Canonical finish_reason vocabulary for the whole stack. Native upstream
 * markers are mapped onto these four values at the provider edge. */
#define LLM_FINISH_STOP "stop"                     /* natural end */
#define LLM_FINISH_LENGTH "length"                 /* output token cap hit */
#define LLM_FINISH_CONTENT_FILTER "content_filter" /* upstream safety block */
#define LLM_FINISH_TOOL_CALLS "tool_calls"         /* turn yielded tool calls */

/**
 * @brief Normalize a provider-native completion marker to the canonical set.
 *
 * Markers already expressed in the canonical set pass through unchanged;
 * unrecognized markers are returned as-is so no upstream signal is dropped.
 *
 * @param raw Native marker (may be NULL)
 * @return Canonical marker (static storage), never NULL
 */
static inline const char *llm_finish_reason_norm(const char *raw)
{
    if (!raw || !raw[0])
        return LLM_FINISH_STOP;
    if (strcmp(raw, "STOP") == 0 || strcmp(raw, "end_turn") == 0 ||
        strcmp(raw, "stop_sequence") == 0)
        return LLM_FINISH_STOP;
    if (strcmp(raw, "MAX_TOKENS") == 0 || strcmp(raw, "max_tokens") == 0)
        return LLM_FINISH_LENGTH;
    if (strcmp(raw, "SAFETY") == 0 || strcmp(raw, "refusal") == 0)
        return LLM_FINISH_CONTENT_FILTER;
    if (strcmp(raw, "tool_use") == 0)
        return LLM_FINISH_TOOL_CALLS;
    return raw;
}

/**
 * @brief Whether the turn ended because the output token cap was reached.
 *
 * True means the received content is a prefix of the model's answer rather
 * than a finished one. Single predicate for the whole stack — callers must
 * not re-test the literal "length" on their own.
 *
 * @param finish_reason Canonical marker (may be NULL)
 * @return 1 when truncated, otherwise 0
 */
static inline int llm_finish_is_truncated(const char *finish_reason)
{
    return finish_reason && strcmp(finish_reason, LLM_FINISH_LENGTH) == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_LLM_SERVICE_TYPES_H */
