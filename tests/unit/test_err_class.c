// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_err_class.c
 * @brief ErrorModel v1 错误分类 SSoT 单元测试
 *
 * 覆盖：类别名/解析往返 / 五类 golden / 符号归一 / 数值碰撞不入表 /
 * JSON 信封往返（含 ctx）/ diag 安全截断 / 缺省回退 / 非法参数。
 *
 * 依据：0.1.19 架构改进方案 §3.7.2（N1）。
 */

#include "err_class.h"

#include "error_codes.h"

#include <cjson/cJSON.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_name_parse(void)
{
    printf("  test_name_parse...\n");
    assert(strcmp(airy_err_class_name(AIRY_ERR_CLASS_OK), "ok") == 0);
    assert(strcmp(airy_err_class_name(AIRY_ERR_CLASS_TRANSPORT), "transport") == 0);
    assert(strcmp(airy_err_class_name(AIRY_ERR_CLASS_BUSINESS), "business") == 0);
    assert(strcmp(airy_err_class_name(AIRY_ERR_CLASS_POLICY), "policy") == 0);
    assert(strcmp(airy_err_class_name(AIRY_ERR_CLASS_CONFIG), "config") == 0);
    assert(strcmp(airy_err_class_name(AIRY_ERR_CLASS_INTERNAL), "internal") == 0);
    assert(strcmp(airy_err_class_name((airy_err_class_t)99), "unknown") == 0);

    assert(airy_err_class_parse("ok") == AIRY_ERR_CLASS_OK);
    assert(airy_err_class_parse("transport") == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_parse("business") == AIRY_ERR_CLASS_BUSINESS);
    assert(airy_err_class_parse("policy") == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_parse("config") == AIRY_ERR_CLASS_CONFIG);
    assert(airy_err_class_parse("internal") == AIRY_ERR_CLASS_INTERNAL);
    /* 安全默认：未知/NULL 绝不臆造 transport */
    assert(airy_err_class_parse("bogus") == AIRY_ERR_CLASS_BUSINESS);
    assert(airy_err_class_parse(NULL) == AIRY_ERR_CLASS_BUSINESS);
    printf("    PASSED\n");
}

static void test_class_of(void)
{
    printf("  test_class_of...\n");
    assert(airy_err_class_of(AIRY_OK) == AIRY_ERR_CLASS_OK);

    /* transport：网络/服务就绪/LLM 供给/协议校验/IPC */
    assert(airy_err_class_of(AIRY_ERR_TIMEOUT) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ECONNRESET) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ENOTCONN) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ETIMEDOUT) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ECONNREFUSED) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_SYS_SOCKET) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_SYS_PIPE) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_SVC_NOT_READY) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_SVC_BUSY) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_SVC_STOPPED) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_SVC_DEPENDENCY) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_SVC_HEALTH) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_LLM_NO_PROVIDER) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_LLM_PROVIDER_FAIL) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_LLM_RATE_LIMIT) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_PROTOCOL) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_CHECKSUM) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(AIRY_ERR_KERN_IPC) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(-AIRY_EIPC_MAGIC) == AIRY_ERR_CLASS_TRANSPORT);
    assert(airy_err_class_of(-AIRY_EIPC_FROZEN) == AIRY_ERR_CLASS_TRANSPORT);

    /* config：配置/环境/模型/鉴权 */
    assert(airy_err_class_of(AIRY_ERR_SVC_CONFIG) == AIRY_ERR_CLASS_CONFIG);
    assert(airy_err_class_of(AIRY_ERR_EXEC_ENV) == AIRY_ERR_CLASS_CONFIG);
    assert(airy_err_class_of(AIRY_ERR_LLM_INVALID_MODEL) == AIRY_ERR_CLASS_CONFIG);
    assert(airy_err_class_of(AIRY_ERR_LLM_AUTH_FAIL) == AIRY_ERR_CLASS_CONFIG);

    /* policy：权限/沙箱 + SEC/Cupolas 连续段 */
    assert(airy_err_class_of(AIRY_EPERM) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_EACCES) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_ERR_PERMISSION_DENIED) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_ERR_EXEC_SANDBOX) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_ERR_EXEC_PERMISSION) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_ERR_SEC_VIOLATION) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_ERR_SEC_PATH_TRAV) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_ERR_CUPOLAS_DENIED) == AIRY_ERR_CLASS_POLICY);
    assert(airy_err_class_of(AIRY_ERR_CUPOLAS_CERT_EXPIRED) == AIRY_ERR_CLASS_POLICY);

    /* internal：断言/未实现/内核非 IPC/内存损坏/Fault */
    assert(airy_err_class_of(AIRY_ERR_NULL_POINTER) == AIRY_ERR_CLASS_INTERNAL);
    assert(airy_err_class_of(AIRY_ERR_NOT_IMPLEMENTED) == AIRY_ERR_CLASS_INTERNAL);
    assert(airy_err_class_of(AIRY_ERR_SYS_DEADLOCK) == AIRY_ERR_CLASS_INTERNAL);
    assert(airy_err_class_of(AIRY_ERR_MEM_CORRUPT) == AIRY_ERR_CLASS_INTERNAL);
    assert(airy_err_class_of(AIRY_ERR_KERN_TASK) == AIRY_ERR_CLASS_INTERNAL);
    assert(airy_err_class_of(-AIRY_FAULT_CAP_FORGED) == AIRY_ERR_CLASS_INTERNAL);
    assert(airy_err_class_of(-AIRY_FAULT_VM_FAULT) == AIRY_ERR_CLASS_INTERNAL);

    /* business：泛用失败/业务码 + 数值碰撞段一律默认 */
    assert(airy_err_class_of(AIRY_ERR_NOT_FOUND) == AIRY_ERR_CLASS_BUSINESS);
    assert(airy_err_class_of(AIRY_ERR_IO) == AIRY_ERR_CLASS_BUSINESS);
    assert(airy_err_class_of(AIRY_ERR_GENERIC_FAIL) == AIRY_ERR_CLASS_BUSINESS);
    assert(airy_err_class_of(AIRY_ERR_EXEC_FAIL) == AIRY_ERR_CLASS_BUSINESS);
    assert(airy_err_class_of(AIRY_ERR_UNKNOWN) == AIRY_ERR_CLASS_BUSINESS);

    /* 符号归一：正幅值等价 */
    assert(airy_err_class_of(-AIRY_ERR_TIMEOUT) == AIRY_ERR_CLASS_TRANSPORT);
    printf("    PASSED\n");
}

static void test_json_roundtrip(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *ctx = cJSON_CreateObject();
    airy_err_class_t cls = AIRY_ERR_CLASS_OK;
    airy_err_t code = 0;
    char diag[128];
    const cJSON *octx = NULL;
    const cJSON *tool;

    printf("  test_json_roundtrip...\n");
    assert(root != NULL && ctx != NULL);
    cJSON_AddStringToObject(ctx, "tool", "fs_write");
    cJSON_AddStringToObject(ctx, "path", "/abs/a/b.html");

    assert(airy_err_json_put(root, AIRY_ERR_CLASS_BUSINESS, AIRY_ERR_IO,
                             "Cannot write file 'a/b.html': No such file",
                             ctx) == 0);
    cJSON_Delete(ctx);

    assert(airy_err_json_get(root, &cls, &code, diag, sizeof(diag), &octx) == 0);
    assert(cls == AIRY_ERR_CLASS_BUSINESS);
    assert(code == AIRY_ERR_IO);
    assert(strcmp(diag, "Cannot write file 'a/b.html': No such file") == 0);
    assert(octx != NULL);
    tool = cJSON_GetObjectItemCaseSensitive(octx, "tool");
    assert(cJSON_IsString(tool) && strcmp(tool->valuestring, "fs_write") == 0);

    cJSON_Delete(root);
    printf("    PASSED\n");
}

static void test_json_edges(void)
{
    cJSON *r = cJSON_CreateObject();
    cJSON *e = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    cJSON *empty = cJSON_CreateObject();
    cJSON *arr2;
    char d[4];
    const cJSON *octx = NULL;
    airy_err_class_t cls = AIRY_ERR_CLASS_OK;
    airy_err_t code = 0;

    printf("  test_json_edges...\n");

    /* put 非法 parent */
    assert(airy_err_json_put(NULL, AIRY_ERR_CLASS_BUSINESS, AIRY_ERR_IO, "x", NULL) == -1);
    assert(arr != NULL);
    assert(airy_err_json_put(arr, AIRY_ERR_CLASS_BUSINESS, AIRY_ERR_IO, "x", NULL) == -1);
    cJSON_Delete(arr);

    /* put 无 ctx / diag NULL → ""；get 空 diag 与无 ctx */
    assert(r != NULL);
    assert(airy_err_json_put(r, AIRY_ERR_CLASS_TRANSPORT, AIRY_ERR_SYS_SOCKET, NULL, NULL) == 0);
    assert(airy_err_json_get(r, NULL, NULL, d, sizeof(d), &octx) == 0);
    assert(strcmp(d, "") == 0);
    assert(octx == NULL);

    /* diag 安全截断（cap-1 加 NUL） */
    cJSON_Delete(r);
    r = cJSON_CreateObject();
    assert(r != NULL);
    assert(airy_err_json_put(r, AIRY_ERR_CLASS_INTERNAL, AIRY_ERR_NULL_POINTER,
                             "abcdefghij", NULL) == 0);
    assert(airy_err_json_get(r, NULL, NULL, d, sizeof(d), NULL) == 0);
    assert(strlen(d) == 3 && strcmp(d, "abc") == 0);
    cJSON_Delete(r);

    /* get 直传 err 对象（无 err 包裹） */
    assert(e != NULL);
    cJSON_AddStringToObject(e, "class", "policy");
    cJSON_AddNumberToObject(e, "code", -505);
    assert(airy_err_json_get(e, &cls, &code, NULL, 0, NULL) == 0);
    assert(cls == AIRY_ERR_CLASS_POLICY && code == -505);
    cJSON_Delete(e);

    /* get 非对象 / 无错误信封 */
    arr2 = cJSON_CreateArray();
    assert(arr2 != NULL);
    assert(airy_err_json_get(arr2, NULL, NULL, NULL, 0, NULL) == -1);
    cJSON_Delete(arr2);
    assert(empty != NULL);
    cJSON_AddStringToObject(empty, "result", "ok");
    assert(airy_err_json_get(empty, NULL, NULL, NULL, 0, NULL) == -1);
    cJSON_Delete(empty);

    printf("    PASSED\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  err_class Unit Tests\n");
    printf("=========================================\n");

    test_name_parse();
    test_class_of();
    test_json_roundtrip();
    test_json_edges();

    printf("\nAll err_class tests PASSED\n");
    return 0;
}
