/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file error_catalog_internal.h
 * @brief 错误码目录（code → 名称/中英描述/严重度）内部查询契约。
 *
 * 仅由 handler.c 与 error_catalog.c 共用，不对外导出：目录表本体是纯静
 * 态数据，与错误链、线程态、i18n 注册等运行态解耦，便于独立审阅与替换。
 */

#ifndef AIRY_RT_ERROR_CATALOG_INTERNAL_H
#define AIRY_RT_ERROR_CATALOG_INTERNAL_H

#include "error.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    airy_err_t code;
    const char *name;
    const char *description_en;
    const char *description_zh_cn;
    airy_err_severity_t severity;
} error_info_t;

/** @brief 目录表首地址（只读，生命期贯穿进程）。 */
const error_info_t *airy_err_table(size_t *count);

/**
 * @brief 按已归一的错误码查目录项。
 * @param norm 调用方已用 AIRY_ERR_NEG() 归一（负值）的错误码
 * @return 命中项指针；未收录返回 NULL
 */
const error_info_t *airy_err_find(airy_err_t norm);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_ERROR_CATALOG_INTERNAL_H */
