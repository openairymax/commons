// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file json_key.h
 * @brief JSON 键定位机制件（全树唯一实现，无 cJSON 依赖）。
 *
 * 供未强制依赖 cJSON 的模块在解析外部或不可信 JSON 文本时定位对象成员
 * 值的位置，取代各模块自研的 strstr 键定位副本（Unify Design SSoT）。
 *
 * 0.1.21 D-5：定位器按 JSON 语法逐层扫描——字符串值按引号与转义整体跳过，
 * 因此值域内嵌的 `"key":` 不会被误认成键；路径支持 `a.b[0].c`；同一层
 * 同名键出现多次返回可区分的 EAMBIG，不再静默取首个。仍非完整解析器：
 * 不校验数字/转义合法性，仅保证结构边界正确。
 *
 * 判据（§19.1 D-5）：同名嵌套键取对层；字符串内嵌 `"key":` 不误匹配；
 * 键不存在返回 NOT_FOUND 而非空串；多处命中报错。
 */

#ifndef AIRY_RT_STRING_JSON_KEY_H
#define AIRY_RT_STRING_JSON_KEY_H

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 定位结果码：0 成功；其余为可区分的失败原因。 */
typedef enum airy_json_rc {
    AIRY_JSON_OK = 0,
    AIRY_JSON_EINVAL = 1,    /**< 入参非法（NULL/空路径/路径语法错） */
    AIRY_JSON_EFORMAT = 2,   /**< 文本不是合法 JSON 结构（截断/缺冒号等） */
    AIRY_JSON_ENOTFOUND = 3, /**< 路径在文本中不存在 */
    AIRY_JSON_EAMBIG = 4,    /**< 同一层同名键出现多次，归属不唯一 */
    AIRY_JSON_ENOTBOOL = 5   /**< 命中值不是 true/false 字面量 */
} airy_json_rc;

static inline const char *airy_json_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

/* 从字符串起始引号跳到闭合引号之后；未闭合或转义截断返回 NULL。 */
static inline const char *airy_json_str_end(const char *p)
{
    if (!p || *p != '"')
        return NULL;
    for (p++; *p; p++) {
        if (*p == '\\') {
            if (!p[1])
                return NULL;
            p++;
            continue;
        }
        if (*p == '"')
            return p + 1;
    }
    return NULL;
}

/* 容器/标量的边界字符：逗号、闭合括号或空白。 */
static inline int airy_json_stop_char(char c)
{
    return c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\n' ||
           c == '\r';
}

/* 从 '{'/'[' 跳到匹配的闭合括号之后；未闭合返回 NULL。 */
static inline const char *airy_json_cont_end(const char *p)
{
    char close = (*p == '{') ? '}' : ']';
    int depth = 0;

    for (const char *s = p; *s; s++) {
        if (*s == '"') {
            s = airy_json_str_end(s);
            if (!s)
                return NULL;
            s--;
            continue;
        }
        if (*s == *p) {
            depth++;
        } else if (*s == close && --depth == 0) {
            return s + 1;
        }
    }
    return NULL;
}

/* 返回值的结束位置（不含尾随空白）；标量止于分隔符或空白。 */
static inline const char *airy_json_val_end(const char *p)
{
    const char *s;

    if (!p)
        return NULL;
    if (*p == '"')
        return airy_json_str_end(p);
    if (*p == '{' || *p == '[')
        return airy_json_cont_end(p);
    for (s = p; *s; s++) {
        if (airy_json_stop_char(*s))
            break;
    }
    return s == p ? NULL : s;
}

/* 解析一个 "key": value 成员；pp 指向起始引号，成功时推进到成员之后。 */
static inline int airy_json_member(const char **pp, const char **keyp, size_t *klenp,
                                   const char **valp, const char **endp)
{
    const char *p = *pp;

    if (*p != '"')
        return AIRY_JSON_EFORMAT;
    const char *ke = airy_json_str_end(p);
    if (!ke)
        return AIRY_JSON_EFORMAT;
    const char *v = airy_json_ws(ke);
    if (*v != ':')
        return AIRY_JSON_EFORMAT;
    v = airy_json_ws(v + 1);
    const char *ve = airy_json_val_end(v);
    if (!ve)
        return AIRY_JSON_EFORMAT;
    *keyp = p + 1;
    *klenp = (size_t)(ke - p - 2);
    *valp = v;
    *endp = ve;
    return AIRY_JSON_OK;
}

static inline int airy_json_key_eq(const char *kp, size_t cur_len, const char *key, size_t klen)
{
    return cur_len == klen && memcmp(kp, key, klen) == 0;
}

/* 在对象内按成员名取唯一命中；重复键报 EAMBIG，缺键报 ENOTFOUND。 */
static inline int airy_json_obj_get(const char *obj, const char *key, size_t klen,
                                    const char **out_ptr, size_t *out_len)
{
    const char *p;
    const char *hit = NULL;
    size_t hit_len = 0;

    if (!obj || *obj != '{' || !key)
        return AIRY_JSON_EINVAL;
    p = airy_json_ws(obj + 1);
    if (*p == '}')
        return AIRY_JSON_ENOTFOUND;
    for (;;) {
        const char *kp;
        const char *v;
        const char *ve;
        size_t cur_len;

        int rc = airy_json_member(&p, &kp, &cur_len, &v, &ve);
        if (rc != AIRY_JSON_OK)
            return rc;
        if (airy_json_key_eq(kp, cur_len, key, klen)) {
            if (hit)
                return AIRY_JSON_EAMBIG;
            hit = v;
            hit_len = (size_t)(ve - v);
        }
        p = airy_json_ws(ve);
        if (*p == ',') {
            p = airy_json_ws(p + 1);
            continue;
        }
        if (*p == '}')
            break;
        return AIRY_JSON_EFORMAT;
    }
    if (!hit)
        return AIRY_JSON_ENOTFOUND;
    if (out_ptr)
        *out_ptr = hit;
    if (out_len)
        *out_len = hit_len;
    return AIRY_JSON_OK;
}

/* 数组下标取值；越界报 ENOTFOUND。 */
static inline int airy_json_arr_get(const char *arr, size_t idx,
                                    const char **out_ptr, size_t *out_len)
{
    const char *p;
    size_t i = 0;

    if (!arr || *arr != '[')
        return AIRY_JSON_EINVAL;
    p = airy_json_ws(arr + 1);
    if (*p == ']')
        return AIRY_JSON_ENOTFOUND;
    for (;;) {
        const char *ve = airy_json_val_end(p);

        if (!ve)
            return AIRY_JSON_EFORMAT;
        if (i == idx) {
            if (out_ptr)
                *out_ptr = p;
            if (out_len)
                *out_len = (size_t)(ve - p);
            return AIRY_JSON_OK;
        }
        i++;
        p = airy_json_ws(ve);
        if (*p == ',') {
            p = airy_json_ws(p + 1);
            continue;
        }
        if (*p == ']')
            return AIRY_JSON_ENOTFOUND;
        return AIRY_JSON_EFORMAT;
    }
}

static inline int airy_json_find_init(const char *json, const char *path, const char **curp)
{
    if (!json || !path || !*path)
        return AIRY_JSON_EINVAL;
    *curp = airy_json_ws(json);
    return AIRY_JSON_OK;
}

/* 返回 1=取到下一段，0=路径结束，-1=路径语法错。 */
static inline int airy_json_seg_next(const char **pathp, const char **segp, size_t *seg_lenp)
{
    while (**pathp == '$' || **pathp == '.')
        (*pathp)++;
    if (!**pathp)
        return 0;
    const char *seg = *pathp;
    while (**pathp && **pathp != '.' && **pathp != '[')
        (*pathp)++;
    *segp = seg;
    *seg_lenp = (size_t)(*pathp - seg);
    if (*seg_lenp == 0)
        return -1;
    return 1;
}

static inline int airy_json_arr_step(const char **pathp, size_t *idxp)
{
    const char *path = *pathp;
    size_t idx = 0;

    path++;
    if (!isdigit((unsigned char)*path))
        return AIRY_JSON_EINVAL;
    while (isdigit((unsigned char)*path)) {
        idx = idx * 10 + (size_t)(*path - '0');
        path++;
    }
    if (*path != ']')
        return AIRY_JSON_EINVAL;
    *pathp = path + 1;
    *idxp = idx;
    return AIRY_JSON_OK;
}

static inline int airy_json_resolve(const char *cur, size_t cur_len, const char **out_ptr,
                                    size_t *out_len)
{
    if (cur_len == 0) {
        const char *ve = airy_json_val_end(cur);

        if (!ve)
            return AIRY_JSON_EFORMAT;
        cur_len = (size_t)(ve - cur);
    }
    if (out_ptr)
        *out_ptr = cur;
    if (out_len)
        *out_len = cur_len;
    return AIRY_JSON_OK;
}

/**
 * @brief 按路径定位值，返回值切片（不写终止符）。
 *
 * 路径语法：`name`、`a.b`、`a.b[0].c`，可带前导 `$` 或 `.`。字符串值的
 * 切片含首尾引号；对象/数组含完整括号；标量不含尾随空白。
 */
static inline int airy_json_key_find(const char *json, const char *path,
                                     const char **out_ptr, size_t *out_len)
{
    const char *cur;
    size_t cur_len = 0;
    int rc;

    if (out_ptr)
        *out_ptr = NULL;
    if (out_len)
        *out_len = 0;
    rc = airy_json_find_init(json, path, &cur);
    if (rc != AIRY_JSON_OK)
        return rc;
    while (*path) {
        const char *seg;
        size_t seg_len;
        const char *v;
        size_t vlen;

        rc = airy_json_seg_next(&path, &seg, &seg_len);
        if (rc < 0)
            return AIRY_JSON_EINVAL;
        if (rc == 0)
            break;
        if (*cur != '{')
            return AIRY_JSON_ENOTFOUND;
        rc = airy_json_obj_get(cur, seg, seg_len, &v, &vlen);
        if (rc != AIRY_JSON_OK)
            return rc;
        cur = v;
        cur_len = vlen;
        while (*path == '[') {
            size_t idx = 0;

            rc = airy_json_arr_step(&path, &idx);
            if (rc != AIRY_JSON_OK)
                return rc;
            rc = airy_json_arr_get(cur, idx, &v, &vlen);
            if (rc != AIRY_JSON_OK)
                return rc;
            cur = v;
            cur_len = vlen;
        }
    }
    return airy_json_resolve(cur, cur_len, out_ptr, out_len);
}

/** @brief 按路径取布尔字面量；非 true/false 返回 ENOTBOOL。 */
static inline int airy_json_key_bool(const char *json, const char *path,
                                     int *out_bool)
{
    const char *v;
    size_t len;
    int rc;

    if (!out_bool)
        return AIRY_JSON_EINVAL;
    *out_bool = 0;
    rc = airy_json_key_find(json, path, &v, &len);
    if (rc != AIRY_JSON_OK)
        return rc;
    if (len == 4 && memcmp(v, "true", 4) == 0) {
        *out_bool = 1;
        return AIRY_JSON_OK;
    }
    if (len == 5 && memcmp(v, "false", 5) == 0)
        return AIRY_JSON_OK;
    return AIRY_JSON_ENOTBOOL;
}

/* 0.1.21 D-5 之前的无层级扫描，仅排障开关 AIRY_JSON_KEY_LEGACY=1 时启用。 */
static inline const char *airy_json_key_legacy(const char *json, const char *key)
{
    size_t klen;
    const char *p;

    if (!json || !key || !*key)
        return NULL;
    klen = strlen(key);
    p = json;
    while ((p = strchr(p, '"')) != NULL) {
        if (strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            const char *c = p + 2 + klen;

            while (isspace((unsigned char)*c))
                c++;
            if (*c != ':') {
                p = c;
                continue;
            }
            c++;
            while (isspace((unsigned char)*c))
                c++;
            return c;
        }
        p++;
    }
    return NULL;
}

static inline int airy_json_key_legacy_on(void)
{
    static int cached = -1;

    if (cached < 0) {
        const char *v = getenv("AIRY_JSON_KEY_LEGACY");

        cached = (v && *v && *v != '0') ? 1 : 0;
    }
    return cached;
}

/**
 * @brief 兼容旧签名：按顶层键返回唯一命中的值首字符。
 *
 * 内部改走 airy_json_key_find（字符串值不再被误扫），命中多处或不存在
 * 返回 NULL；旧的无层级首命中行为可按排障开关恢复。
 */
static inline const char *airy_json_key(const char *json, const char *key)
{
    const char *v;

    if (airy_json_key_legacy_on())
        return airy_json_key_legacy(json, key);
    if (airy_json_key_find(json, key, &v, NULL) != AIRY_JSON_OK)
        return NULL;
    return v;
}

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_STRING_JSON_KEY_H */
