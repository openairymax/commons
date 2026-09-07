// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_serialize.c
 * @brief YAML 1.1 parser - serialization output.
 *
 * Implements YAML node tree serialization: recursive dumping and whole
 * document serialization, single responsibility.
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

/* 递归 dump 的核心：全部写入共享 *off 游标，杜绝"子层基于 strlen 自增、
 * 父层基于旧 off 覆写"的 offset 错乱（原实现 L27 每次递归重新 strlen，
 * 父子同写一段导致输出错乱且可能截断误判）。写入饱和到 bufsize，
 * 永不越界。 */
#define YAML_APPEND(offp, buf, bufsize, ...)                                          \
    do {                                                                               \
        size_t _off = *(offp);                                                         \
        if (_off < (bufsize)) {                                                        \
            int _n = snprintf((buf) + _off, (bufsize) - _off, __VA_ARGS__);            \
            if (_n > 0)                                                                \
                *(offp) = _off + (size_t)_n;                                           \
        }                                                                              \
        if (*(offp) > (bufsize))                                                       \
            *(offp) = (bufsize);                                                       \
    } while (0)

static void yaml_dump_append(struct yaml_node *node, char *buf, size_t bufsize, int indent,
                             size_t *off)
{
    if (!node || !buf || bufsize == 0 || !off)
        return;

    for (int i = 0; i < indent; i++)
        YAML_APPEND(off, buf, bufsize, "  ");

    switch (node->type) {
    case YAML_NODE_NONE:
        YAML_APPEND(off, buf, bufsize, "~");
        break;
    case YAML_NODE_SCALAR: {
        const char *v = node->scalar.value ? node->scalar.value : "";
        bool needs_quote = (*v == '\0') || strchr(v, ':') || strchr(v, '#') || strchr(v, '[') ||
                           strchr(v, '{') || strchr(v, ',') || strchr(v, '"') || strchr(v, '\'');
        if (needs_quote) {
            YAML_APPEND(off, buf, bufsize, "\"%s\"", v);
        } else {
            YAML_APPEND(off, buf, bufsize, "%s", v);
        }
        break;
    }
    case YAML_NODE_MAPPING: {
        size_t sz = yaml_size(node);
        if (sz == 0) {
            YAML_APPEND(off, buf, bufsize, "{}");
            break;
        }
        YAML_APPEND(off, buf, bufsize, "{\n");
        for (size_t i = 0; i < sz; i++) {
            for (int j = 0; j < indent + 1; j++)
                YAML_APPEND(off, buf, bufsize, "  ");
            YAML_APPEND(off, buf, bufsize, "%s: ", node->mapping[i].key);
            yaml_dump_append(node->mapping[i].value, buf, bufsize, indent + 1, off);
            if (i < sz - 1)
                YAML_APPEND(off, buf, bufsize, ",");
            YAML_APPEND(off, buf, bufsize, "\n");
        }
        for (int i = 0; i < indent; i++)
            YAML_APPEND(off, buf, bufsize, "  ");
        YAML_APPEND(off, buf, bufsize, "}");
        break;
    }
    case YAML_NODE_SEQUENCE: {
        size_t cnt = node->sequence.count;
        if (cnt == 0) {
            YAML_APPEND(off, buf, bufsize, "[]");
            break;
        }
        YAML_APPEND(off, buf, bufsize, "[\n");
        for (size_t i = 0; i < cnt; i++) {
            yaml_dump_append(node->sequence.items[i].item, buf, bufsize, indent + 1, off);
            if (i < cnt - 1)
                YAML_APPEND(off, buf, bufsize, ",");
            YAML_APPEND(off, buf, bufsize, "\n");
        }
        for (int i = 0; i < indent; i++)
            YAML_APPEND(off, buf, bufsize, "  ");
        YAML_APPEND(off, buf, bufsize, "]");
        break;
    }
    }
}

void yaml_dump(struct yaml_node *node, char *buf, size_t bufsize, int indent)
{
    /* 追加契约：调用方须提供已 NUL 终止的缓冲区（可为空串），dump 从
     * 末尾续写（cupolas_config_export_yaml 在头部注释后调用依赖此语义）。 */
    if (!node || !buf || bufsize == 0)
        return;
    size_t off = strlen(buf);
    if (off >= bufsize)
        return;
    yaml_dump_append(node, buf, bufsize, indent, &off);
    if (off >= bufsize)
        off = bufsize - 1;
    buf[off] = '\0';
}
#undef YAML_APPEND

char *yaml_serialize(yaml_document_t *doc)
{
    if (!doc || !doc->root)
        return NULL;

    size_t bufsize = 4096;
    char *buf = (char *)AIRY_MALLOC(bufsize);
    if (!buf)
        return NULL;

    buf[0] = '\0';
    size_t off = 0;
    yaml_dump_append(doc->root, buf, bufsize, 0, &off);
    if (off >= bufsize)
        off = bufsize - 1;
    buf[off] = '\0';

    char *result = (char *)AIRY_MALLOC(off + 1);
    if (result) {
        __builtin_memcpy(result, buf, off + 1);
    }
    AIRY_FREE(buf);
    return result;
}
