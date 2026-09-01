// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file airy_tool_schema.h
 * @brief 工具目录 OpenAI tools schema 单一权威（SSoT，M1-1a 下沉）。
 *
 * 工具目录的 LLM 视角表述（OpenAI function calling tools 数组）唯一定义于此，
 * 供 agent_d（run 引擎工具循环）、gateway（SSE chat 工具循环 / agent.run
 * 工具循环）与 tool_d（内置工具执行校验）共引，杜绝双份手写漂移。
 *
 * SSoT 规则：本数组 MUST 与 tool_d/service_builtin.c 注册的 builtin 工具
 * 一一对应（id / description / parameters.required 数组）；tool_d 校验器
 * 将每个 required!=0 参数视为必填，若此处标记 optional 而 tool_d 要求必填，
 * LLM 可能省略该参数导致校验失败。一致性由 daemons/gateway_d 的
 * test_mcp_tools_schema.c 门禁保证。
 */

#ifndef AIRY_RT_TOOL_SCHEMA_H
#define AIRY_RT_TOOL_SCHEMA_H

#define AIRY_TOOLS_JSON_SOURCE \
    "[" \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_read\"," \
    "\"description\":\"Read a file's content from the local filesystem\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}," \
    "\"required\":[\"path\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"fs_write\"," \
    "\"description\":\"Write content to a local file (creates or overwrites)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}," \
    "\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"fs_list\"," \
    "\"description\":\"List entries of a local directory (JSON array)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}," \
    "\"required\":[]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"shell_run\"," \
    "\"description\":\"Execute a shell command and capture its output\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}}," \
    "\"required\":[\"command\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"web_fetch\"," \
    "\"description\":\"Fetch a web page over HTTP(S) and return its body text\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\"}}," \
    "\"required\":[\"url\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"fs_glob\"," \
    "\"description\":\"List files matching a glob pattern (supports * ? and **)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"}," \
    "\"base\":{\"type\":\"string\"}},\"required\":[\"pattern\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"fs_grep\"," \
    "\"description\":\"Search file contents with a regular expression (relpath:line:text)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"}," \
    "\"path\":{\"type\":\"string\"},\"glob\":{\"type\":\"string\"}," \
    "\"max_results\":{\"type\":\"integer\"}},\"required\":[\"pattern\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"fs_edit\"," \
    "\"description\":\"Replace an exact string in a file (search-and-replace edit)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}," \
    "\"old\":{\"type\":\"string\"},\"new\":{\"type\":\"string\"}," \
    "\"count\":{\"type\":\"integer\"}},\"required\":[\"path\",\"old\",\"new\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"fs_delete\"," \
    "\"description\":\"Delete a local file, or a directory (recursive=1 for " \
    "non-empty trees; destructive)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}," \
    "\"recursive\":{\"type\":\"boolean\"}},\"required\":[\"path\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"web_search\"," \
    "\"description\":\"Search the web (DuckDuckGo) and return ranked results\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}," \
    "\"max_results\":{\"type\":\"integer\"}},\"required\":[\"query\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"git_exec\"," \
    "\"description\":\"Execute a read-only git command (whitelisted: " \
    "status/diff/log/branch/show/ls-files/grep) and capture output\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"command_args\":" \
    "{\"type\":\"array\",\"items\":{\"type\":\"string\"}}," \
    "\"cwd\":{\"type\":\"string\"}},\"required\":[\"command_args\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"git_diff\"," \
    "\"description\":\"Generate a unified diff for a path (git diff [--cached] [path])\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}," \
    "\"staged\":{\"type\":\"boolean\"}},\"required\":[]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"git_apply\"," \
    "\"description\":\"Apply a unified diff to the working tree (git apply [--check] -)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"patch\":{\"type\":\"string\"}," \
    "\"check_only\":{\"type\":\"boolean\"}},\"required\":[\"patch\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"maths_eval\"," \
    "\"description\":\"Evaluate a math expression precisely (arithmetic, " \
    "powers, factorial, sqrt/sin/cos/tan/ln/log10/log2/exp/abs/min/max/floor/ceil " \
    "etc.)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"expression\":" \
    "{\"type\":\"string\"}},\"required\":[\"expression\"]}}}" \
    ",{\"type\":\"function\",\"function\":{\"name\":\"maths_stats\"," \
    "\"description\":\"Compute descriptive statistics of a numeric array " \
    "(sum/mean/median/min/max/variance/stddev)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"}," \
    "\"values\":{\"type\":\"array\",\"items\":{\"type\":\"number\"}}}," \
    "\"required\":[\"op\",\"values\"]}}}" \
    "]"

#endif /* AIRY_RT_TOOL_SCHEMA_H */
