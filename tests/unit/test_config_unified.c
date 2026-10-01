// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_config_unified.c
 * @brief 统一配置模块（config_unified）schema 默认值契约单元测试。
 *
 * 覆盖 config_schema_apply_defaults() 的根因修复：默认值须真正写入上下文
 * （此前构造后即销毁、静默失效，属桩实现）。断言：缺失键按类型填充、既有键
 * 不被覆盖、无默认值项保持缺失、非法参数 fail-closed、二次施加幂等，
 * 以及 config_service_create() 生命周期接线确实施加默认值。
 */

#include "config_service.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition, message)                                \
    do {                                                               \
        if (!(condition)) {                                            \
            fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
            return 1;                                                  \
        }                                                              \
    } while (0)

#define TEST_RUN(test_func)                                   \
    do {                                                      \
        printf("  [TEST] %s ... ", #test_func);               \
        if (test_func() != 0) {                               \
            fprintf(stderr, "test failed: %s\n", #test_func); \
            failed_tests++;                                   \
        } else {                                              \
            printf("PASS\n");                                 \
            passed_tests++;                                   \
        }                                                     \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

static config_schema_t *make_schema(void)
{
    config_schema_t *schema = config_schema_create("unit_schema");
    if (!schema)
        return NULL;

    config_schema_item_t items[] = {
        {.key = "enabled",
         .type = CONFIG_TYPE_BOOL,
         .required = false,
         .description = "开关",
         .default_value = "true"},
        {.key = "retries",
         .type = CONFIG_TYPE_INT,
         .required = false,
         .description = "重试次数",
         .default_value = "5"},
        {.key = "quota",
         .type = CONFIG_TYPE_INT64,
         .required = false,
         .description = "配额",
         .default_value = "1099511627776"},
        {.key = "ratio",
         .type = CONFIG_TYPE_DOUBLE,
         .required = false,
         .description = "比率",
         .default_value = "0.25"},
        {.key = "mode",
         .type = CONFIG_TYPE_STRING,
         .required = false,
         .description = "模式",
         .default_value = "yaml"},
        {.key = "noval",
         .type = CONFIG_TYPE_STRING,
         .required = false,
         .description = "无默认值",
         .default_value = NULL},
        {.key = "nullv",
         .type = CONFIG_TYPE_NULL,
         .required = false,
         .description = "空类型默认值",
         .default_value = "1"},
    };

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        if (config_schema_add_item(schema, &items[i]) != CONFIG_SUCCESS) {
            config_schema_destroy(schema);
            return NULL;
        }
    }

    return schema;
}

/* 缺失键须按声明类型真正填充默认值（修复前为静默 no-op）。 */
static int test_defaults_filled(void)
{
    config_schema_t *schema = make_schema();
    TEST_ASSERT(schema != NULL, "schema create");

    config_context_t *ctx = config_context_create("unit_ctx");
    TEST_ASSERT(ctx != NULL, "context create");

    TEST_ASSERT(config_schema_apply_defaults(schema, ctx) == CONFIG_SUCCESS, "apply defaults");

    TEST_ASSERT(config_context_has(ctx, "enabled"), "bool key present");
    TEST_ASSERT(config_value_get_bool(config_context_get(ctx, "enabled"), false) == true,
                "bool default value");

    TEST_ASSERT(config_context_has(ctx, "retries"), "int key present");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "retries"), -1) == 5,
                "int default value");

    TEST_ASSERT(config_context_has(ctx, "quota"), "int64 key present");
    TEST_ASSERT(config_value_get_int64(config_context_get(ctx, "quota"), -1) == 1099511627776LL,
                "int64 default value");

    TEST_ASSERT(config_context_has(ctx, "ratio"), "double key present");
    TEST_ASSERT(config_value_get_double(config_context_get(ctx, "ratio"), -1.0) == 0.25,
                "double default value");

    TEST_ASSERT(config_context_has(ctx, "mode"), "string key present");
    TEST_ASSERT(strcmp(config_value_get_string(config_context_get(ctx, "mode"), ""), "yaml") == 0,
                "string default value");

    config_context_destroy(ctx);
    config_schema_destroy(schema);
    return 0;
}

/* 既有键不得被默认值覆盖（默认值语义 = 缺失回退，非强制写值）。 */
static int test_existing_kept(void)
{
    config_schema_t *schema = make_schema();
    TEST_ASSERT(schema != NULL, "schema create");

    config_context_t *ctx = config_context_create("unit_ctx");
    TEST_ASSERT(ctx != NULL, "context create");

    config_value_t *preset = config_value_create_string("toml");
    TEST_ASSERT(preset != NULL, "preset value create");
    TEST_ASSERT(config_context_set(ctx, "mode", preset) == CONFIG_SUCCESS, "preset set");

    TEST_ASSERT(config_schema_apply_defaults(schema, ctx) == CONFIG_SUCCESS, "apply defaults");
    TEST_ASSERT(strcmp(config_value_get_string(config_context_get(ctx, "mode"), ""), "toml") == 0,
                "existing value preserved");

    config_context_destroy(ctx);
    config_schema_destroy(schema);
    return 0;
}

/* 无默认值项与不可构造类型（NULL）须保持缺失，且不视为错误。 */
static int test_absent_left(void)
{
    config_schema_t *schema = make_schema();
    TEST_ASSERT(schema != NULL, "schema create");

    config_context_t *ctx = config_context_create("unit_ctx");
    TEST_ASSERT(ctx != NULL, "context create");

    TEST_ASSERT(config_schema_apply_defaults(schema, ctx) == CONFIG_SUCCESS, "apply defaults");
    TEST_ASSERT(!config_context_has(ctx, "noval"), "no-default key stays absent");
    TEST_ASSERT(!config_context_has(ctx, "nullv"), "null-type key stays absent");

    config_context_destroy(ctx);
    config_schema_destroy(schema);
    return 0;
}

/* 二次施加须幂等：键数与取值不因重复调用而变化。 */
static int test_idempotent(void)
{
    config_schema_t *schema = make_schema();
    TEST_ASSERT(schema != NULL, "schema create");

    config_context_t *ctx = config_context_create("unit_ctx");
    TEST_ASSERT(ctx != NULL, "context create");

    TEST_ASSERT(config_schema_apply_defaults(schema, ctx) == CONFIG_SUCCESS, "first apply");
    TEST_ASSERT(config_schema_apply_defaults(schema, ctx) == CONFIG_SUCCESS, "second apply");

    TEST_ASSERT(config_context_has(ctx, "mode"), "mode present");
    TEST_ASSERT(strcmp(config_value_get_string(config_context_get(ctx, "mode"), ""), "yaml") == 0,
                "mode stable");
    TEST_ASSERT(!config_context_has(ctx, "noval"), "no-default still absent");

    config_context_destroy(ctx);
    config_schema_destroy(schema);
    return 0;
}

/* 非法参数 fail-closed。 */
static int test_invalid_args(void)
{
    config_schema_t *schema = make_schema();
    TEST_ASSERT(schema != NULL, "schema create");

    config_context_t *ctx = config_context_create("unit_ctx");
    TEST_ASSERT(ctx != NULL, "context create");

    TEST_ASSERT(config_schema_apply_defaults(NULL, ctx) == CONFIG_ERROR_INVALID_ARG, "null schema");
    TEST_ASSERT(config_schema_apply_defaults(schema, NULL) == CONFIG_ERROR_INVALID_ARG, "null ctx");

    config_context_destroy(ctx);
    config_schema_destroy(schema);
    return 0;
}

/* config_service_create() 生命周期须接线默认值施加。 */
static int test_service_wiring(void)
{
    config_schema_t *schema = make_schema();
    TEST_ASSERT(schema != NULL, "schema create");

    config_context_t *ctx = config_service_create("unit_service", schema, false, false);
    TEST_ASSERT(ctx != NULL, "service create");
    TEST_ASSERT(config_context_has(ctx, "enabled"), "service applied bool default");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "retries"), -1) == 5,
                "service applied int default");

    config_context_destroy(ctx);
    config_schema_destroy(schema);
    return 0;
}

int main(void)
{
    printf("agentrt/commons/config_unified schema 默认值单元测试\n");

    TEST_RUN(test_defaults_filled);
    TEST_RUN(test_existing_kept);
    TEST_RUN(test_absent_left);
    TEST_RUN(test_idempotent);
    TEST_RUN(test_invalid_args);
    TEST_RUN(test_service_wiring);

    printf("测试结果：%d 通过，%d 失败\n", passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
