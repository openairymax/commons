// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file airy_llm_stream.h
 * @brief LLM 流式控制帧（RS 分帧）协议单一权威（SSoT）。
 *
 * llm_d 的 provider 在流式回复的裸文本分片之间插入控制帧，把无法用正文
 * 表达的旁路信息带内传给消费者：
 *
 *     RS <tag> <body> RS
 *
 *   tag 'T'：tool_calls JSON 数组（流结束前一次性发出，替换语义）；
 *   tag 'R'：reasoning_content 增量（逐 delta 发出，追加语义）；
 *   tag 'U'：usage（真实 token 计数与费用，流结束时发出）；
 *   tag 'E'：错误（JSON-RPC error 对象，参数校验等前置失败）。
 *
 * 分帧无歧义：RS(0x1E) 既不出现在帧体 JSON 内（cJSON 转义控制字符），
 * 也不出现在普通 LLM 文本内。
 *
 * 本头是生产者（daemons/llm_d providers）与消费者（atoms/coreloopthree
 * LLM adapter、daemons/agent_d run loop）的唯一权威：禁止在别处重定义
 * 这些字面量；新增 tag 只允许追加，禁止改动既有取值（只加不删）。
 */

#ifndef AIRY_RT_LLM_STREAM_H
#define AIRY_RT_LLM_STREAM_H

/* 帧界定符：既非合法正文，也不出现在帧体 JSON 内 */
#define AIRY_LLM_STREAM_RS 0x1e

/* 控制帧 tag */
#define AIRY_LLM_STREAM_TAG_TOOL   'T'
#define AIRY_LLM_STREAM_TAG_REASON 'R'
#define AIRY_LLM_STREAM_TAG_USAGE  'U'
#define AIRY_LLM_STREAM_TAG_ERROR  'E'

/* 帧体缓冲初始容量；超出按 2 倍动态增长，禁止静默截断帧体 */
#define AIRY_LLM_STREAM_FRAME_CAP 32768

/* 帧体 fail-closed 上限：超过视为流协议异常，整帧丢弃并告警 */
#define AIRY_LLM_STREAM_FRAME_MAX (4u * 1024u * 1024u)

/* 该字节是否为已定义的控制帧 tag */
#define AIRY_LLM_STREAM_IS_TAG(c)                                                            \
    ((c) == AIRY_LLM_STREAM_TAG_TOOL || (c) == AIRY_LLM_STREAM_TAG_REASON ||                 \
     (c) == AIRY_LLM_STREAM_TAG_USAGE || (c) == AIRY_LLM_STREAM_TAG_ERROR)

#endif /* AIRY_RT_LLM_STREAM_H */
