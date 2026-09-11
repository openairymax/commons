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
 * types locally (ARC-02: atoms must not include an upper-layer header
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
    char *finish_reason;
} llm_response_t;

typedef void (*llm_stream_callback_t)(const char *chunk, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_LLM_SERVICE_TYPES_H */
