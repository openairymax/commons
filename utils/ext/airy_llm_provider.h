/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_llm_provider.h
 * @brief LLM 域 provider vtable 契约（阶段 2 统一扩展机制）。
 *
 * LLM 域扩展以 JSON 字符串消息为统一传输形态（与 llm_d JSON-RPC 契合），
 * 兼容远程（llm_svc_adapter -> llm_d）与未来本地（in-process）实现。
 * 注册：airy_llm_provider_register() 挂载到统一注册表（domain=LLM）。
 *
 * Consumer 获取：airy_ext_get(AIRY_EXT_DOMAIN_LLM, "llm_d") -> vtable。
 */

#ifndef AIRY_RT_LLM_PROVIDER_H
#define AIRY_RT_LLM_PROVIDER_H

#include "airy_ext.h"

#ifdef __cplusplus
extern "C" {
#endif

struct airy_llm_provider;

typedef struct airy_llm_provider {
    const char *name;
    const char *version;
    void *impl; /* 实现实例数据（如 llm_svc_adapter_t*） */

    /* 同步补全：request_json -> out_response_json（调用方 AIRY_FREE） */
    airy_err_t (*complete)(struct airy_llm_provider *p, const char *request_json,
                           char **out_response_json);

    /* 流式补全：逐块回调 on_chunk；out_full_response 为拼接结果 */
    airy_err_t (*complete_stream)(struct airy_llm_provider *p, const char *request_json,
                                  int (*on_chunk)(const char *chunk, void *ud), void *ud,
                                  char **out_full_response);

    int (*is_connected)(struct airy_llm_provider *p);
} airy_llm_provider_t;

/** @brief 将 LLM provider 注册到统一注册表（AIRY_EXT_DOMAIN_LLM）。 */
airy_err_t airy_llm_provider_register(airy_llm_provider_t *provider);

/** @brief 注销（按 provider->name）。 */
airy_err_t airy_llm_provider_unregister(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_LLM_PROVIDER_H */
