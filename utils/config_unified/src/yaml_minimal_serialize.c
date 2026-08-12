// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_serialize.c
 * @brief YAML 1.1 解析器 - 序列化输出
 *
 * 本文件实现 YAML 节点树序列化：递归转储与整文档序列化，
 * 单一职责。
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

void yaml_dump(struct yaml_node *node, char *buf, size_t bufsize, int indent)
{
    if (!node || !buf || bufsize == 0)
        return;
    size_t off = strlen(buf);
#define APPEND(...)                                                                 \
    do {                                                                            \
        off += snprintf(buf + off, bufsize > off ? bufsize - off : 0, __VA_ARGS__); \
    } while (0)

    for (int i = 0; i < indent; i++)
        APPEND("  ");

    switch (node->type) {
    case YAML_NODE_NONE:
        APPEND("~");
        break;
    case YAML_NODE_SCALAR: {
        const char *v = node->scalar.value ? node->scalar.value : "";
        bool needs_quote = (*v == '\0') || strchr(v, ':') || strchr(v, '#') || strchr(v, '[') ||
                           strchr(v, '{') || strchr(v, ',') || strchr(v, '"') || strchr(v, '\'');
        if (needs_quote) {
            APPEND("\"%s\"", v);
        } else {
            APPEND("%s", v);
        }
        break;
    }
    case YAML_NODE_MAPPING: {
        size_t sz = yaml_size(node);
        if (sz == 0) {
            APPEND("{}");
            break;
        }
        APPEND("{\n");
        for (size_t i = 0; i < sz; i++) {
            for (int j = 0; j < indent + 1; j++)
                APPEND("  ");
            APPEND("%s: ", node->mapping[i].key);
            yaml_dump(node->mapping[i].value, buf, bufsize, indent + 1);
            if (i < sz - 1)
                APPEND(",");
            APPEND("\n");
        }
        for (int i = 0; i < indent; i++)
            APPEND("  ");
        APPEND("}");
        break;
    }
    case YAML_NODE_SEQUENCE: {
        size_t cnt = node->sequence.count;
        if (cnt == 0) {
            APPEND("[]");
            break;
        }
        APPEND("[\n");
        for (size_t i = 0; i < cnt; i++) {
            yaml_dump(node->sequence.items[i].item, buf, bufsize, indent + 1);
            if (i < cnt - 1)
                APPEND(",");
            APPEND("\n");
        }
        for (int i = 0; i < indent; i++)
            APPEND("  ");
        APPEND("]");
        break;
    }
    }
#undef APPEND
}

char *yaml_serialize(yaml_document_t *doc)
{
    if (!doc || !doc->root)
        return NULL;

    size_t bufsize = 4096;
    char *buf = (char *)AIRY_MALLOC(bufsize);
    if (!buf)
        return NULL;

    buf[0] = '\0';
    yaml_dump(doc->root, buf, bufsize, 0);

    size_t len = strlen(buf);
    char *result = (char *)AIRY_MALLOC(len + 1);
    if (result) {
        __builtin_memcpy(result, buf, len + 1);
    }
    AIRY_FREE(buf);
    return result;
}
