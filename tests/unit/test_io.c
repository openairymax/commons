// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_io.c
 * @brief 文件增删改查（CRUD）跨平台往返单元测试。
 *
 * 验证 2.2.4 文件操作端到端闭环（coding 场景基础能力）：
 * - create：airy_io_write_file 写入
 * - read：airy_io_read_file 读回内容与长度
 * - update：覆盖写后读回新内容
 * - delete：airy_io_remove_file 删除
 * - 幂等：删除不存在的文件视为成功
 * - 目录：mkdir_p / ensure_dir / list_files / remove_dir_recursive
 * - 错误路径：NULL 参数
 */

#include "io.h"
#include "airy_memory.h"

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

#define TEST_DIR "airy_io_test_tmp"

/* ================================================================
 * 用例
 * ================================================================ */

static int test_file_crud_roundtrip(void)
{
    const char *path = TEST_DIR "/roundtrip.txt";
    const char *v1 = "hello, airymax rt";
    const char *v2 = "updated content with 中文";

    /* create */
    CHECK(airy_io_remove_file(path) == 0);
    CHECK(airy_io_write_file(path, v1, strlen(v1)) == 0);

    /* read */
    size_t len = 0;
    char *buf = airy_io_read_file(path, &len);
    CHECK(buf != NULL);
    CHECK(len == strlen(v1));
    CHECK(memcmp(buf, v1, len) == 0);
    airy_io_free_list(NULL, 0); /* 惰性释放语义不应崩溃（free_list(NULL) 安全） */
    memory_free(buf);

    /* update（覆盖写） */
    CHECK(airy_io_write_file(path, v2, strlen(v2)) == 0);
    buf = airy_io_read_file(path, &len);
    CHECK(buf != NULL);
    CHECK(len == strlen(v2));
    CHECK(memcmp(buf, v2, len) == 0);
    memory_free(buf);

    /* delete */
    CHECK(airy_io_remove_file(path) == 0);

    /* 幂等：删除不存在的文件视为成功 */
    CHECK(airy_io_remove_file(path) == 0);

    /* 删除后读不到 */
    CHECK(airy_io_read_file(path, NULL) == NULL);

    return 0;
}

static int test_file_crud_auto_len(void)
{
    const char *path = TEST_DIR "/auto_len.txt";
    const char *data = "no-explicit-len";

    CHECK(airy_io_write_file(path, data, (size_t)-1) == 0);
    size_t len = 0;
    char *buf = airy_io_read_file(path, &len);
    CHECK(buf != NULL);
    CHECK(len == strlen(data));
    CHECK(memcmp(buf, data, strlen(data)) == 0);
    memory_free(buf);
    CHECK(airy_io_remove_file(path) == 0);
    return 0;
}

static int test_dir_crud(void)
{
    const char *dir = TEST_DIR "/nested/a/b";

    /* mkdir_p 递归建目录 */
    CHECK(airy_io_mkdir_p(dir, 0755) == 0);
    /* 已存在视为成功 */
    CHECK(airy_io_mkdir_p(dir, 0755) == 0);

    /* ensure_dir 幂等 */
    CHECK(airy_io_ensure_dir(dir) == 0);

    /* 目录内写文件并列出 */
    char file[256];
    snprintf(file, sizeof(file), "%s/item.txt", dir);
    CHECK(airy_io_write_file(file, "x", 1) == 0);

    char **files = NULL;
    size_t count = 0;
    CHECK(airy_io_list_files(dir, &files, &count) == 0);
    CHECK(count == 1);
    CHECK(strcmp(files[0], "item.txt") == 0);
    airy_io_free_list(files, count);

    /* 递归删除整棵目录树 */
    CHECK(airy_io_remove_dir_recursive(TEST_DIR "/nested") == 0);
    /* 幂等：删除不存在的目录视为成功 */
    CHECK(airy_io_remove_dir_recursive(TEST_DIR "/nested") == 0);

    return 0;
}

static int test_error_paths(void)
{
    /* 空参（失败返回负错误码） */
    CHECK(airy_io_write_file(NULL, "x", 1) < 0);
    CHECK(airy_io_write_file("x", NULL, 1) < 0);
    CHECK(airy_io_read_file(NULL, NULL) == NULL);
    CHECK(airy_io_ensure_dir(NULL) < 0);
    CHECK(airy_io_remove_file(NULL) < 0);
    CHECK(airy_io_remove_file("") < 0);
    CHECK(airy_io_list_files(NULL, NULL, NULL) < 0);
    CHECK(airy_io_mkdir_p(NULL, 0) < 0);
    CHECK(airy_io_remove_dir_recursive(NULL) < 0);

    /* 不存在的目录 list 失败 */
    char **files = NULL;
    size_t count = 0;
    CHECK(airy_io_list_files(TEST_DIR "/does_not_exist", &files, &count) < 0);

    return 0;
}

/* ================================================================
 * main
 * ================================================================ */

int main(void)
{
    printf("=== 文件增删改查（io）单元测试 ===\n");
    int rc = 0;

    /* 清理并重建测试工作目录（幂等） */
    airy_io_remove_dir_recursive(TEST_DIR);
    if (airy_io_mkdir_p(TEST_DIR, 0755) != 0) {
        fprintf(stderr, "cannot create test dir\n");
        return 1;
    }

    rc |= test_file_crud_roundtrip();
    rc |= test_file_crud_auto_len();
    rc |= test_dir_crud();
    rc |= test_error_paths();

    /* 清理测试工作目录 */
    airy_io_remove_dir_recursive(TEST_DIR);

    if (rc == 0) {
        printf("ALL PASS\n");
    } else {
        printf("FAILED\n");
    }
    return rc;
}
