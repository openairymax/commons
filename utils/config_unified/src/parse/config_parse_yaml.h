// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse_yaml.h
 * @brief Unified config module - YAML parsing internal declarations.
 *
 * 2026-08-27 域拆分：YAML 解析按结构（映射/序列递归骨架）与标量
 * （引号/块标量/流式集合/纯标量）拆为两个翻译单元，互递归依赖经
 * 本头声明衔接：
 *   - config_parse_yaml.c        状态机骨架 + mapping/sequence + 入口
 *   - config_parse_yaml_scalar.c 各类标量与流式集合解析
 */

#ifndef CONFIG_PARSE_YAML_H
#define CONFIG_PARSE_YAML_H

#include "config_parse_internal.h"

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int line;
} yaml_parse_state_t;

/* ---- 状态机基础助手（config_parse_yaml.c 实现，标量侧复用） ---- */
int yaml_ps_peek(yaml_parse_state_t *s);
int yaml_ps_advance(yaml_parse_state_t *s);
void yaml_ps_skip_ws(yaml_parse_state_t *s);
void yaml_ps_skip_to_eol(yaml_parse_state_t *s);
void yaml_ps_skip_eol(yaml_parse_state_t *s);
int yaml_ps_count_indent(yaml_parse_state_t *s);

/* ---- 结构递归（config_parse_yaml.c 实现） ---- */
config_error_t yaml_parse_value(yaml_parse_state_t *s, int base_indent, const char *prefix,
                                config_context_t *ctx);
config_error_t yaml_parse_mapping(yaml_parse_state_t *s, int base_indent, const char *prefix,
                                  config_context_t *ctx);
config_error_t yaml_parse_sequence(yaml_parse_state_t *s, int base_indent, const char *prefix,
                                   config_context_t *ctx);

/* ---- 标量/流式解析（config_parse_yaml_scalar.c 实现） ---- */
bool yaml_ps_skip_anchor_tag(yaml_parse_state_t *s, int *c);
config_error_t yaml_parse_quoted(yaml_parse_state_t *s, int quote, const char *prefix,
                                 config_context_t *ctx);
config_error_t yaml_parse_block_scalar(yaml_parse_state_t *s, const char *prefix,
                                       config_context_t *ctx);
config_error_t yaml_parse_inline_sequence(yaml_parse_state_t *s, const char *prefix,
                                          config_context_t *ctx);
config_error_t yaml_parse_inline_mapping(yaml_parse_state_t *s, const char *prefix,
                                         config_context_t *ctx);
config_error_t yaml_parse_plain_scalar_value(yaml_parse_state_t *s, int base_indent,
                                             const char *prefix, config_context_t *ctx);

#endif /* CONFIG_PARSE_YAML_H */
