// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_config_unified.c
 * @brief 统一配置模块（config_unified）契约单元测试。
 *
 * 一、config_schema_apply_defaults() 根因修复：默认值须真正写入上下文
 * （此前构造后即销毁、静默失效，属桩实现）。断言：缺失键按类型填充、既有键
 * 不被覆盖、无默认值项保持缺失、非法参数 fail-closed、二次施加幂等，
 * 以及 config_service_create() 生命周期接线确实施加默认值。
 *
 * 二、config_parse_json() 拍平语义（经内存源公共路径加载）：对象展开为点分
 * 键、数组展开为 a.N 索引键、空容器与 null 落为空串，与 YAML SSoT 对齐；
 * 并覆盖四类缺陷回归——数组项解析失败不死循环、未闭合/截断转义 fail-closed、
 * int32 越界不再截断、数字解析与 locale 无关。
 */

#include "config_service.h"
#include "config_source.h"

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

/* 经内存源公共路径加载一段 JSON。返回上下文并回填解析错误码。 */
static config_context_t *load_json(const char *text, config_error_t *out_err)
{
    config_context_t *ctx = config_context_create("json_ctx");
    if (!ctx) {
        if (out_err)
            *out_err = CONFIG_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    config_memory_source_options_t opt = {
        .data = text, .data_len = strlen(text), .format = "json"};
    config_source_t *src = config_source_create_memory(&opt);
    if (!src) {
        config_context_destroy(ctx);
        if (out_err)
            *out_err = CONFIG_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    config_error_t err = config_source_load(src, ctx);
    config_source_destroy(src);
    if (out_err)
        *out_err = err;
    return ctx;
}

/* 嵌套对象须展平为 a.b.c 点分键，可经 config_context_get 读取。 */
static int test_json_nested_object(void)
{
    config_error_t err = CONFIG_SUCCESS;
    config_context_t *ctx = load_json("{\"a\":{\"b\":{\"c\":42}}}", &err);
    TEST_ASSERT(ctx != NULL, "context create");
    TEST_ASSERT(err == CONFIG_SUCCESS, "nested object parses");

    /* 修复前：数组位置的对象整体丢弃，嵌套对象不可读。 */
    TEST_ASSERT(config_context_has(ctx, "a.b.c"), "nested key flattened");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "a.b.c"), -1) == 42,
                "nested value readable");

    config_context_destroy(ctx);
    return 0;
}

/* 数组须展平为 索引键，逐元素可读。 */
static int test_json_array(void)
{
    config_error_t err = CONFIG_SUCCESS;
    config_context_t *ctx = load_json("{\"list\":[10,20,30]}", &err);
    TEST_ASSERT(ctx != NULL, "context create");
    TEST_ASSERT(err == CONFIG_SUCCESS, "array parses");

    TEST_ASSERT(config_context_has(ctx, "list.0"), "index 0 present");
    TEST_ASSERT(config_context_has(ctx, "list.1"), "index 1 present");
    TEST_ASSERT(config_context_has(ctx, "list.2"), "index 2 present");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "list.0"), -1) == 10, "index 0 value");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "list.2"), -1) == 30, "index 2 value");

    config_context_destroy(ctx);
    return 0;
}

/* 对象数组须展平为 a.N.k 复合键。 */
static int test_json_array_of_objects(void)
{
    config_error_t err = CONFIG_SUCCESS;
    const char *json = "{\"srv\":[{\"id\":1,\"name\":\"a\"},{\"id\":2,\"name\":\"b\"}]}";
    config_context_t *ctx = load_json(json, &err);
    TEST_ASSERT(ctx != NULL, "context create");
    TEST_ASSERT(err == CONFIG_SUCCESS, "array of objects parses");

    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "srv.0.id"), -1) == 1, "srv.0.id");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "srv.1.id"), -1) == 2, "srv.1.id");
    TEST_ASSERT(strcmp(config_value_get_string(config_context_get(ctx, "srv.1.name"), ""), "b") == 0,
                "srv.1.name");

    config_context_destroy(ctx);
    return 0;
}

/* 标量类型与取值范围：bool/null/double/int64 越界不得截断。 */
static int test_json_scalars(void)
{
    config_error_t err = CONFIG_SUCCESS;
    const char *json =
        "{\"b1\":true,\"b2\":false,\"n\":null,\"d\":3.5,\"big\":4294967296,\"neg\":-7}";
    config_context_t *ctx = load_json(json, &err);
    TEST_ASSERT(ctx != NULL, "context create");
    TEST_ASSERT(err == CONFIG_SUCCESS, "scalars parse");

    TEST_ASSERT(config_value_get_bool(config_context_get(ctx, "b1"), false) == true, "true");
    TEST_ASSERT(config_value_get_bool(config_context_get(ctx, "b2"), true) == false, "false");
    TEST_ASSERT(strcmp(config_value_get_string(config_context_get(ctx, "n"), "x"), "") == 0,
                "null becomes empty string");
    TEST_ASSERT(config_value_get_double(config_context_get(ctx, "d"), -1.0) == 3.5, "double");
    /* 2^32 超出 int32，修复前 atol 截断为 0；现须保留 int64 全值。 */
    TEST_ASSERT(config_value_get_int64(config_context_get(ctx, "big"), -1) == 4294967296LL,
                "int64 not truncated");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "neg"), 0) == -7, "negative int");

    config_context_destroy(ctx);
    return 0;
}

/* 空容器与 null 落为空字符串值，与 YAML SSoT 一致。 */
static int test_json_empty_containers(void)
{
    config_error_t err = CONFIG_SUCCESS;
    config_context_t *ctx = load_json("{\"e\":{},\"a\":[]}", &err);
    TEST_ASSERT(ctx != NULL, "context create");
    TEST_ASSERT(err == CONFIG_SUCCESS, "empty containers parse");

    TEST_ASSERT(config_context_has(ctx, "e"), "empty object key present");
    TEST_ASSERT(config_context_has(ctx, "a"), "empty array key present");
    TEST_ASSERT(strcmp(config_value_get_string(config_context_get(ctx, "e"), "x"), "") == 0,
                "empty object is empty string");
    TEST_ASSERT(strcmp(config_value_get_string(config_context_get(ctx, "a"), "x"), "") == 0,
                "empty array is empty string");

    config_context_destroy(ctx);
    return 0;
}

/* 顶层数组为合法 JSON 文档（修复前被入口拒绝）。 */
static int test_json_top_level_array(void)
{
    config_error_t err = CONFIG_SUCCESS;
    config_context_t *ctx = load_json("[1,2,3]", &err);
    TEST_ASSERT(ctx != NULL, "context create");
    TEST_ASSERT(err == CONFIG_SUCCESS, "top-level array accepted");

    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "0"), -1) == 1, "root index 0");
    TEST_ASSERT(config_value_get_int(config_context_get(ctx, "2"), -1) == 3, "root index 2");

    config_context_destroy(ctx);
    return 0;
}

/* 畸形输入须 fail-closed：数组项错误不死循环、语法错、未闭环/转义截断。 */
static int json_fails(const char *text)
{
    config_error_t err = CONFIG_SUCCESS;
    config_context_t *ctx = load_json(text, &err);
    int bad = (ctx == NULL) || (err != CONFIG_ERROR_PARSE);
    if (ctx)
        config_context_destroy(ctx);
    return bad;
}

static int test_json_fail_closed(void)
{
    /* 数组项非法：修复前 continue 未推进指针导致死循环。 */
    TEST_ASSERT(json_fails("{\"a\":[1,x]}") == 0, "bad array item fails closed");

    /* 缺冒号。 */
    TEST_ASSERT(json_fails("{\"a\" 1}") == 0, "missing colon fails closed");

    /* 未闭合字符串：修复前静默成功。 */
    TEST_ASSERT(json_fails("{\"a\":\"unterminated") == 0, "unterminated string fails closed");

    /* 截断 \u 转义：修复前静默丢弃。 */
    TEST_ASSERT(json_fails("{\"s\":\"\\u12") == 0, "truncated escape fails closed");

    return 0;
}

int main(void)
{
    printf("agentrt/commons/config_unified 单元测试\n");

    TEST_RUN(test_defaults_filled);
    TEST_RUN(test_existing_kept);
    TEST_RUN(test_absent_left);
    TEST_RUN(test_idempotent);
    TEST_RUN(test_invalid_args);
    TEST_RUN(test_service_wiring);
    TEST_RUN(test_json_nested_object);
    TEST_RUN(test_json_array);
    TEST_RUN(test_json_array_of_objects);
    TEST_RUN(test_json_scalars);
    TEST_RUN(test_json_empty_containers);
    TEST_RUN(test_json_top_level_array);
    TEST_RUN(test_json_fail_closed);

    printf("测试结果：%d 通过，%d 失败\n", passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
