/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file airy_ext.h
 * @brief 统一扩展注册表（阶段 2 统一扩展机制）。
 *
 * 把 memory 域的"能力接缝"（provider vtable + 注册/获取）模式统一推广到
 * LLM / tool / storage / sandbox 四域：每个扩展以 (domain, name) 为唯一
 * 注册键，挂载域专有 vtable（Definition）+ 实现实例（Provider），消费方
 * 按 key 查找后经 vtable 调用（Consumer）。参考 dsh Cordis 三角色，但
 * 采用 agentrt 的 C vtable 形态。
 *
 * 设计约束：
 * - 注册表仅持有通用扩展头（domain/name/version/capabilities/vtable/impl），
 *   不感知各域 vtable 内部结构（域头各自定义）。
 * - 注册发生在启动/配置阶段；查询（get/count/foreach）可在运行期并发，
 *   注册表内部以互斥锁保护。
 * - name/version 在注册时深拷贝，调用方传入字符串可复用栈/字面量。
 * - foreach 的回调在注册表锁内执行，回调中禁止再调用本模块注册/注销 API。
 *
 * 域 vtable 契约头（同目录）：
 *   airy_llm_provider.h     LLM 域（complete / complete_stream / stats）
 *   airy_tool_provider.h    Tool 域（list / execute / execute_stream）
 *   airy_storage_provider.h Storage 域（get / set / delete / list）
 *   airy_sandbox_provider.h Sandbox 域（能力描述）
 * memory 域复用 atoms/memory/provider.h 的
 * airy_memory_provider_t 作为 vtable（首个接入域，验证范式）。
 */

#ifndef AIRY_RT_EXT_H
#define AIRY_RT_EXT_H

#include "airy_types.h" /* AIRY_SUCCESS / AIRY_E*（用户态负值码） */
#include "error.h"      /* airy_err_t / AIRY_ERR_NEG */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 扩展域
 * ================================================================ */

/**
 * @brief 扩展域枚举。
 *
 * 新增域需同时扩展 airy_ext_domain_name() 与注册表容量。
 */
typedef enum airy_ext_domain {
    AIRY_EXT_DOMAIN_MEMORY = 0,
    AIRY_EXT_DOMAIN_LLM,
    AIRY_EXT_DOMAIN_TOOL,
    AIRY_EXT_DOMAIN_STORAGE,
    AIRY_EXT_DOMAIN_SANDBOX,
    AIRY_EXT_DOMAIN_MAX
} airy_ext_domain_t;

/* ================================================================
 * 能力标记
 * ================================================================ */

/**
 * @brief 扩展能力标记。
 *
 * feature_bits 按域解释（各域 provider 头定义）；flags 为通用位。
 */
typedef struct airy_ext_capabilities {
    uint32_t feature_bits;
    uint32_t flags;
} airy_ext_capabilities_t;

#define AIRY_EXT_FLAG_BUILTIN 0x00000001u   /* 内置实现 */
#define AIRY_EXT_FLAG_REMOTE 0x00000002u    /* 远程/RPC 后端（daemon 代理） */
#define AIRY_EXT_FLAG_OPTIONAL 0x00000004u  /* 可选扩展（缺失不影响运行） */

/* ================================================================
 * 扩展条目
 * ================================================================ */

/**
 * @brief 注册条目（Definition 角色）。
 *
 * 注册时 name/version 被深拷贝；vtable 指向域专有 vtable（只读，
 * 调用方生命周期须覆盖注册期），impl 为实现实例数据。
 */
typedef struct airy_extension {
    airy_ext_domain_t domain;
    const char *name;
    const char *version;
    airy_ext_capabilities_t capabilities;
    const void *vtable; /* 域专有 vtable（如 airy_memory_provider_t*） */
    void *impl;         /* 实现实例数据 */
} airy_extension_t;

/** @brief 每域最大注册数（防御性上限）。 */
#define AIRY_EXT_MAX_PER_DOMAIN 16u

/* ================================================================
 * 注册表 API
 * ================================================================ */

/**
 * @brief 注册（或覆盖）一个扩展。
 *
 * (domain, name) 已存在时覆盖（释放旧 name/version 深拷贝，替换新值）。
 *
 * @param ext [in] 扩展条目（注册时深拷贝 name/version）
 * @return AIRY_SUCCESS / AIRY_EINVAL（非法参数或域越界）/ AIRY_EOVERFLOW（域容量满）
 *
 * @threadsafe yes（内部互斥锁）
 */
airy_err_t airy_ext_register(const airy_extension_t *ext);

/**
 * @brief 注销一个扩展。
 *
 * @param domain [in] 域
 * @param name [in] 注册 key
 * @return AIRY_SUCCESS / AIRY_ENOENT（不存在）
 *
 * @threadsafe yes
 */
airy_err_t airy_ext_unregister(airy_ext_domain_t domain, const char *name);

/**
 * @brief 按 (domain, name) 查找扩展。
 *
 * @return 内部条目指针（只读，注册表持有；注销后失效）或 NULL
 *
 * @threadsafe yes
 */
const airy_extension_t *airy_ext_get(airy_ext_domain_t domain, const char *name);

/**
 * @brief 域内已注册数量。
 *
 * @threadsafe yes
 */
size_t airy_ext_count(airy_ext_domain_t domain);

/**
 * @brief 域内遍历。
 *
 * @param fn [in] 回调（在注册表锁内执行；禁止在回调中调用本模块注册/注销 API）
 * @param ud [in] 回调透传数据
 *
 * @threadsafe yes
 */
void airy_ext_foreach(airy_ext_domain_t domain, void (*fn)(const airy_extension_t *ext, void *ud),
                      void *ud);

/**
 * @brief 清空域内全部扩展（释放深拷贝）。
 *
 * @threadsafe yes
 */
void airy_ext_clear(airy_ext_domain_t domain);

/**
 * @brief 域名（日志/诊断）。
 *
 * @return 字符串字面量；非法域返回 "UNKNOWN"
 */
const char *airy_ext_domain_name(airy_ext_domain_t domain);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_EXT_H */
