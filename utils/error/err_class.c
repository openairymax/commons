/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file err_class.c
 * @brief ErrorModel v1 错误分类唯一真相源实现。
 *
 * 分类判据以「无歧义语义段」为主、显式枚举为辅，其余一律默认 business：
 *   - 安全策略连续段 SEC(-701..-711) + Cupolas(-712..-726) → policy
 *   - [SC] Fault 码段(-4097..-4102)                          → internal
 *   - [SC] IPC 传输码段(-41..-53)                            → transport
 *   - 内核段(-200..-208)：仅 KERN_IPC 属传输，其余内部一致性
 *   - 其余显式枚举（网络/服务就绪/LLM 供给/协议校验等）
 *
 * 数值碰撞规避：airy_types.h 负 POSIX 与 error_codes.h 扩展码在 -1..-99
 * 段重叠，[SC] ECAP(71..84)/ECFG(101..105) 与 SYS 码同值——这些码一律
 * 不入显式表，落 default business，绝不臆造「服务不可达」。
 *
 * 依据：0.1.19 架构改进方案 §3.7.2 / §3.7.5（N1）。
 */

#include "err_class.h"

#include "error_codes.h"

#include <cjson/cJSON.h>
#include <compat.h>

#include <string.h>

const char *airy_err_class_name(airy_err_class_t cls)
{
    static const char *const names[] = {"ok",      "transport", "business",
                                        "policy",  "config",    "internal"};

    if ((int)cls < 0 || (int)cls >= (int)(sizeof(names) / sizeof(names[0])))
        return "unknown";
    return names[(int)cls];
}

airy_err_class_t airy_err_class_parse(const char *name)
{
    int i;

    if (name == NULL)
        return AIRY_ERR_CLASS_BUSINESS;

    for (i = 0; i <= (int)AIRY_ERR_CLASS_INTERNAL; ++i) {
        if (strcmp(name, airy_err_class_name((airy_err_class_t)i)) == 0)
            return (airy_err_class_t)i;
    }
    return AIRY_ERR_CLASS_BUSINESS;
}

typedef struct {
    airy_err_t code;
    airy_err_class_t cls;
} err_map_t;

static const err_map_t k_err_map[] = {
    {AIRY_ERR_TIMEOUT, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ECONNRESET, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ENOTCONN, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ETIMEDOUT, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ECONNREFUSED, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SYS_SOCKET, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SYS_PIPE, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SVC_NOT_READY, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SVC_BUSY, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SVC_STOPPED, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SVC_DEPENDENCY, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SVC_HEALTH, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SVC_LOADBALANCE, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_LLM_NO_PROVIDER, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_LLM_PROVIDER_FAIL, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_LLM_RATE_LIMIT, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_PROTOCOL, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_CHECKSUM, AIRY_ERR_CLASS_TRANSPORT},
    {AIRY_ERR_SVC_CONFIG, AIRY_ERR_CLASS_CONFIG},
    {AIRY_ERR_EXEC_ENV, AIRY_ERR_CLASS_CONFIG},
    {AIRY_ERR_LLM_INVALID_MODEL, AIRY_ERR_CLASS_CONFIG},
    {AIRY_ERR_LLM_AUTH_FAIL, AIRY_ERR_CLASS_CONFIG},
    {AIRY_EPERM, AIRY_ERR_CLASS_POLICY},
    {AIRY_EACCES, AIRY_ERR_CLASS_POLICY},
    {AIRY_ERR_PERMISSION_DENIED, AIRY_ERR_CLASS_POLICY},
    {AIRY_ERR_EXEC_SANDBOX, AIRY_ERR_CLASS_POLICY},
    {AIRY_ERR_EXEC_PERMISSION, AIRY_ERR_CLASS_POLICY},
    {AIRY_ERR_NULL_POINTER, AIRY_ERR_CLASS_INTERNAL},
    {AIRY_ERR_NOT_IMPLEMENTED, AIRY_ERR_CLASS_INTERNAL},
    {AIRY_ERR_SYS_DEADLOCK, AIRY_ERR_CLASS_INTERNAL},
    {AIRY_ERR_MEM_CORRUPT, AIRY_ERR_CLASS_INTERNAL},
};

static airy_err_class_t err_map_find(airy_err_t c)
{
    size_t i;

    for (i = 0; i < sizeof(k_err_map) / sizeof(k_err_map[0]); ++i) {
        if (k_err_map[i].code == c)
            return k_err_map[i].cls;
    }
    return AIRY_ERR_CLASS_BUSINESS;
}

airy_err_class_t airy_err_class_of(airy_err_t code)
{
    airy_err_t c = code > 0 ? -code : code;

    if (c == 0)
        return AIRY_ERR_CLASS_OK;

    if ((int)c <= (int)(-AIRY_FAULT_CAP_FORGED) &&
        (int)c >= (int)(-AIRY_FAULT_VM_FAULT))
        return AIRY_ERR_CLASS_INTERNAL;

    if (c <= AIRY_ERR_SEC_VIOLATION && c >= AIRY_ERR_CUPOLAS_CERT_EXPIRED)
        return AIRY_ERR_CLASS_POLICY;

    if ((int)c <= (int)(-AIRY_EIPC_MAGIC) && (int)c >= (int)(-AIRY_EIPC_FROZEN))
        return AIRY_ERR_CLASS_TRANSPORT;

    if (c <= AIRY_ERR_KERN_BASE && c >= AIRY_ERR_KERN_INTERRUPT)
        return c == AIRY_ERR_KERN_IPC ? AIRY_ERR_CLASS_TRANSPORT
                                      : AIRY_ERR_CLASS_INTERNAL;

    return err_map_find(c);
}

int airy_err_json_put(cJSON *parent, airy_err_class_t cls, airy_err_t code,
                      const char *diag, const cJSON *ctx)
{
    cJSON *obj;
    cJSON *dup;

    if (parent == NULL)
        return -1;
    if (!cJSON_IsObject(parent))
        return -1;

    obj = cJSON_CreateObject();
    if (obj == NULL)
        return -1;

    if (cJSON_AddStringToObject(obj, "class", airy_err_class_name(cls)) == NULL ||
        cJSON_AddNumberToObject(obj, "code", (double)code) == NULL ||
        cJSON_AddStringToObject(obj, "diag", diag != NULL ? diag : "") == NULL) {
        cJSON_Delete(obj);
        return -1;
    }

    if (ctx != NULL) {
        dup = cJSON_Duplicate(ctx, 1);
        if (dup == NULL) {
            cJSON_Delete(obj);
            return -1;
        }
        cJSON_AddItemToObject(obj, "ctx", dup);
    }

    if (cJSON_AddItemToObject(parent, "err", obj) == 0) {
        cJSON_Delete(obj);
        return -1;
    }
    return 0;
}

int airy_err_json_get(const cJSON *errobj, airy_err_class_t *out_cls,
                      airy_err_t *out_code, char *diag_buf, size_t diag_cap,
                      const cJSON **out_ctx)
{
    const cJSON *err;
    const cJSON *item;

    if (!cJSON_IsObject(errobj))
        return -1;

    err = cJSON_GetObjectItemCaseSensitive(errobj, "err");
    if (!cJSON_IsObject(err))
        err = errobj;

    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(err, "class")) &&
        !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(err, "code")))
        return -1;

    if (out_cls != NULL) {
        const cJSON *jc = cJSON_GetObjectItemCaseSensitive(err, "class");
        *out_cls = cJSON_IsString(jc) ? airy_err_class_parse(jc->valuestring)
                                      : AIRY_ERR_CLASS_BUSINESS;
    }

    if (out_code != NULL) {
        const cJSON *jn = cJSON_GetObjectItemCaseSensitive(err, "code");
        *out_code = cJSON_IsNumber(jn) ? (airy_err_t)jn->valueint
                                       : AIRY_ERR_UNKNOWN;
    }

    if (diag_buf != NULL && diag_cap > 0) {
        const cJSON *jd = cJSON_GetObjectItemCaseSensitive(err, "diag");
        const char *s =
            (cJSON_IsString(jd) && jd->valuestring != NULL) ? jd->valuestring : "";

        airy_strlcpy(diag_buf, s, diag_cap);
    }

    if (out_ctx != NULL) {
        item = cJSON_GetObjectItemCaseSensitive(err, "ctx");
        *out_ctx = cJSON_IsObject(item) ? item : NULL;
    }

    return 0;
}
