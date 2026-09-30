// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse_yaml.c
 * @brief Unified config module - YAML flattening onto config_context_t.
 *
 * YAML 语法解析唯一实现为 yaml_minimal（锚点/别名/合并键/块标量等
 * 全集支持）；本文件只承担「文档树 -> 点分键」语义拍平：映射展开为
 * a.b.c 层级键，序列展开为 a.0/a.1 数字索引键，空容器与空标量落为
 * 空字符串值。原独立缩进状态机（本文件 + config_parse_yaml_scalar.c）
 * 已消解，行为对齐原拍平规则。
 */

#include "config_parse_internal.h"
#include "yaml_minimal.h"

static void flatten_node(const struct yaml_node *node, const char *prefix, config_context_t *ctx)
{
    if (!node)
        return;

    if (node->type == YAML_NODE_SCALAR) {
        const char *val = node->scalar.value ? node->scalar.value : "";
        config_value_t *cv = config_value_create_string(val);
        if (cv)
            config_context_set(ctx, prefix, cv);
        return;
    }

    if (node->type == YAML_NODE_MAPPING && node->mapping == NULL) {
        config_value_t *cv = config_value_create_string("");
        if (cv)
            config_context_set(ctx, prefix, cv);
        return;
    }

    if (node->type == YAML_NODE_MAPPING) {
        for (struct yaml_mapping_entry *e = node->mapping; e && e->key; e++) {
            char full_key[1024];
            if (prefix && prefix[0])
                snprintf(full_key, sizeof(full_key), "%s.%s", prefix, e->key);
            else
                snprintf(full_key, sizeof(full_key), "%s", e->key);
            flatten_node(e->value, full_key, ctx);
        }
        return;
    }

    if (node->type == YAML_NODE_SEQUENCE) {
        if (node->sequence.items == NULL || node->sequence.count == 0) {
            config_value_t *cv = config_value_create_string("");
            if (cv)
                config_context_set(ctx, prefix, cv);
            return;
        }
        for (size_t i = 0; i < node->sequence.count; i++) {
            char idx_key[1024];
            snprintf(idx_key, sizeof(idx_key), "%s.%zu", prefix, i);
            flatten_node(node->sequence.items[i].item, idx_key, ctx);
        }
    }
}

config_error_t config_parse_yaml(const char *data, size_t data_len, config_context_t *ctx)
{
    if (!data || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    yaml_document_t *doc = yaml_create();
    if (!doc)
        return CONFIG_ERROR_OUT_OF_MEMORY;

    if (yaml_parse_string(doc, data, data_len) != 0) {
        yaml_destroy(doc);
        return CONFIG_ERROR_PARSE;
    }

    flatten_node(yaml_root(doc), "", ctx);
    yaml_destroy(doc);
    return CONFIG_SUCCESS;
}
