/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file safe_utf8.h
 * @brief UTF-8 安全工具：RFC 3629 合规性校验与非法序列净化。
 *
 * 与 safe_string_utils.h 同属 commons 字符串唯一真相源（SSoT）；所有
 * 需要判定或修复 UTF-8 字节流的模块一律委托本文件，不得自研轮子。
 */

#ifndef AIRY_RT_SAFE_UTF8_H
#define AIRY_RT_SAFE_UTF8_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


bool utf8_validate(const char *str, size_t len);


size_t utf8_sanitize(const char *in, size_t len, char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SAFE_UTF8_H */
