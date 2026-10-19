// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file airy_tool_schema.h
 * @brief 工具目录 OpenAI tools schema 单一权威（SSoT，M1-1a 下沉）。
 *
 * 工具目录的 LLM 视角表述（OpenAI function calling tools 数组）唯一定义
 * 于此，供 agent_d（run 引擎工具循环）、gateway（SSE chat / agent.run 工
 * 具循环）、tool_d（内置工具执行校验）与 airy_cli（聊天工具回路子集）
 * 共引，杜绝双份手写漂移。
 *
 * 结构：每工具一个 AIRY_TOOL_JSON_<ID> 片段宏；AIRY_TOOLS_JSON_SOURCE
 * 为 16 工具全量拼接。消费者按策略选取子集时必须引用片段宏，禁止手写
 * schema 文本。
 *
 * SSoT 规则：片段的 id / description / parameters（含 required 数组）
 * MUST 与 tool_d/service_builtin.c 的 BTOOL 注册一一对应（描述逐字对齐，
 * test_registry 守护名字集合与 required 语义）；tool_d 校验器将每个
 * required!=0 参数视为必填，若此处标记 optional 而 tool_d 要求必填，
 * LLM 可能省略该参数导致校验失败。
 */

#ifndef AIRY_RT_TOOL_SCHEMA_H
#define AIRY_RT_TOOL_SCHEMA_H

/* 工具路径锚点契约（缺陷 #4）：所有 path/base/cwd 参数以 tool_d 进程工作
 * 目录（cwd）为相对路径解析基准；工具结果回执回显解析后的绝对路径，调用方
 * 据此判断锚点，无需猜测。 */
#define AIRY_TOOL_PATH_DESC \
    "File path. A relative path resolves against the tool service working directory; the result echoes the absolute path."
#define AIRY_TOOL_BASE_DESC \
    "Base directory. A relative path resolves against the tool service working directory (default '.')."
#define AIRY_TOOL_CWD_DESC \
    "Working directory for the command. A relative path resolves against the tool service working directory."
#define AIRY_TOOL_MKDIRS_DESC "Create missing parent directories (default true)."

#define AIRY_TOOL_JSON_FS_READ \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_read\"," \
    "\"description\":\"Read a file's content from the local filesystem\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"," \
    "\"description\":\"" AIRY_TOOL_PATH_DESC "\"}},\"required\":[\"path\"]}}}"

#define AIRY_TOOL_JSON_FS_WRITE \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_write\"," \
    "\"description\":\"Write content to a file (create/overwrite)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"," \
    "\"description\":\"" AIRY_TOOL_PATH_DESC "\"}," \
    "\"content\":{\"type\":\"string\"},\"create_dirs\":{\"type\":\"boolean\"," \
    "\"description\":\"" AIRY_TOOL_MKDIRS_DESC "\"}},\"required\":[\"path\",\"content\"]}}}"

#define AIRY_TOOL_JSON_FS_LIST \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_list\"," \
    "\"description\":\"List entries of a local directory (JSON array)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"," \
    "\"description\":\"" AIRY_TOOL_PATH_DESC "\"}},\"required\":[]}}}"

#define AIRY_TOOL_JSON_SHELL_RUN \
    "{\"type\":\"function\",\"function\":{\"name\":\"shell_run\"," \
    "\"description\":\"Execute a shell command and capture its output\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}," \
    "\"cwd\":{\"type\":\"string\",\"description\":\"" AIRY_TOOL_CWD_DESC "\"}}," \
    "\"required\":[\"command\"]}}}"

#define AIRY_TOOL_JSON_WEB_FETCH \
    "{\"type\":\"function\",\"function\":{\"name\":\"web_fetch\"," \
    "\"description\":\"Fetch a URL, return page body text\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\"," \
    "\"description\":\"Full http(s) URL\"}},\"required\":[\"url\"]}}}"

#define AIRY_TOOL_JSON_FS_GLOB \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_glob\"," \
    "\"description\":\"List files matching a glob pattern (supports * ? and **)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"}," \
    "\"base\":{\"type\":\"string\",\"description\":\"" AIRY_TOOL_BASE_DESC "\"}}," \
    "\"required\":[\"pattern\"]}}}"

#define AIRY_TOOL_JSON_FS_GREP \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_grep\"," \
    "\"description\":\"Search file contents with a regular expression (relpath:line:text)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"}," \
    "\"path\":{\"type\":\"string\",\"description\":\"" AIRY_TOOL_PATH_DESC "\"}," \
    "\"glob\":{\"type\":\"string\"}," \
    "\"max_results\":{\"type\":\"integer\"}},\"required\":[\"pattern\"]}}}"

#define AIRY_TOOL_JSON_FS_EDIT \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_edit\"," \
    "\"description\":\"Replace an exact string in a file (search-and-replace edit)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"," \
    "\"description\":\"" AIRY_TOOL_PATH_DESC "\"}," \
    "\"old\":{\"type\":\"string\"},\"new\":{\"type\":\"string\"}," \
    "\"count\":{\"type\":\"integer\"}},\"required\":[\"path\",\"old\",\"new\"]}}}"

#define AIRY_TOOL_JSON_FS_DELETE \
    "{\"type\":\"function\",\"function\":{\"name\":\"fs_delete\"," \
    "\"description\":\"Delete a local file, or a directory (recursive=1 for " \
    "non-empty trees; destructive)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"," \
    "\"description\":\"" AIRY_TOOL_PATH_DESC "\"}," \
    "\"recursive\":{\"type\":\"boolean\"}},\"required\":[\"path\"]}}}"

#define AIRY_TOOL_JSON_WEB_SEARCH \
    "{\"type\":\"function\",\"function\":{\"name\":\"web_search\"," \
    "\"description\":\"Search the web, return ranked results\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"," \
    "\"description\":\"Search query\"}," \
    "\"max_results\":{\"type\":\"integer\",\"description\":\"Max result count, 1-8\"," \
    "\"minimum\":1,\"maximum\":8}},\"required\":[\"query\"]}}}"

#define AIRY_TOOL_JSON_GIT_EXEC \
    "{\"type\":\"function\",\"function\":{\"name\":\"git_exec\"," \
    "\"description\":\"Execute a read-only git command (whitelisted: " \
    "status/diff/log/branch/show/ls-files/grep) and capture output\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"command_args\":" \
    "{\"type\":\"array\",\"items\":{\"type\":\"string\"}}," \
    "\"cwd\":{\"type\":\"string\"}},\"required\":[\"command_args\"]}}}"

#define AIRY_TOOL_JSON_GIT_DIFF \
    "{\"type\":\"function\",\"function\":{\"name\":\"git_diff\"," \
    "\"description\":\"Generate a unified diff for a path (git diff [--cached] [path])\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}," \
    "\"staged\":{\"type\":\"boolean\"}},\"required\":[]}}}"

#define AIRY_TOOL_JSON_GIT_APPLY \
    "{\"type\":\"function\",\"function\":{\"name\":\"git_apply\"," \
    "\"description\":\"Apply a unified diff to the working tree\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"patch\":{\"type\":\"string\"}," \
    "\"check_only\":{\"type\":\"boolean\"}},\"required\":[\"patch\"]}}}"

#define AIRY_TOOL_JSON_MATHS_EVAL \
    "{\"type\":\"function\",\"function\":{\"name\":\"maths_eval\"," \
    "\"description\":\"Evaluate a math expression precisely (arithmetic, " \
    "powers, factorial, sqrt/sin/cos/tan/ln/log10/log2/exp/abs/min/max/floor/ceil " \
    "etc.)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"expression\":" \
    "{\"type\":\"string\"}},\"required\":[\"expression\"]}}}"

#define AIRY_TOOL_JSON_MATHS_STATS \
    "{\"type\":\"function\",\"function\":{\"name\":\"maths_stats\"," \
    "\"description\":\"Compute numeric statistics (sum/mean/median/min/max/var/stddev)\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"}," \
    "\"values\":{\"type\":\"array\",\"items\":{\"type\":\"number\"}}}," \
    "\"required\":[\"op\",\"values\"]}}}"

#define AIRY_TOOL_JSON_MATHS_PLOT \
    "{\"type\":\"function\",\"function\":{\"name\":\"maths_plot\"," \
    "\"description\":\"Sample y=f(x) over [xmin,xmax] and return a " \
    "```plot block for TUI canvas rendering\"," \
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"expression\":" \
    "{\"type\":\"string\",\"description\":\"Expression in variable x, " \
    "e.g. sin(x)+x/2\"}," \
    "\"xmin\":{\"type\":\"number\"}," \
    "\"xmax\":{\"type\":\"number\"}," \
    "\"samples\":{\"type\":\"integer\",\"description\":\"Sample count, " \
    "default 128, max 256\"}}," \
    "\"required\":[\"expression\",\"xmin\",\"xmax\"]}}}"

/* 全量目录（16 工具，与 tool_d BTOOL 注册同序）。 */
#define AIRY_TOOLS_JSON_SOURCE \
    "[" AIRY_TOOL_JSON_FS_READ "," AIRY_TOOL_JSON_FS_WRITE "," AIRY_TOOL_JSON_FS_LIST \
    "," AIRY_TOOL_JSON_SHELL_RUN "," AIRY_TOOL_JSON_WEB_FETCH "," AIRY_TOOL_JSON_FS_GLOB \
    "," AIRY_TOOL_JSON_FS_GREP "," AIRY_TOOL_JSON_FS_EDIT "," AIRY_TOOL_JSON_FS_DELETE \
    "," AIRY_TOOL_JSON_WEB_SEARCH "," AIRY_TOOL_JSON_GIT_EXEC "," AIRY_TOOL_JSON_GIT_DIFF \
    "," AIRY_TOOL_JSON_GIT_APPLY "," AIRY_TOOL_JSON_MATHS_EVAL "," AIRY_TOOL_JSON_MATHS_STATS \
    "," AIRY_TOOL_JSON_MATHS_PLOT "]"

#endif /* AIRY_RT_TOOL_SCHEMA_H */
