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
     * 响应才允许写入语义缓存（缺省 0 = fail-closed）。 */
    int cacheable;
    void *user_data;
} llm_request_config_t;

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
    double cost_usd;
    /* Completion outcome in the canonical vocabulary defined below.
     * Every provider normalizes its native marker with
     * llm_finish_reason_norm() before returning, so consumers
     * (llm_d / gateway / cli) never branch on provider-specific strings. */
    char *finish_reason;
} llm_response_t;

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
