/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_tool_provider.h
 * @brief Tool 域 provider vtable 契约（阶段 2 统一扩展机制）。
 *
 * Tool 域扩展以 JSON 字符串消息为统一传输形态（与 tool_d JSON-RPC 契合）。
 * 注册：airy_tool_provider_register() 挂载到统一注册表（domain=TOOL）。
 */

#ifndef AIRY_RT_TOOL_PROVIDER_H
#define AIRY_RT_TOOL_PROVIDER_H

#include "airy_ext.h"

#ifdef __cplusplus
extern "C" {
#endif

struct airy_tool_provider;

typedef struct airy_tool_provider {
    const char *name;
    const char *version;
    void *impl; /* 实现实例数据（如 tool_svc_adapter_t*） */

    /* 工具清单：out_tools_json（调用方 AIRY_FREE） */
    airy_err_t (*list)(struct airy_tool_provider *p, char **out_tools_json);

    /* 工具执行：request_json -> out_response_json */
    airy_err_t (*execute)(struct airy_tool_provider *p, const char *request_json,
                          char **out_response_json);

    /* 流式执行：逐块回调 on_chunk */
    airy_err_t (*execute_stream)(struct airy_tool_provider *p, const char *request_json,
                                 int (*on_chunk)(const char *chunk, void *ud), void *ud,
                                 char **out_full_response);
} airy_tool_provider_t;

/** @brief 将 Tool provider 注册到统一注册表（AIRY_EXT_DOMAIN_TOOL）。 */
airy_err_t airy_tool_provider_register(airy_tool_provider_t *provider);

/** @brief 注销（按 provider->name）。 */
airy_err_t airy_tool_provider_unregister(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_TOOL_PROVIDER_H */
