// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_providers.c
 * @brief 四域 provider 注册辅助（阶段 2 统一扩展机制）。
 *
 * 薄封装：构造 airy_extension_t 并挂载到统一注册表。各域 vtable 契约
 * 见同目录 airy_{llm,tool,storage,sandbox}_provider.h。
 */

#include "airy_llm_provider.h"
#include "airy_sandbox_provider.h"
#include "airy_storage_provider.h"
#include "airy_tool_provider.h"

#include <string.h>

/* 通用注册：vtable 与 name 必须非空。 */
static airy_err_t airy_provider_register_common(airy_ext_domain_t domain, const char *name,
                                                const char *version, const void *vtable, void *impl,
                                                uint32_t flags)
{
    if (name == NULL || vtable == NULL) {
        return AIRY_ERR_NEG(AIRY_EINVAL);
    }

    airy_extension_t ext;
    __builtin_memset(&ext, 0, sizeof(ext));
    ext.domain = domain;
    ext.name = name;
    ext.version = version != NULL ? version : "0.0.0";
    ext.capabilities.flags = flags;
    ext.vtable = vtable;
    ext.impl = impl;
    return airy_ext_register(&ext);
}

/* ================================================================
 * LLM 域
 * ================================================================ */

airy_err_t airy_llm_provider_register(airy_llm_provider_t *provider)
{
    if (provider == NULL || provider->complete == NULL) {
        return AIRY_ERR_NEG(AIRY_EINVAL);
    }
    /* llm_d 为远程 daemon 后端：标记 REMOTE。 */
    return airy_provider_register_common(AIRY_EXT_DOMAIN_LLM, provider->name, provider->version,
                                         provider, provider->impl, AIRY_EXT_FLAG_REMOTE);
}

airy_err_t airy_llm_provider_unregister(const char *name)
{
    return airy_ext_unregister(AIRY_EXT_DOMAIN_LLM, name);
}

/* ================================================================
 * Tool 域
 * ================================================================ */

airy_err_t airy_tool_provider_register(airy_tool_provider_t *provider)
{
    if (provider == NULL || provider->execute == NULL) {
        return AIRY_ERR_NEG(AIRY_EINVAL);
    }
    /* tool_d 为远程 daemon 后端：标记 REMOTE。 */
    return airy_provider_register_common(AIRY_EXT_DOMAIN_TOOL, provider->name, provider->version,
                                         provider, provider->impl, AIRY_EXT_FLAG_REMOTE);
}

airy_err_t airy_tool_provider_unregister(const char *name)
{
    return airy_ext_unregister(AIRY_EXT_DOMAIN_TOOL, name);
}

/* ================================================================
 * Storage 域
 * ================================================================ */

airy_err_t airy_storage_provider_register(airy_storage_provider_t *provider)
{
    if (provider == NULL || provider->get == NULL || provider->set == NULL) {
        return AIRY_ERR_NEG(AIRY_EINVAL);
    }
    return airy_provider_register_common(AIRY_EXT_DOMAIN_STORAGE, provider->name,
                                         provider->version, provider, provider->impl,
                                         AIRY_EXT_FLAG_BUILTIN);
}

airy_err_t airy_storage_provider_unregister(const char *name)
{
    return airy_ext_unregister(AIRY_EXT_DOMAIN_STORAGE, name);
}

/* ================================================================
 * Sandbox 域
 * ================================================================ */

airy_err_t airy_sandbox_provider_register(airy_sandbox_provider_t *provider)
{
    if (provider == NULL || provider->is_available == NULL) {
        return AIRY_ERR_NEG(AIRY_EINVAL);
    }
    return airy_provider_register_common(AIRY_EXT_DOMAIN_SANDBOX, provider->name,
                                         provider->version, provider, provider->impl,
                                         AIRY_EXT_FLAG_BUILTIN);
}

airy_err_t airy_sandbox_provider_unregister(const char *name)
{
    return airy_ext_unregister(AIRY_EXT_DOMAIN_SANDBOX, name);
}
