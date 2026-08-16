// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_airy_regex.c
 * @brief Unit tests for the airy_regex POSIX-ERE engine (used on Windows,
 *        unit-tested here on Linux by calling airy_re_* directly).
 */

#include "airy_regex.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, fmt, ...)                              \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FAIL %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
            failures++;                                    \
        }                                                  \
    } while (0)

static void test_match(const char *pat, const char *text, int expect)
{
    airy_regex_t re;
    if (airy_re_regcomp(&re, pat, REG_EXTENDED) != 0) {
        CHECK(0, "regcomp(%s)", pat);
        return;
    }
    int rc = airy_re_regexec(&re, text, 0, NULL, 0);
    CHECK((rc == 0) == (expect == 1), "match(%s, %s) rc=%d expect=%d", pat, text, rc, expect);
    airy_re_regfree(&re);
}

static void test_capture(const char *pat, const char *text, int g, int exp_so, int exp_eo)
{
    airy_regex_t re;
    airy_regmatch_t m[3];
    if (airy_re_regcomp(&re, pat, REG_EXTENDED) != 0) {
        CHECK(0, "regcomp(%s)", pat);
        return;
    }
    if (airy_re_regexec(&re, text, 3, m, 0) != 0) {
        CHECK(0, "regexec(%s, %s) no match", pat, text);
        airy_re_regfree(&re);
        return;
    }
    CHECK(m[g].rm_so == exp_so && m[g].rm_eo == exp_eo,
          "capture(%s, %s) g=%d got [%d,%d] want [%d,%d]", pat, text, g, m[g].rm_so, m[g].rm_eo,
          exp_so, exp_eo);
    airy_re_regfree(&re);
}

static void test_invalid(void)
{
    airy_regex_t re;
    CHECK(airy_re_regcomp(&re, "(unclosed", REG_EXTENDED) != 0, "unclosed group accepted");
    CHECK(airy_re_regcomp(&re, "[unclosed", REG_EXTENDED) != 0, "unclosed class accepted");
    CHECK(airy_re_regcomp(&re, "a{3,1}", REG_EXTENDED) != 0, "bad range accepted");
}

int main(void)
{
    /* literals / any / classes */
    test_match("abc", "xxabcxx", 1);
    test_match("abc", "xxabdxx", 0);
    test_match("a.c", "abc", 1);
    test_match("a.c", "ac", 0);
    test_match("[0-9]+", "abc123def", 1);
    test_match("[0-9]+", "abcdef", 0);
    test_match("[a-z]{3}", "abc", 1);
    test_match("[^>]*", "<h2>", 1);
    test_match("[^\"]+", "a\"b", 1);

    /* anchors */
    test_match("^abc", "abc", 1);
    test_match("^abc", "xabc", 0);
    test_match("abc$", "abc", 1);
    test_match("abc$", "abcx", 0);

    /* quantifiers: greedy / non-greedy captures */
    test_capture("(a*)", "aaa", 1, 0, 3);
    test_capture("(a*?)", "aaa", 1, 0, 0);
    test_capture("(a+?)", "aaa", 1, 0, 1);
    test_capture("(a?)", "aaa", 1, 0, 1);

    /* alternation */
    test_match("a|b", "b", 1);
    test_match("a|b", "c", 0);
    test_match("(cat|dog)s", "cats", 1);
    test_match("(cat|dog)s", "dogs", 1);
    test_match("(cat|dog)s", "cows", 0);

    /* escapes */
    test_match("a\\.b", "a.b", 1);
    test_match("a\\.b", "axb", 0);

    /* braces */
    test_match("a{2,3}", "aa", 1);
    test_match("a{2,3}", "aaaa", 1);
    test_match("a{2,3}", "a", 0);
    test_match("a{2}", "aa", 1);
    test_match("a{2}", "aaa", 1);
    test_match("a{1,3}", "a", 1);
    test_match("a{2,}", "aa", 1);
    test_match("a{2,}", "a", 0);

    /* web_search HTML extraction patterns */
    test_capture("<h2[^>]*><a[^>]*href=\"([^\"]+)\"[^>]*>(.*?)</a></h2>",
                 "<h2 class=\"r\"><a href=\"https://example.com\" x=1>Title</a></h2>", 1, 23, 42);
    test_capture("class=\"result__a\" href=\"([^\"]+)\"[^>]*>([^<]+)</a>",
                 "<a class=\"result__a\" href=\"http://x.com/y\">Hello World</a>", 1, 24, 38);

    /* web_search_extract multi-result capture */
    {
        airy_regex_t re;
        airy_regmatch_t m[3];
        const char *html =
            "<h2><a href=\"http://a.b/c\" x>First Result</a></h2>"
            "<h2><a href=\"http://d.e/f\" x>Second</a></h2>";
        CHECK(airy_re_regcomp(&re, "<h2[^>]*><a[^>]*href=\"([^\"]+)\"[^>]*>(.*?)</a></h2>",
                              REG_EXTENDED) == 0,
              "regcomp web pattern");
        if (failures == 0 || 1) {
            int rc = airy_re_regexec(&re, html, 3, m, 0);
            CHECK(rc == 0, "regexec web pattern rc=%d", rc);
            if (rc == 0) {
                CHECK(m[1].rm_so >= 0 && m[2].rm_so >= 0, "web captures set");
                if (m[1].rm_so >= 0) {
                    CHECK(m[1].rm_eo - m[1].rm_so == (int)strlen("http://a.b/c"),
                          "web url capture len");
                    CHECK(m[2].rm_eo - m[2].rm_so == (int)strlen("First Result"),
                          "web title capture len");
                }
            }
        }
        airy_re_regfree(&re);
    }

    /* fs_grep REG_NOSUB */
    {
        airy_regex_t re;
        CHECK(airy_re_regcomp(&re, "int[ \t]+main", REG_EXTENDED | REG_NOSUB) == 0,
              "regcomp grep pattern");
        CHECK(airy_re_regexec(&re, "int  main(void)", 0, NULL, 0) == 0, "grep match");
        airy_re_regfree(&re);
    }

    /* invalid patterns fail closed */
    test_invalid();

    if (failures == 0) {
        printf("test_airy_regex: ALL PASS\n");
        return 0;
    }
    printf("test_airy_regex: %d FAILURES\n", failures);
    return 1;
}
