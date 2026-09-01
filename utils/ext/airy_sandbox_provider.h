/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_sandbox_provider.h
 * @brief Sandbox 域 provider vtable 契约（阶段 2 统一扩展机制）。
 *
 * Sandbox 域扩展为能力描述型 provider（执行隔离由 cupolas workbench /
 * 各平台原生机制承担，此处暴露可用能力与描述，供上层决策）。注册：
 * airy_sandbox_provider_register() 挂载到统一注册表（domain=SANDBOX）。
 */

#ifndef AIRY_RT_SANDBOX_PROVIDER_H
#define AIRY_RT_SANDBOX_PROVIDER_H

#include "airy_ext.h"

#ifdef __cplusplus
extern "C" {
#endif

struct airy_sandbox_provider;

/** @brief 能力名：Landlock（Linux FS 规则）/ seccomp（Linux syscall 过滤）。 */
#define AIRY_SANDBOX_CAP_LANDLOCK "landlock"
#define AIRY_SANDBOX_CAP_SECCOMP "seccomp"

typedef struct airy_sandbox_provider {
    const char *name;
    const char *version;
    void *impl; /* 实现实例数据 */

    /* 能力探测：capability 为 AIRY_SANDBOX_CAP_*；1 = 可用，0 = 不可用 */
    int (*is_available)(struct airy_sandbox_provider *p, const char *capability);

    /* 能力描述（字符串字面量，无 free） */
    const char *(*describe)(struct airy_sandbox_provider *p);
} airy_sandbox_provider_t;

/** @brief 将 Sandbox provider 注册到统一注册表（AIRY_EXT_DOMAIN_SANDBOX）。 */
airy_err_t airy_sandbox_provider_register(airy_sandbox_provider_t *provider);

/** @brief 注销（按 provider->name）。 */
airy_err_t airy_sandbox_provider_unregister(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SANDBOX_PROVIDER_H */
