// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_string_utf8.c
 * @brief UTF-8 validation and sanitisation unit tests.
 *
 * Covers string_utf8_validate() (RFC 3629 conformance: overlong forms,
 * surrogate code points, values above U+10FFFF, truncated tails) and
 * string_utf8_sanitize() (replacement of invalid sequences with U+FFFD and
 * safe truncation when the destination buffer is too small).
 */

#include "airy_string.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("🧪 Running %s...\n", #test_func);              \
        if (test_func() != 0) {                                \
            fprintf(stderr, "✗Test failed: %s\n", #test_func); \
            failed_tests++;                                    \
        } else {                                               \
            printf("✔PASS: %s\n", #test_func);                 \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* U+FFFD REPLACEMENT CHARACTER. */
static const char kReplacement[3] = {(char)0xEF, (char)0xBF, (char)0xBD};

/**
 * @brief Valid UTF-8 sequences must be accepted.
 */
static int test_validate_accepts_valid(void)
{
    const char ascii[] = "Hello, World!";

    /* 中文「你好」：E4 BD A0 E5 A5 BD */
    const char chinese[] = {(char)0xE4, (char)0xBD, (char)0xA0,
                            (char)0xE5, (char)0xA5, (char)0xBD, '\0'};

    /* emoji U+1F600：F0 9F 98 80 */
    const char emoji[] = {(char)0xF0, (char)0x9F, (char)0x98, (char)0x80, '\0'};

    /* 边界码点 U+10FFFF：F4 8F BF BF */
    const char max_cp[] = {(char)0xF4, (char)0x8F, (char)0xBF, (char)0xBF, '\0'};

    /* 两字节下界 U+0080：C2 80 */
    const char min_2byte[] = {(char)0xC2, (char)0x80, '\0'};

    TEST_ASSERT(string_utf8_validate(ascii, strlen(ascii)), "ASCII should be valid");
    TEST_ASSERT(string_utf8_validate(chinese, sizeof(chinese) - 1), "Chinese should be valid");
    TEST_ASSERT(string_utf8_validate(emoji, sizeof(emoji) - 1), "Emoji should be valid");
    TEST_ASSERT(string_utf8_validate(max_cp, sizeof(max_cp) - 1), "U+10FFFF should be valid");
    TEST_ASSERT(string_utf8_validate(min_2byte, sizeof(min_2byte) - 1), "U+0080 should be valid");
    TEST_ASSERT(string_utf8_validate("", 0), "Empty string should be valid");

    return 0;
}

/**
 * @brief Overlong encodings must be rejected (RFC 3629).
 */
static int test_validate_rejects_overlong(void)
{
    /* C0 80：用两字节编码 ASCII NUL */
    const char overlong_2[] = {(char)0xC0, (char)0x80, '\0'};

    /* E0 80 80：用三字节编码 ASCII NUL */
    const char overlong_3[] = {(char)0xE0, (char)0x80, (char)0x80, '\0'};

    /* E0 9F BF：用三字节编码 U+07FF 以下的码点 */
    const char overlong_3b[] = {(char)0xE0, (char)0x9F, (char)0xBF, '\0'};

    /* F0 80 80 80：用四字节编码 ASCII NUL */
    const char overlong_4[] = {(char)0xF0, (char)0x80, (char)0x80, (char)0x80, '\0'};

    TEST_ASSERT(!string_utf8_validate(overlong_2, sizeof(overlong_2) - 1),
                "C0 80 is overlong and must be invalid");
    TEST_ASSERT(!string_utf8_validate(overlong_3, sizeof(overlong_3) - 1),
                "E0 80 80 is overlong and must be invalid");
    TEST_ASSERT(!string_utf8_validate(overlong_3b, sizeof(overlong_3b) - 1),
                "E0 9F BF is overlong and must be invalid");
    TEST_ASSERT(!string_utf8_validate(overlong_4, sizeof(overlong_4) - 1),
                "F0 80 80 80 is overlong and must be invalid");

    return 0;
}

/**
 * @brief Surrogate code points and out-of-range values must be rejected.
 */
static int test_validate_rejects_invalid_code_points(void)
{
    /* ED A0 80：U+D800 高代理 */
    const char surrogate_hi[] = {(char)0xED, (char)0xA0, (char)0x80, '\0'};

    /* ED BF BF：U+DFFF 低代理 */
    const char surrogate_lo[] = {(char)0xED, (char)0xBF, (char)0xBF, '\0'};

    /* F4 90 80 80：U+110000，超出 Unicode 范围 */
    const char above_max[] = {(char)0xF4, (char)0x90, (char)0x80, (char)0x80, '\0'};

    TEST_ASSERT(!string_utf8_validate(surrogate_hi, sizeof(surrogate_hi) - 1),
                "U+D800 surrogate must be invalid");
    TEST_ASSERT(!string_utf8_validate(surrogate_lo, sizeof(surrogate_lo) - 1),
                "U+DFFF surrogate must be invalid");
    TEST_ASSERT(!string_utf8_validate(above_max, sizeof(above_max) - 1),
                "U+110000 is above U+10FFFF and must be invalid");

    return 0;
}

/**
 * @brief Truncated and structurally malformed sequences must be rejected.
 */
static int test_validate_rejects_malformed(void)
{
    /* E4 B8：三字节序列被截断 */
    const char truncated_3[] = {(char)0xE4, (char)0xB8, '\0'};

    /* F0 9F 98：四字节序列被截断 */
    const char truncated_4[] = {(char)0xF0, (char)0x9F, (char)0x98, '\0'};

    /* 80：孤立续接字节 */
    const char lone_cont[] = {(char)0x80, '\0'};

    /* C2 41：续接字节位置出现 ASCII */
    const char bad_cont[] = {(char)0xC2, 'A', '\0'};

    /* FE：非法起始字节 */
    const char bad_lead[] = {(char)0xFE, '\0'};

    /* E4 41 42：三字节序列中间不是续接字节 */
    const char bad_inner[] = {(char)0xE4, 'A', 'B', '\0'};

    TEST_ASSERT(!string_utf8_validate(truncated_3, sizeof(truncated_3) - 1),
                "Truncated 3-byte sequence must be invalid");
    TEST_ASSERT(!string_utf8_validate(truncated_4, sizeof(truncated_4) - 1),
                "Truncated 4-byte sequence must be invalid");
    TEST_ASSERT(!string_utf8_validate(lone_cont, sizeof(lone_cont) - 1),
                "Lone continuation byte must be invalid");
    TEST_ASSERT(!string_utf8_validate(bad_cont, sizeof(bad_cont) - 1),
                "ASCII inside a 2-byte sequence must be invalid");
    TEST_ASSERT(!string_utf8_validate(bad_lead, sizeof(bad_lead) - 1),
                "0xFE is not a valid leading byte");
    TEST_ASSERT(!string_utf8_validate(bad_inner, sizeof(bad_inner) - 1),
                "Non-continuation byte inside a 3-byte sequence must be invalid");

    return 0;
}

/**
 * @brief Sanitising valid input must copy it through untouched.
 */
static int test_sanitize_passthrough(void)
{
    const char input[] = "ASCII text";
    char out[64];

    size_t written = string_utf8_sanitize(input, strlen(input), out, sizeof(out));

    TEST_ASSERT(written == strlen(input), "Valid input length must be preserved");
    TEST_ASSERT(strcmp(out, input) == 0, "Valid input must pass through unchanged");

    written = string_utf8_sanitize(input, 0, out, sizeof(out));
    TEST_ASSERT(written == 0, "Zero-length input yields zero output");
    TEST_ASSERT(out[0] == '\0', "Zero-length input yields empty string");

    return 0;
}

/**
 * @brief Invalid bytes must be replaced by U+FFFD.
 */
static int test_sanitize_replaces_invalid(void)
{
    char out[64];

    /* 'A' 0xFF 'B'：单字节非法值 */
    const char input[] = {'A', (char)0xFF, 'B', '\0'};
    size_t written = string_utf8_sanitize(input, 3, out, sizeof(out));

    TEST_ASSERT(written == 5, "One invalid byte expands to a 3-byte replacement");
    TEST_ASSERT(out[0] == 'A', "Byte before the invalid value is preserved");
    TEST_ASSERT(memcmp(out + 1, kReplacement, sizeof(kReplacement)) == 0,
                "Invalid byte must become U+FFFD");
    TEST_ASSERT(out[4] == 'B', "Byte after the invalid value is preserved");
    TEST_ASSERT(string_utf8_validate(out, written), "Sanitised output must be valid UTF-8");

    /* 截断序列 */
    const char truncated[] = {(char)0xE4, (char)0xB8, '\0'};
    written = string_utf8_sanitize(truncated, sizeof(truncated) - 1, out, sizeof(out));
    TEST_ASSERT(string_utf8_validate(out, written), "Truncated input must sanitise to valid UTF-8");
    TEST_ASSERT(written >= sizeof(kReplacement), "Truncated input must produce a replacement");

    /* overlong 与代理区同样被清洗为合法 UTF-8 */
    const char overlong[] = {(char)0xC0, (char)0x80, '\0'};
    written = string_utf8_sanitize(overlong, sizeof(overlong) - 1, out, sizeof(out));
    TEST_ASSERT(string_utf8_validate(out, written), "Overlong input must sanitise to valid UTF-8");

    const char surrogate[] = {(char)0xED, (char)0xA0, (char)0x80, '\0'};
    written = string_utf8_sanitize(surrogate, sizeof(surrogate) - 1, out, sizeof(out));
    TEST_ASSERT(string_utf8_validate(out, written), "Surrogate input must sanitise to valid UTF-8");

    return 0;
}

/**
 * @brief Sanitising must never overflow a small destination buffer.
 */
static int test_sanitize_truncates_safely(void)
{
    /* 'A' 0xFF 'B' 展开为 5 字节，容器不足时必须安全截断 */
    const char input[] = {'A', (char)0xFF, 'B', '\0'};
    char out[4];

    memset(out, (char)0xAA, sizeof(out));
    size_t written = string_utf8_sanitize(input, 3, out, sizeof(out));

    TEST_ASSERT(written < sizeof(out), "Written length must leave room for the terminator");
    TEST_ASSERT(out[written] == '\0', "Output must be NUL-terminated inside the buffer");
    TEST_ASSERT(written == 1, "Only the leading ASCII byte fits before the replacement");
    TEST_ASSERT(out[0] == 'A', "Leading byte is preserved on truncation");

    /* 容量 3 时连一个替换字符都放不下（需要 3 字节 + NUL） */
    char tiny[3];
    memset(tiny, (char)0xAA, sizeof(tiny));
    written = string_utf8_sanitize(input, 3, tiny, sizeof(tiny));
    TEST_ASSERT(written == 1, "Replacement that does not fit must be skipped");
    TEST_ASSERT(tiny[1] == '\0', "Tiny buffer still NUL-terminated");

    return 0;
}

/**
 * @brief Argument edges must be handled without crashing.
 */
static int test_sanitize_argument_edges(void)
{
    char out[8];

    TEST_ASSERT(string_utf8_sanitize(NULL, 4, out, sizeof(out)) == 0, "NULL input yields 0");
    TEST_ASSERT(string_utf8_sanitize("abc", 3, NULL, sizeof(out)) == 0, "NULL output yields 0");
    TEST_ASSERT(string_utf8_sanitize("abc", 3, out, 0) == 0, "Zero capacity yields 0");

    return 0;
}

int main(void)
{
    printf("=== UTF-8 validation and sanitisation tests ===\n");

    TEST_RUN(test_validate_accepts_valid);
    TEST_RUN(test_validate_rejects_overlong);
    TEST_RUN(test_validate_rejects_invalid_code_points);
    TEST_RUN(test_validate_rejects_malformed);
    TEST_RUN(test_sanitize_passthrough);
    TEST_RUN(test_sanitize_replaces_invalid);
    TEST_RUN(test_sanitize_truncates_safely);
    TEST_RUN(test_sanitize_argument_edges);

    printf("\n=== Results: %d passed, %d failed ===\n", passed_tests, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
