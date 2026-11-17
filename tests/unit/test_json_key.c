// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_json_key.c
 * @brief JSON 键定位机制件单元测试（0.1.21 D-5）：层级路径、字符串值内嵌
 *        键不误匹配、NOT_FOUND/EAMBIG 可区分、旧签名去歧义回退。
 */

#include "json_key.h"

#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* 断言路径命中并等于给定切片（含引号/括号原样比较）。 */
#define ASSERT_SLICE(json, path, want)                                       \
    do {                                                                     \
        const char *v_ = NULL;                                               \
        size_t n_ = 0;                                                       \
        TEST_ASSERT(airy_json_key_find((json), (path), &v_, &n_) ==          \
                        AIRY_JSON_OK,                                        \
                    "路径 " path " 应命中");                                 \
        TEST_ASSERT(n_ == strlen(want) && memcmp(v_, (want), n_) == 0,       \
                    "路径 " path " 取值不符");                               \
    } while (0)

static int test_top_level(void)
{
    const char *json = "{\"score\": 0.8, \"answer\": \"ok\"}";
    const char *p;

    ASSERT_SLICE(json, "score", "0.8");
    ASSERT_SLICE(json, "answer", "\"ok\"");
    p = airy_json_key(json, "score");
    TEST_ASSERT(p && *p == '0', "旧签名应返回 score 值首字符");
    return 0;
}

static int test_nested_same_name(void)
{
    const char *json = "{\"key\":\"outer\",\"a\":{\"key\":\"inner\"}}";
    const char *p;

    ASSERT_SLICE(json, "key", "\"outer\"");
    ASSERT_SLICE(json, "a.key", "\"inner\"");
    ASSERT_SLICE(json, "$.a.key", "\"inner\"");
    p = airy_json_key(json, "key");
    TEST_ASSERT(p && strncmp(p, "\"outer\"", 7) == 0,
                "同类键取顶层而非嵌套层");
    return 0;
}

static int test_embedded_key_in_string(void)
{
    const char *json =
        "{\"msg\":\"fake \\\"key\\\": 1\",\"key\":\"real\"}";

    ASSERT_SLICE(json, "key", "\"real\"");
    return 0;
}

static int test_missing_and_ambiguous(void)
{
    const char *json = "{\"a\":1}";
    const char *dup = "{\"key\":1,\"key\":2}";
    const char *v = NULL;

    TEST_ASSERT(airy_json_key_find(json, "b", &v, NULL) ==
                    AIRY_JSON_ENOTFOUND,
                "缺键应返回 ENOTFOUND");
    TEST_ASSERT(airy_json_key_find(dup, "key", &v, NULL) == AIRY_JSON_EAMBIG,
                "重复键应返回 EAMBIG");
    TEST_ASSERT(airy_json_key(dup, "key") == NULL,
                "旧签名遇重复键不得取首个");
    TEST_ASSERT(airy_json_key(json, "b") == NULL, "旧签名缺键返回 NULL");
    return 0;
}

static int test_array_path(void)
{
    const char *json = "{\"a\":{\"b\":[{\"c\":\"x\"},{\"c\":\"y\"}]}}";
    const char *v = NULL;

    ASSERT_SLICE(json, "a.b[1].c", "\"y\"");
    ASSERT_SLICE(json, "a.b[0]", "{\"c\":\"x\"}");
    TEST_ASSERT(airy_json_key_find(json, "a.b[2].c", &v, NULL) ==
                    AIRY_JSON_ENOTFOUND,
                "数组越界应返回 ENOTFOUND");
    TEST_ASSERT(airy_json_key_find(json, "a.b[x]", &v, NULL) ==
                    AIRY_JSON_EINVAL,
                "非法下标应返回 EINVAL");
    return 0;
}

static int test_bool(void)
{
    const char *json = "{\"think\":true,\"off\":false,\"n\":1}";
    int b = -1;

    TEST_ASSERT(airy_json_key_bool(json, "think", &b) == AIRY_JSON_OK && b == 1,
                "true 应解析为 1");
    TEST_ASSERT(airy_json_key_bool(json, "off", &b) == AIRY_JSON_OK && b == 0,
                "false 应解析为 0");
    TEST_ASSERT(airy_json_key_bool(json, "n", &b) == AIRY_JSON_ENOTBOOL,
                "非布尔字面量应返回 ENOTBOOL");
    TEST_ASSERT(airy_json_key_bool(json, "none", &b) ==
                    AIRY_JSON_ENOTFOUND,
                "缺键应返回 ENOTFOUND");
    return 0;
}

static int test_nested_objects(void)
{
    const char *json =
        "{\"msg\":\"}\",\"usage\":{\"prompt_tokens\":12,"
        "\"completion_tokens\":7},\"capacity\":{\"context_window\":128000}}";

    ASSERT_SLICE(json, "usage.prompt_tokens", "12");
    ASSERT_SLICE(json, "usage.completion_tokens", "7");
    ASSERT_SLICE(json, "capacity.context_window", "128000");
    TEST_ASSERT(airy_json_key_find(json, "usage", NULL, NULL) == AIRY_JSON_OK,
                "对象值整段可定位");
    ASSERT_SLICE(json, "usage", "{\"prompt_tokens\":12,\"completion_tokens\":7}");
    return 0;
}

static int test_malformed(void)
{
    const char *json = "{\"a\" 1}";
    const char *empty = "{}";
    const char *v = NULL;

    TEST_ASSERT(airy_json_key_find(json, "a", &v, NULL) == AIRY_JSON_EFORMAT,
                "缺冒号应返回 EFORMAT");
    TEST_ASSERT(airy_json_key_find(empty, "a", &v, NULL) ==
                    AIRY_JSON_ENOTFOUND,
                "空对象应返回 ENOTFOUND");
    TEST_ASSERT(airy_json_key_find(NULL, "a", &v, NULL) == AIRY_JSON_EINVAL,
                "NULL 文本应返回 EINVAL");
    TEST_ASSERT(airy_json_key_find(empty, "", &v, NULL) == AIRY_JSON_EINVAL,
                "空路径应返回 EINVAL");
    return 0;
}

int main(void)
{
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"top_level", test_top_level},
        {"nested_same_name", test_nested_same_name},
        {"embedded_key_in_string", test_embedded_key_in_string},
        {"missing_and_ambiguous", test_missing_and_ambiguous},
        {"array_path", test_array_path},
        {"bool", test_bool},
        {"nested_objects", test_nested_objects},
        {"malformed", test_malformed},
    };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        printf("🧪 Running %s...\n", tests[i].name);
        if (tests[i].fn() != 0) {
            fprintf(stderr, "✗Test failed: %s\n", tests[i].name);
            failed_tests++;
        } else {
            printf("✔PASS: %s\n", tests[i].name);
            passed_tests++;
        }
    }
    printf("\n结果: %d 通过, %d 失败\n", passed_tests, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
