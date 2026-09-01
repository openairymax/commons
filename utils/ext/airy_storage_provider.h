/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_storage_provider.h
 * @brief Storage 域 provider vtable 契约（阶段 2 统一扩展机制）。
 *
 * Storage 域扩展提供键值持久化（字符串值）。内置实现候选：hall_store
 * （任务文件）、状态文件等。注册：airy_storage_provider_register() 挂载
 * 到统一注册表（domain=STORAGE）。
 */

#ifndef AIRY_RT_STORAGE_PROVIDER_H
#define AIRY_RT_STORAGE_PROVIDER_H

#include "airy_ext.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct airy_storage_provider;

typedef struct airy_storage_provider {
    const char *name;
    const char *version;
    void *impl; /* 实现实例数据 */

    /* 读取：out_value 由实现分配（调用方 AIRY_FREE）；key 不存在返回 AIRY_ENOENT */
    airy_err_t (*get)(struct airy_storage_provider *p, const char *key, char **out_value);

    /* 写入（覆盖） */
    airy_err_t (*set)(struct airy_storage_provider *p, const char *key, const char *value);

    /* 删除：key 不存在返回 AIRY_ENOENT */
    airy_err_t (*delete)(struct airy_storage_provider *p, const char *key);

    /* 列举：prefix 匹配（可为 NULL = 全部）；out_keys 为字符串数组
     * （调用方逐个 AIRY_FREE 后 AIRY_FREE(out_keys)） */
    airy_err_t (*list)(struct airy_storage_provider *p, const char *prefix, char ***out_keys,
                       size_t *out_count);
} airy_storage_provider_t;

/** @brief 将 Storage provider 注册到统一注册表（AIRY_EXT_DOMAIN_STORAGE）。 */
airy_err_t airy_storage_provider_register(airy_storage_provider_t *provider);

/** @brief 注销（按 provider->name）。 */
airy_err_t airy_storage_provider_unregister(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_STORAGE_PROVIDER_H */
