// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_ext.c
 * @brief 统一扩展注册表实现（阶段 2 统一扩展机制）。
 *
 * 每域一个定长槽位数组（AIRY_EXT_MAX_PER_DOMAIN），注册时深拷贝
 * name/version；全局互斥锁保护注册/查询/遍历。注册期调用方负责
 * vtable/impl 生命周期覆盖注册期。
 */

#include "airy_ext.h"

#include "airy_memory.h"
#include "sync.h"

#include <string.h>

/* ================================================================
 * 注册表存储
 * ================================================================ */

typedef struct airy_ext_slot {
    airy_extension_t ext;
    int in_use;
} airy_ext_slot_t;

static airy_ext_slot_t g_registry[AIRY_EXT_DOMAIN_MAX][AIRY_EXT_MAX_PER_DOMAIN];
static sync_mutex_t g_lock = NULL;

/* ================================================================
 * 锁辅助
 * ================================================================ */

static void airy_ext_lock(void)
{
    if (g_lock == NULL) {
        /* 惰性创建：首次并发注册理论窗口极小且注册集中在启动期；
         * 双检锁 + 幂等 create 兜底。 */
        sync_mutex_t created = NULL;
        if (sync_mutex_create(&created, NULL) == SYNC_SUCCESS) {
            sync_mutex_t old = g_lock;
            if (old == NULL) {
                g_lock = created;
            } else {
                sync_mutex_free(created);
            }
        }
    }
    if (g_lock != NULL) {
        (void)sync_mutex_lock_ex(g_lock, NULL);
    }
}

static void airy_ext_unlock(void)
{
    if (g_lock != NULL) {
        (void)sync_mutex_unlock_ex(g_lock);
    }
}

/* ================================================================
 * 内部工具
 * ================================================================ */

static void airy_ext_slot_clear(airy_ext_slot_t *slot)
{
    if (slot->ext.name != NULL) {
        AIRY_FREE((void *)slot->ext.name);
        slot->ext.name = NULL;
    }
    if (slot->ext.version != NULL) {
        AIRY_FREE((void *)slot->ext.version);
        slot->ext.version = NULL;
    }
    slot->in_use = 0;
}

static airy_ext_slot_t *airy_ext_find(airy_ext_domain_t domain, const char *name)
{
    for (size_t i = 0; i < AIRY_EXT_MAX_PER_DOMAIN; i++) {
        airy_ext_slot_t *slot = &g_registry[domain][i];
        if (slot->in_use && slot->ext.name != NULL && strcmp(slot->ext.name, name) == 0) {
            return slot;
        }
    }
    return NULL;
}

/* ================================================================
 * 公开 API
 * ================================================================ */

airy_err_t airy_ext_register(const airy_extension_t *ext)
{
    if (ext == NULL || ext->name == NULL || ext->domain < 0 ||
        ext->domain >= AIRY_EXT_DOMAIN_MAX) {
        return AIRY_ERR_NEG(AIRY_EINVAL);
    }

    char *name_copy = AIRY_STRDUP(ext->name);
    if (name_copy == NULL) {
        return AIRY_ERR_NEG(AIRY_ENOMEM);
    }
    char *version_copy = AIRY_STRDUP(ext->version != NULL ? ext->version : "0.0.0");
    if (version_copy == NULL) {
        AIRY_FREE(name_copy);
        return AIRY_ERR_NEG(AIRY_ENOMEM);
    }

    airy_ext_lock();

    airy_ext_slot_t *slot = airy_ext_find(ext->domain, ext->name);
    if (slot != NULL) {
        /* 覆盖：释放旧深拷贝后写入新值 */
        airy_ext_slot_clear(slot);
    } else {
        slot = NULL;
        for (size_t i = 0; i < AIRY_EXT_MAX_PER_DOMAIN; i++) {
            if (!g_registry[ext->domain][i].in_use) {
                slot = &g_registry[ext->domain][i];
                break;
            }
        }
        if (slot == NULL) {
            AIRY_FREE(name_copy);
            AIRY_FREE(version_copy);
            airy_ext_unlock();
            return AIRY_ERR_NEG(AIRY_ENOSPC);
        }
    }

    __builtin_memset(&slot->ext, 0, sizeof(airy_extension_t));
    slot->ext.domain = ext->domain;
    slot->ext.name = name_copy;
    slot->ext.version = version_copy;
    slot->ext.capabilities = ext->capabilities;
    slot->ext.vtable = ext->vtable;
    slot->ext.impl = ext->impl;
    slot->in_use = 1;

    airy_ext_unlock();
    return AIRY_SUCCESS;
}

airy_err_t airy_ext_unregister(airy_ext_domain_t domain, const char *name)
{
    if (name == NULL || domain < 0 || domain >= AIRY_EXT_DOMAIN_MAX) {
        return AIRY_ERR_NEG(AIRY_EINVAL);
    }

    airy_ext_lock();
    airy_ext_slot_t *slot = airy_ext_find(domain, name);
    if (slot == NULL) {
        airy_ext_unlock();
        return AIRY_ERR_NEG(AIRY_ENOENT);
    }
    airy_ext_slot_clear(slot);
    airy_ext_unlock();
    return AIRY_SUCCESS;
}

const airy_extension_t *airy_ext_get(airy_ext_domain_t domain, const char *name)
{
    if (name == NULL || domain < 0 || domain >= AIRY_EXT_DOMAIN_MAX) {
        return NULL;
    }

    const airy_extension_t *result = NULL;
    airy_ext_lock();
    airy_ext_slot_t *slot = airy_ext_find(domain, name);
    if (slot != NULL) {
        result = &slot->ext;
    }
    airy_ext_unlock();
    return result;
}

size_t airy_ext_count(airy_ext_domain_t domain)
{
    if (domain < 0 || domain >= AIRY_EXT_DOMAIN_MAX) {
        return 0;
    }

    size_t count = 0;
    airy_ext_lock();
    for (size_t i = 0; i < AIRY_EXT_MAX_PER_DOMAIN; i++) {
        if (g_registry[domain][i].in_use) {
            count++;
        }
    }
    airy_ext_unlock();
    return count;
}

void airy_ext_foreach(airy_ext_domain_t domain, void (*fn)(const airy_extension_t *ext, void *ud),
                      void *ud)
{
    if (fn == NULL || domain < 0 || domain >= AIRY_EXT_DOMAIN_MAX) {
        return;
    }

    airy_ext_lock();
    for (size_t i = 0; i < AIRY_EXT_MAX_PER_DOMAIN; i++) {
        airy_ext_slot_t *slot = &g_registry[domain][i];
        if (slot->in_use) {
            fn(&slot->ext, ud);
        }
    }
    airy_ext_unlock();
}

void airy_ext_clear(airy_ext_domain_t domain)
{
    if (domain < 0 || domain >= AIRY_EXT_DOMAIN_MAX) {
        return;
    }

    airy_ext_lock();
    for (size_t i = 0; i < AIRY_EXT_MAX_PER_DOMAIN; i++) {
        airy_ext_slot_clear(&g_registry[domain][i]);
    }
    airy_ext_unlock();
}

const char *airy_ext_domain_name(airy_ext_domain_t domain)
{
    switch (domain) {
    case AIRY_EXT_DOMAIN_MEMORY:
        return "memory";
    case AIRY_EXT_DOMAIN_LLM:
        return "llm";
    case AIRY_EXT_DOMAIN_TOOL:
        return "tool";
    case AIRY_EXT_DOMAIN_STORAGE:
        return "storage";
    case AIRY_EXT_DOMAIN_SANDBOX:
        return "sandbox";
    default:
        return "UNKNOWN";
    }
}
