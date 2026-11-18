/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_verify.h
 * @brief 完成判据（success criteria）wire 契约 SSoT（0.1.21 C-17）。
 *
 * 判据的执行方只有 tool_d：agent_d 在终局前经 daemon 间 RPC 调用
 * `tool.verify`，以只读方式复核产物；该方法不进入 gateway 工具目录，
 * 模型与外部客户端都无法把它当普通工具自行执行。
 *
 * 请求 {"criteria":[<item>...],"timeout_ms":n,"on_fail":"fail|report|continue"}
 * 应答 {"ok":bool,"total":n,"passed":n,"items":[<result>...],"error":"..."}
 * item   {"type":"cmd|file|service|rpc", ...}
 * result {"type":str,"expect":str,"actual":str,"pass":bool}
 *
 * 未满足的处置由调用方策略决定（on_fail 缺省 fail，即判任务失败）；
 * verifier 自身只如实回报逐项证据，不做状态裁决。
 */

#ifndef AIRY_RT_COMMONS_AIRY_VERIFY_H
#define AIRY_RT_COMMONS_AIRY_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 方法/端点 ---- */

#define AIRY_VERIFY_METHOD "verify"
#define AIRY_VERIFY_DAEMON "tool"
#define AIRY_VERIFY_DEFAULT_HOST "127.0.0.1"
#define AIRY_VERIFY_DEFAULT_TIMEOUT_MS 20000 /* 单次全量复核预算 */
#define AIRY_VERIFY_MAX_TIMEOUT_MS 60000
#define AIRY_VERIFY_MAX_ITEMS 16 /* 单次判据条数上限（防误用为脚本宿主） */

/* ---- 请求键 ---- */

#define AIRY_VK_CRITERIA "criteria"
#define AIRY_VK_TIMEOUT_MS "timeout_ms"
#define AIRY_VK_ON_FAIL "on_fail"

/* ---- 判据项键 ---- */

#define AIRY_VC_TYPE "type"
#define AIRY_VC_CMD "cmd"
#define AIRY_VC_PATH "path"
#define AIRY_VC_CONTAINS "contains"
#define AIRY_VC_HOST "host"
#define AIRY_VC_PORT "port"
#define AIRY_VC_DAEMON "daemon"
#define AIRY_VC_METHOD "method"

/* ---- 判据类型（wire 值） ---- */

#define AIRY_VC_T_CMD "cmd"         /* 命令退出码为 0 */
#define AIRY_VC_T_FILE "file"       /* 路径存在，可选内容包含 */
#define AIRY_VC_T_SERVICE "service" /* TCP 端口可连（服务在跑） */
#define AIRY_VC_T_RPC "rpc"         /* 只读 RPC 探测成功 */

/* ---- 应答键 ---- */

#define AIRY_VK_OK "ok"
#define AIRY_VK_TOTAL "total"
#define AIRY_VK_PASSED "passed"
#define AIRY_VK_ITEMS "items"
#define AIRY_VK_ERROR "error"
#define AIRY_VK_EXPECT "expect"
#define AIRY_VK_ACTUAL "actual"
#define AIRY_VK_PASS "pass"

/* ---- on_fail 取值（调用方策略，缺省 fail） ---- */

#define AIRY_VF_FAIL "fail"
#define AIRY_VF_REPORT "report"
#define AIRY_VF_CONTINUE "continue"

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMONS_AIRY_VERIFY_H */
