// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_node.c
 * @brief YAML 1.1 解析器 - 节点类型访问
 *
 * 本文件实现 YAML 节点访问接口：大小统计、键存在判断、
 * 按键/索引取值，单一职责。
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <string.h>
#include "error.h"

size_t yaml_size(struct yaml_node *node)
{
    if (!node)
        return 0;
    switch (node->type) {
    case YAML_NODE_MAPPING: {
        size_t c = 0;
        for (size_t i = 0;; i++) {
            if (!node->mapping[i].key)
                break;
            c++;
        }
        return c;
    }
    case YAML_NODE_SEQUENCE:
        return node->sequence.count;
    default:
        return 0;
    }
}

bool yaml_has_key(struct yaml_node *node, const char *key)
{
    if (!node || node->type != YAML_NODE_MAPPING || !key)
        return false;
    size_t sz = yaml_size(node);
    for (size_t i = 0; i < sz; i++) {
        if (node->mapping[i].key && strcmp(node->mapping[i].key, key) == 0)
            return true;
    }
    return false;
}

struct yaml_node *yaml_get(struct yaml_node *node, const char *key)
{
    if (!node || node->type != YAML_NODE_MAPPING || !key)
        return NULL;
    size_t sz = yaml_size(node);
    for (size_t i = 0; i < sz; i++) {
        if (node->mapping[i].key && strcmp(node->mapping[i].key, key) == 0)
            return node->mapping[i].value;
    }
    return NULL;
}

struct yaml_node *yaml_get_index(struct yaml_node *node, size_t index)
{
    if (!node || node->type != YAML_NODE_SEQUENCE)
        return NULL;
    if (index >= node->sequence.count)
        return NULL;
    return node->sequence.items[index].item;
}
