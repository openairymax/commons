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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


bool utf8_validate(const char *str, size_t len);


size_t utf8_sanitize(const char *in, size_t len, char *out, size_t out_cap);


size_t utf8_seq_len(unsigned char first);


/**
 * @brief 返回不大于 len 且结束于完整 UTF-8 序列边界的前缀字节数。
 *
 * 供字节级截断（snprintf 精度、定长缓冲）写完后回退到字符边界：尾部
 * 残缺的续字节与不完整首字节一并丢弃，避免把半个多字节字符交给下游
 * （LLM 接口以 400/412 拒绝含非法 UTF-8 的 messages）。输入无需 NUL
 * 结尾；返回值为 [0, len]，调用方据此重新置终止符。
 */
size_t utf8_prefix_len(const char *s, size_t len);


bool utf8_decode(const unsigned char *s, size_t avail, size_t *n, uint32_t *cp);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_SAFE_UTF8_H */
