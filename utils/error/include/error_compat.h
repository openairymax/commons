// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file error_compat.h
 * @brief 错误处理模块向后兼容层
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件提供从其他错误处理模块到统一错误处理模块（agentrt/commons/utils/error/）的
 * 向后兼容映射，支持渐进式迁移。
 *
 * 设计原则：
 * 1. 保持现有代码不变，通过映射层实现兼容
 * 2. 提供逐步迁移路径
 * 3. 保持错误处理语义一致性
 */

#ifndef AIRY_RT_UTILS_ERROR_COMPAT_H
#define AIRY_RT_UTILS_ERROR_COMPAT_H

#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== agentrt/atoms/utils/error/ 模块兼容层 ==================== */
/**
 * 为 agentrt/atoms/utils/error/include/error.h 提供的兼容层。
 * 该模块使用正数错误码（1000+），需要映射到统一模块的负数错误码。
 */

/* 错误码映射表 */
#define AIRY_COMPAT_ERRNO_BASE 1000

/* 成功码映射 */
#define AIRY_COMPAT_SUCCESS AIRY_OK

/* 通用错误映射 (1001-1010) */
#define AIRY_COMPAT_EUNKNOWN AIRY_ERR_UNKNOWN
#define AIRY_COMPAT_EINVAL AIRY_ERR_INVALID_PARAM
#define AIRY_COMPAT_ENOMEM AIRY_ERR_OUT_OF_MEMORY
#define AIRY_COMPAT_EBUSY AIRY_ERR_BUSY
#define AIRY_COMPAT_ENOENT AIRY_ERR_NOT_FOUND
#define AIRY_COMPAT_EPERM AIRY_ERR_PERMISSION_DENIED
#define AIRY_COMPAT_ETIMEDOUT AIRY_ERR_TIMEOUT
#define AIRY_COMPAT_EEXIST AIRY_ERR_ALREADY_EXISTS
#define AIRY_COMPAT_ECANCELED AIRY_ERR_CANCELED
#define AIRY_COMPAT_ENOTSUP AIRY_ERR_NOT_SUPPORTED

/* 系统错误映射 (1011-1020) */
#define AIRY_COMPAT_EIO AIRY_ERR_IO
#define AIRY_COMPAT_EINTR AIRY_ERR_INTERRUPTED
#define AIRY_COMPAT_EOVERFLOW AIRY_ERR_OVERFLOW
#define AIRY_COMPAT_EBADF AIRY_ERR_SYS_FILE
#define AIRY_COMPAT_ENOTINIT AIRY_ERR_SYS_NOT_INIT
#define AIRY_COMPAT_ERESOURCE AIRY_ERR_SYS_RESOURCE

/* 内核错误映射 (1021-1030) */
#define AIRY_COMPAT_EIPCFAIL AIRY_ERR_KERN_IPC
#define AIRY_COMPAT_ETASKFAIL AIRY_ERR_KERN_TASK
#define AIRY_COMPAT_ESYNCFAIL AIRY_ERR_KERN_SYNC
#define AIRY_COMPAT_ELOCKFAIL AIRY_ERR_KERN_LOCK

/* 认知层错误映射 (1031-1040) */
#define AIRY_COMPAT_EPLANFAIL AIRY_ERR_COORD_PLAN_FAIL
#define AIRY_COMPAT_ECOORDFAIL AIRY_ERR_COORD_SYNC_FAIL
#define AIRY_COMPAT_EDISPFAIL AIRY_ERR_COORD_DISPATCH
#define AIRY_COMPAT_EINTENTFAIL AIRY_ERR_COORD_INTENT

/* 执行层错误映射 (1041-1050) */
#define AIRY_COMPAT_EEXECFAIL AIRY_ERR_EXEC_FAIL
#define AIRY_COMPAT_ECOMPENSATE AIRY_ERR_COORD_COMPENSATE
#define AIRY_COMPAT_ERETRYEXCEEDED AIRY_ERR_COORD_RETRY_EXCEED
#define AIRY_COMPAT_EUNITNOTFOUND AIRY_ERR_EXEC_NOT_FOUND

/* 记忆层错误映射 (1051-1060) */
#define AIRY_COMPAT_EMEMWRITE AIRY_ERR_MEM_WRITE
#define AIRY_COMPAT_EMEMREAD AIRY_ERR_MEM_READ
#define AIRY_COMPAT_EMEMQUERY AIRY_ERR_MEM_QUERY
#define AIRY_COMPAT_EEVOLVE AIRY_ERR_MEM_EVOLVE

/* 安全错误映射 (1061-1070) */
#define AIRY_COMPAT_ESECURITY AIRY_ERR_ESECURITY
#define AIRY_COMPAT_ESANITIZE AIRY_ERR_ESANITIZE
#define AIRY_COMPAT_EAUDIT AIRY_ERR_SEC_AUDIT

/* ==================== 类型定义兼容层 ==================== */

/* 错误严重程度映射 */
typedef airy_err_severity_t airy_compat_error_severity_t;

/* 错误类别映射 */
typedef enum {
    AIRY_COMPAT_ERROR_CAT_SYSTEM = 0,
    AIRY_COMPAT_ERROR_CAT_KERNEL = 1,
    AIRY_COMPAT_ERROR_CAT_COGNITION = 2,
    AIRY_COMPAT_ERROR_CAT_EXECUTION = 3,
    AIRY_COMPAT_ERROR_CAT_MEMORY = 4,
    AIRY_COMPAT_ERROR_CAT_SECURITY = 5
} airy_compat_error_category_t;

/* 结构化错误信息兼容结构 */
typedef struct {
    int code;
    airy_compat_error_severity_t severity;
    airy_compat_error_category_t category;
    const char *module;
    const char *function;
    const char *file;
    int line;
    char message[512];
    uint64_t timestamp_ns;
    void *context;
} airy_compat_error_info_t;

/* 错误上下文兼容结构 */
typedef struct {
    const char *function;
    const char *file;
    int line;
    char message[512];
    void *user_data;
} airy_compat_error_context_t;

/* 错误处理回调函数类型 */
typedef void (*airy_compat_error_handler_t)(airy_err_t err,
                                               const airy_compat_error_context_t *context);
typedef void (*airy_compat_error_info_handler_t)(const airy_compat_error_info_t *info);

/* ==================== 函数接口兼容层 ==================== */

/**
 * @brief 获取错误码的字符串描述（兼容接口）
 * @param err 错误码
 * @return 错误描述字符串
 */
static inline const char *airy_compat_error_str(airy_err_t err)
{
    return airy_err_str(err);
}

/**
 * @brief 获取错误码的严重程度（兼容接口）
 * @param err 错误码
 * @return 严重程度
 */
static inline airy_compat_error_severity_t airy_compat_error_get_severity(airy_err_t err)
{
    return (airy_compat_error_severity_t)airy_err_get_severity(err);
}

/**
 * @brief 获取错误码的类别（兼容接口）
 * @param err 错误码
 * @return 错误类别
 * @note 这是一个近似映射，因为统一模块使用不同的类别系统
 */
static inline airy_compat_error_category_t airy_compat_error_get_category(airy_err_t err)
{
    /* 简化映射：根据错误码范围判断类别 */
    if (err >= -99 && err <= -1)
        return AIRY_COMPAT_ERROR_CAT_SYSTEM;
    else if (err >= -199 && err <= -100)
        return AIRY_COMPAT_ERROR_CAT_SYSTEM;
    else if (err >= -299 && err <= -200)
        return AIRY_COMPAT_ERROR_CAT_KERNEL;
    else if (err >= -399 && err <= -300)
        return AIRY_COMPAT_ERROR_CAT_COGNITION;
    else if (err >= -599 && err <= -400)
        return AIRY_COMPAT_ERROR_CAT_EXECUTION;
    else if (err >= -699 && err <= -600)
        return AIRY_COMPAT_ERROR_CAT_MEMORY;
    else if (err >= -899 && err <= -700)
        return AIRY_COMPAT_ERROR_CAT_SECURITY;
    else
        return AIRY_COMPAT_ERROR_CAT_SYSTEM;
}

/**
 * @brief 设置全局错误处理回调（兼容接口）
 * @param handler 错误处理回调函数
 * @note 统一模块使用不同的错误处理机制，此函数提供基本兼容
 */
/* G2.5 统一错误码表：原为 static 全局变量（per-TU 独立副本，跨 TU 不可见），
 * 已改为 extern 声明，定义位于 handler.c，确保全局回调机制跨翻译单元生效。 */
extern airy_compat_error_handler_t g_compat_error_handler;
extern airy_compat_error_info_handler_t g_compat_error_info_handler;

void airy_compat_error_set_handler(airy_compat_error_handler_t handler);
void airy_compat_error_set_info_handler(airy_compat_error_info_handler_t handler);

/**
 * @brief 处理错误并记录日志（兼容接口）
 * @param err 错误码
 * @param file 文件名
 * @param line 行号
 * @param fmt 附加信息格式
 * @param ... 参数
 */
static inline void airy_compat_error_handle(airy_err_t err, const char *file, int line,
                                               const char *fmt, ...)
{
    /* 使用统一模块的错误推送接口 */
    if (err != AIRY_OK) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt,
                  args); /* flawfinder: ignore - variadic error wrapper with bounded buffer */
        va_end(args);

        airy_err_push_ex(err, file, line, "unknown", "%s", buffer);
    }
}

/**
 * @brief 带上下文的错误处理（兼容接口）
 * @param err 错误码
 * @param function 函数名
 * @param file 文件名
 * @param line 行号
 * @param user_data 用户数据
 * @param fmt 附加信息格式
 * @param ... 参数
 */
static inline void airy_compat_error_handle_with_context(airy_err_t err,
                                                            const char *function, const char *file,
                                                            int line, void *user_data,
                                                            const char *fmt, ...)
{
    if (g_compat_error_info_handler && err != AIRY_OK) {
        char buffer[512];
        va_list args2;
        va_start(args2, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args2);
        va_end(args2);
        airy_compat_error_info_t info;
__builtin_memset(&info, 0, sizeof(info));
        info.code = err;
        info.function = function;
        info.file = file;
        info.line = line;
        info.context = user_data;
        snprintf(info.message, sizeof(info.message), "%s", buffer);
        g_compat_error_info_handler(&info);
    }

    if (err != AIRY_OK) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt,
                  args); /* flawfinder: ignore - variadic error wrapper with bounded buffer */
        va_end(args);

        airy_err_push_ex(err, file, line, function, "%s", buffer);
    }
}

/* ==================== 便捷宏兼容层 ==================== */

#define AIRY_COMPAT_ERROR_HANDLE(err, fmt, ...) \
    airy_compat_error_handle(err, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define AIRY_COMPAT_ERROR_HANDLE_CONTEXT(err, user_data, fmt, ...)                           \
    airy_compat_error_handle_with_context(err, __func__, __FILE__, __LINE__, user_data, fmt, \
                                             ##__VA_ARGS__)

#define AIRY_COMPAT_CHECK_NULL(ptr, err)                             \
    do {                                                                \
        if ((ptr) == NULL) {                                            \
            AIRY_COMPAT_ERROR_HANDLE(err, "Null pointer: %s", #ptr); \
            return err;                                                 \
        }                                                               \
    } while (0)

#define AIRY_COMPAT_CHECK_ERROR(expr)      \
    do {                                      \
        airy_err_t _err = (expr);        \
        if (_err != AIRY_COMPAT_SUCCESS) { \
            return _err;                      \
        }                                     \
    } while (0)

/* ==================== 迁移辅助宏 ==================== */

/**
 * 使用这些宏可以逐步迁移代码到统一错误处理模块
 */

/* 将 agentrt/atoms/utils/error/ 的错误码宏名映射到兼容层 */
#ifdef AIRY_USE_COMPAT_ERRORS
/* 定义原 agentrt/atoms/utils/error/ 模块的宏名，映射到兼容层 */
#define AIRY_SUCCESS AIRY_COMPAT_SUCCESS
#define AIRY_EUNKNOWN AIRY_COMPAT_EUNKNOWN
#define AIRY_EINVAL AIRY_COMPAT_EINVAL
#define AIRY_ENOMEM AIRY_COMPAT_ENOMEM
#define AIRY_EBUSY AIRY_COMPAT_EBUSY
#define AIRY_ENOENT AIRY_COMPAT_ENOENT
#define AIRY_EPERM AIRY_COMPAT_EPERM
#define AIRY_ETIMEDOUT AIRY_COMPAT_ETIMEDOUT
#define AIRY_EEXIST AIRY_COMPAT_EEXIST
#define AIRY_ECANCELED AIRY_COMPAT_ECANCELED
#define AIRY_ENOTSUP AIRY_COMPAT_ENOTSUP
#define AIRY_EIO AIRY_COMPAT_EIO
#define AIRY_EINTR AIRY_COMPAT_EINTR
#define AIRY_EOVERFLOW AIRY_COMPAT_EOVERFLOW
#define AIRY_EBADF AIRY_COMPAT_EBADF
#define AIRY_ENOTINIT AIRY_COMPAT_ENOTINIT
#define AIRY_ERESOURCE AIRY_COMPAT_ERESOURCE
#define AIRY_EIPCFAIL AIRY_COMPAT_EIPCFAIL
#define AIRY_ETASKFAIL AIRY_COMPAT_ETASKFAIL
#define AIRY_ESYNCFAIL AIRY_COMPAT_ESYNCFAIL
#define AIRY_ELOCKFAIL AIRY_COMPAT_ELOCKFAIL
#define AIRY_EPLANFAIL AIRY_COMPAT_EPLANFAIL
#define AIRY_ECOORDFAIL AIRY_COMPAT_ECOORDFAIL
#define AIRY_EDISPFAIL AIRY_COMPAT_EDISPFAIL
#define AIRY_EINTENTFAIL AIRY_COMPAT_EINTENTFAIL
#define AIRY_EEXECFAIL AIRY_COMPAT_EEXECFAIL
#define AIRY_ECOMPENSATE AIRY_COMPAT_ECOMPENSATE
#define AIRY_ERETRYEXCEEDED AIRY_COMPAT_ERETRYEXCEEDED
#define AIRY_EUNITNOTFOUND AIRY_COMPAT_EUNITNOTFOUND
#define AIRY_EMEMWRITE AIRY_COMPAT_EMEMWRITE
#define AIRY_EMEMREAD AIRY_COMPAT_EMEMREAD
#define AIRY_EMEMQUERY AIRY_COMPAT_EMEMQUERY
#define AIRY_EEVOLVE AIRY_COMPAT_EEVOLVE
#define AIRY_ESECURITY AIRY_COMPAT_ESECURITY
#define AIRY_ESANITIZE AIRY_COMPAT_ESANITIZE
#define AIRY_EAUDIT AIRY_COMPAT_EAUDIT

/* 类型定义映射 */
#define airy_err_severity_t airy_compat_error_severity_t
#define airy_err_category_t airy_compat_error_category_t
#define airy_err_info_t airy_compat_error_info_t
#define airy_err_context_t airy_compat_error_context_t
#define airy_err_handler_t airy_compat_error_handler_t
#define airy_err_info_handler_t airy_compat_error_info_handler_t

/* 函数接口映射 */
#define airy_err_str airy_compat_error_str
#define airy_err_get_severity airy_compat_error_get_severity
#define airy_err_get_category airy_compat_error_get_category
#define airy_err_set_handler airy_compat_error_set_handler
#define airy_err_set_info_handler airy_compat_error_set_info_handler
#define airy_err_handle airy_compat_error_handle
#define airy_err_handle_with_context airy_compat_error_handle_with_context

/* 宏定义映射 */
#define AIRY_ERROR_HANDLE AIRY_COMPAT_ERROR_HANDLE
#define AIRY_ERROR_HANDLE_CONTEXT AIRY_COMPAT_ERROR_HANDLE_CONTEXT
#define AIRY_CHECK_NULL AIRY_COMPAT_CHECK_NULL
#define AIRY_CHECK_ERROR AIRY_COMPAT_CHECK_ERROR
#endif

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_ERROR_COMPAT_H */