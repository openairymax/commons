// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_airy_ext.c
 * @brief 统一扩展注册表单元测试（阶段 2 统一扩展机制）。
 *
 * 验证：
 * - 注册 / 按 (domain,name) 查找 / 域内计数
 * - 深拷贝（注册后源字符串修改不影响注册条目）
 * - 覆盖注册（同 key 更新 vtable/impl）
 * - 注销 / 清空
 * - 域隔离（同域多个、异域互不影响）
 * - 四域 provider 注册辅助（LLM/tool/storage/sandbox vtable 契约）
 * - 错误路径（NULL / 越界域 / 不存在）
 */

#include "airy_ext.h"
#include "airy_llm_provider.h"
#include "airy_sandbox_provider.h"
#include "airy_storage_provider.h"
#include "airy_tool_provider.h"

#include <stdio.h>
#include <string.h>

/* Release 下 assert 无效：恒生效 CHECK 宏 */
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "CHECK FAILED at %s:%d: %s\n", __FILE__,    \
                    __LINE__, #cond);                                   \
            return 1;                                                   \
        }                                                               \
    } while (0)

/* ================================================================
 * 用例
 * ================================================================ */

static int test_register_get_deepcopy(void)
{
    airy_ext_clear(AIRY_EXT_DOMAIN_MEMORY);

    /* 深拷贝验证：源字符串用栈缓冲，注册后修改不影响注册条目 */
    char name[32];
    char version[16];
    snprintf(name, sizeof(name), "builtin");
    snprintf(version, sizeof(version), "0.1.2");

    int dummy_vtable = 0xABCD;
    int dummy_impl = 0x1234;
    airy_extension_t ext;
    __builtin_memset(&ext, 0, sizeof(ext));
    ext.domain = AIRY_EXT_DOMAIN_MEMORY;
    ext.name = name;
    ext.version = version;
    ext.capabilities.flags = AIRY_EXT_FLAG_BUILTIN;
    ext.vtable = &dummy_vtable;
    ext.impl = &dummy_impl;
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);

    /* 篡改源字符串：注册条目不受影响 */
    name[0] = 'X';
    version[0] = 'X';

    const airy_extension_t *got = airy_ext_get(AIRY_EXT_DOMAIN_MEMORY, "builtin");
    CHECK(got != NULL);
    CHECK(got->domain == AIRY_EXT_DOMAIN_MEMORY);
    CHECK(strcmp(got->name, "builtin") == 0);
    CHECK(strcmp(got->version, "0.1.2") == 0);
    CHECK(got->vtable == &dummy_vtable);
    CHECK(got->impl == &dummy_impl);
    CHECK(got->capabilities.flags == AIRY_EXT_FLAG_BUILTIN);
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_MEMORY) == 1);

    airy_ext_clear(AIRY_EXT_DOMAIN_MEMORY);
    return 0;
}

static int test_overwrite(void)
{
    airy_ext_clear(AIRY_EXT_DOMAIN_MEMORY);

    int v1 = 1, v2 = 2;
    airy_extension_t ext;
    __builtin_memset(&ext, 0, sizeof(ext));
    ext.domain = AIRY_EXT_DOMAIN_MEMORY;
    ext.name = "builtin";
    ext.vtable = &v1;
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);

    /* 覆盖：同 key 更新 vtable，count 不变 */
    ext.vtable = &v2;
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_MEMORY) == 1);
    const airy_extension_t *got = airy_ext_get(AIRY_EXT_DOMAIN_MEMORY, "builtin");
    CHECK(got != NULL);
    CHECK(got->vtable == &v2);

    airy_ext_clear(AIRY_EXT_DOMAIN_MEMORY);
    return 0;
}

static int test_unregister_and_clear(void)
{
    airy_ext_clear(AIRY_EXT_DOMAIN_TOOL);

    airy_extension_t ext;
    __builtin_memset(&ext, 0, sizeof(ext));
    ext.domain = AIRY_EXT_DOMAIN_TOOL;
    ext.name = "tool_a";
    ext.vtable = &ext;
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);
    ext.name = "tool_b";
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_TOOL) == 2);

    CHECK(airy_ext_unregister(AIRY_EXT_DOMAIN_TOOL, "tool_a") == AIRY_SUCCESS);
    CHECK(airy_ext_get(AIRY_EXT_DOMAIN_TOOL, "tool_a") == NULL);
    CHECK(airy_ext_get(AIRY_EXT_DOMAIN_TOOL, "tool_b") != NULL);
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_TOOL) == 1);

    /* 注销不存在的 key */
    CHECK(airy_ext_unregister(AIRY_EXT_DOMAIN_TOOL, "nope") == AIRY_ERR_NEG(AIRY_ENOENT));

    airy_ext_clear(AIRY_EXT_DOMAIN_TOOL);
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_TOOL) == 0);
    return 0;
}

static void collect_names(const airy_extension_t *e, void *ud)
{
    char *buf = (char *)ud;
    strcat(buf, e->name);
    strcat(buf, ";");
}

static int test_foreach_domain_isolation(void)
{
    airy_ext_clear(AIRY_EXT_DOMAIN_MEMORY);
    airy_ext_clear(AIRY_EXT_DOMAIN_LLM);

    airy_extension_t ext;
    __builtin_memset(&ext, 0, sizeof(ext));
    ext.domain = AIRY_EXT_DOMAIN_MEMORY;
    ext.name = "m1";
    ext.vtable = &ext;
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);
    ext.name = "m2";
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);

    ext.domain = AIRY_EXT_DOMAIN_LLM;
    ext.name = "l1";
    CHECK(airy_ext_register(&ext) == AIRY_SUCCESS);

    /* 域隔离：各域 count 独立 */
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_MEMORY) == 2);
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_LLM) == 1);
    CHECK(airy_ext_count(AIRY_EXT_DOMAIN_STORAGE) == 0);

    /* foreach 收集 memory 域 */
    char joined[64] = {0};
    airy_ext_foreach(AIRY_EXT_DOMAIN_MEMORY, collect_names, joined);
    CHECK(strcmp(joined, "m1;m2;") == 0);

    airy_ext_clear(AIRY_EXT_DOMAIN_MEMORY);
    airy_ext_clear(AIRY_EXT_DOMAIN_LLM);
    return 0;
}

/* ================================================================
 * 四域 provider 注册辅助
 * ================================================================ */

static airy_err_t fake_llm_complete(struct airy_llm_provider *p, const char *req, char **out)
{
    (void)p;
    (void)req;
    *out = NULL;
    return AIRY_SUCCESS;
}

static int test_provider_helpers(void)
{
    airy_ext_clear(AIRY_EXT_DOMAIN_LLM);
    airy_ext_clear(AIRY_EXT_DOMAIN_TOOL);
    airy_ext_clear(AIRY_EXT_DOMAIN_STORAGE);
    airy_ext_clear(AIRY_EXT_DOMAIN_SANDBOX);

    /* LLM 域 */
    struct airy_llm_provider llm = {"llm_d", "1.0.0", NULL, fake_llm_complete, NULL, NULL};
    CHECK(airy_llm_provider_register(&llm) == AIRY_SUCCESS);
    const airy_extension_t *e = airy_ext_get(AIRY_EXT_DOMAIN_LLM, "llm_d");
    CHECK(e != NULL);
    CHECK(e->vtable == &llm);
    CHECK(e->capabilities.flags & AIRY_EXT_FLAG_REMOTE);
    CHECK(airy_llm_provider_unregister("llm_d") == AIRY_SUCCESS);
    CHECK(airy_ext_get(AIRY_EXT_DOMAIN_LLM, "llm_d") == NULL);

    /* Tool 域 */
    struct airy_tool_provider tool = {"tool_d", "1.0.0", NULL, NULL, NULL, NULL};
    CHECK(airy_tool_provider_register(&tool) == AIRY_EINVAL); /* execute 缺失 */
    tool.execute = (airy_err_t(*)(struct airy_tool_provider *, const char *, char **))0x1;
    CHECK(airy_tool_provider_register(&tool) == AIRY_SUCCESS);
    CHECK(airy_ext_get(AIRY_EXT_DOMAIN_TOOL, "tool_d") != NULL);
    CHECK(airy_tool_provider_unregister("tool_d") == AIRY_SUCCESS);

    /* Storage 域 */
    struct airy_storage_provider st = {"file", "1.0.0", NULL, NULL, NULL, NULL, NULL};
    CHECK(airy_storage_provider_register(&st) == AIRY_EINVAL); /* get/set 缺失 */
    st.get = (airy_err_t(*)(struct airy_storage_provider *, const char *, char **))0x1;
    st.set = (airy_err_t(*)(struct airy_storage_provider *, const char *, const char *))0x1;
    CHECK(airy_storage_provider_register(&st) == AIRY_SUCCESS);
    CHECK(airy_ext_get(AIRY_EXT_DOMAIN_STORAGE, "file") != NULL);
    CHECK(airy_storage_provider_unregister("file") == AIRY_SUCCESS);

    /* Sandbox 域 */
    struct airy_sandbox_provider sb = {"native", "1.0.0", NULL, NULL, NULL};
    CHECK(airy_sandbox_provider_register(&sb) == AIRY_EINVAL); /* is_available 缺失 */
    sb.is_available = (int (*)(struct airy_sandbox_provider *, const char *))0x1;
    CHECK(airy_sandbox_provider_register(&sb) == AIRY_SUCCESS);
    CHECK(airy_ext_get(AIRY_EXT_DOMAIN_SANDBOX, "native") != NULL);
    CHECK(airy_sandbox_provider_unregister("native") == AIRY_SUCCESS);

    return 0;
}

static int test_error_paths(void)
{
    /* NULL ext / 空 name */
    CHECK(airy_ext_register(NULL) == AIRY_ERR_NEG(AIRY_EINVAL));
    airy_extension_t ext;
    __builtin_memset(&ext, 0, sizeof(ext));
    ext.domain = AIRY_EXT_DOMAIN_MEMORY;
    ext.name = NULL;
    ext.vtable = &ext;
    CHECK(airy_ext_register(&ext) == AIRY_ERR_NEG(AIRY_EINVAL));

    /* 越界域 */
    ext.name = "x";
    ext.domain = (airy_ext_domain_t)AIRY_EXT_DOMAIN_MAX;
    CHECK(airy_ext_register(&ext) == AIRY_ERR_NEG(AIRY_EINVAL));
    CHECK(airy_ext_get((airy_ext_domain_t)AIRY_EXT_DOMAIN_MAX, "x") == NULL);
    CHECK(airy_ext_unregister((airy_ext_domain_t)AIRY_EXT_DOMAIN_MAX, "x") ==
          AIRY_ERR_NEG(AIRY_EINVAL));

    /* 不存在查找 / 注销 */
    CHECK(airy_ext_get(AIRY_EXT_DOMAIN_MEMORY, "missing") == NULL);
    CHECK(airy_ext_unregister(AIRY_EXT_DOMAIN_MEMORY, "missing") == AIRY_ERR_NEG(AIRY_ENOENT));

    /* 域名 */
    CHECK(strcmp(airy_ext_domain_name(AIRY_EXT_DOMAIN_MEMORY), "memory") == 0);
    CHECK(strcmp(airy_ext_domain_name(AIRY_EXT_DOMAIN_LLM), "llm") == 0);
    CHECK(strcmp(airy_ext_domain_name(AIRY_EXT_DOMAIN_TOOL), "tool") == 0);
    CHECK(strcmp(airy_ext_domain_name(AIRY_EXT_DOMAIN_STORAGE), "storage") == 0);
    CHECK(strcmp(airy_ext_domain_name(AIRY_EXT_DOMAIN_SANDBOX), "sandbox") == 0);
    CHECK(strcmp(airy_ext_domain_name((airy_ext_domain_t)99), "UNKNOWN") == 0);

    return 0;
}

/* ================================================================
 * main
 * ================================================================ */

int main(void)
{
    printf("=== 统一扩展注册表（airy_ext）单元测试 ===\n");
    int rc = 0;
    rc |= test_register_get_deepcopy();
    rc |= test_overwrite();
    rc |= test_unregister_and_clear();
    rc |= test_foreach_domain_isolation();
    rc |= test_provider_helpers();
    rc |= test_error_paths();
    if (rc == 0) {
        printf("ALL PASS\n");
    } else {
        printf("FAILED\n");
    }
    return rc;
}
