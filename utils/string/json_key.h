// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file json_key.h
 * @brief JSON 顶层键定位机制件（全树唯一实现，无 cJSON 依赖）。
 *
 * 供未强制依赖 cJSON 的模块在解析外部或不可信 JSON 文本时定位对象成员
 * 值的起始位置，取代各模块自研的 strstr 键定位副本（Unify Design SSoT）。
 * 仅识别「带引号完整键后紧邻冒号」的形态，用于跳过值域内嵌的同名字面量，
 * 并非完整 JSON 解析器——强制依赖 cJSON 的模块应优先使用 cJSON 结构化
 * 解析（见 commons/utils/cjson/cjson_helpers.h）。
 *
 * 语义：以带引号完整键 `"key"` 精确匹配（`"turn"` 不会命中 `"current_turn"`），
 * 只认其后跳过空白即随 `:` 的匹配，返回越过 `:` 与空白后的值首字符指针；
 * 未找到或参数非法返回 NULL。
 */

#ifndef AIRY_RT_STRING_JSON_KEY_H
#define AIRY_RT_STRING_JSON_KEY_H

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *airy_json_key(const char *json, const char *key)
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

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_STRING_JSON_KEY_H */
