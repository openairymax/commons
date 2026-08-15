/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file error.h
 * @brief 统一错误处理框架
 *
 * 设计原则：
 * 1. 错误返回值恒为「0 或负」（AIRY_EOK=0；A-UEF [SC] 正幅值宏经
 *    AIRY_ERR_NEG 取负返回，用户态扩展码 AIRY_ERR_* 负值直接返回）
 * 2. 错误码分段管理，避免冲突
 * 3. 支持错误链追踪
 * 4. 线程安全的错误信息存储
 * 5. 支持结构化错误上下文
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-03-30
 * @version 2.0
 *
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯原则
 */

#ifndef AIRY_RT_UTILS_ERROR_H
#define AIRY_RT_UTILS_ERROR_H

#include "../../types/include/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#include "error_codes.h"


/**
 * @brief 错误上下文最大深度
 */
#define AIRY_ERROR_CONTEXT_MAX_DEPTH 16

/**
 * @brief 错误严重程度
 */
typedef enum {
    AIRY_ERR_SEVERITY_INFO = 0,
    AIRY_ERR_SEVERITY_WARNING = 1,
    AIRY_ERR_SEVERITY_ERROR = 2,
    AIRY_ERR_SEVERITY_CRITICAL = 3
} airy_err_severity_t;

/**
 * @brief 错误上下文条目
 */
typedef struct {
    const char *file;
    int line;
    const char *function;
    const char *message;
    airy_err_t error_code;
    uint64_t timestamp_ns;
} airy_err_context_entry_t;

/**
 * @brief 错误链结构
 */
typedef struct {
    airy_err_t code;
    int depth;
    airy_err_context_entry_t contexts[AIRY_ERROR_CONTEXT_MAX_DEPTH];
} airy_err_chain_t;


/**
 * @brief 获取错误码的可读描述
 * @param code 错误码
 * @return 错误描述字符串
 */
const char *airy_err_str(airy_err_t code);

/**
 * @brief 获取错误严重程度
 * @param code 错误码
 * @return 严重程度
 */
airy_err_severity_t airy_err_get_severity(airy_err_t code);

/**
 * @brief 获取当前线程的错误链
 * @return 错误链指针
 */
airy_err_chain_t *airy_err_get_chain(void);

/**
 * @brief 清除当前线程的错误链
 */
void airy_err_clear(void);

/**
 * @brief 清理当前线程的错误状态（释放线程局部存储）
 * @note 应在线程退出前调用，以释放 thread_error_state_t 及其错误链中的 message 字符串
 */
void airy_err_thread_cleanup(void);

/**
 * @brief 添加错误上下文
 * @param code 错误码
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化消息
 * @param ... 可变参数
 */
void airy_err_push_ex(airy_err_t code, const char *file, int line, const char *func,
                      const char *fmt, ...);

/**
 * @brief 打印错误链（用于调试）
 * @param chain 错误链
 */
void airy_err_print_chain(const airy_err_chain_t *chain);

/**
 * @brief 将错误链转换为 JSON 字符串
 * @param chain 错误链
 * @return JSON 字符串（需调用者释放）
 */
char *airy_err_chain_to_json(const airy_err_chain_t *chain);


/**
 * @brief 统一错误返回值：正幅值错误码取负，负值/零原样。
 *
 * S-1 收敛（2026-08-14）：A-UEF [SC] airymax/error.h 的错误码宏为正幅值
 * （AIRY_EINVAL=5 等），调用方须返回 -AIRY_E* 产生负值；用户态扩展码
 * （AIRY_ERR_* 及 airy_types.h 的 AIRY_ETIMEDOUT=-110 等）为负值直接返回。
 * AIRY_ERR_NEG 统一两者：正幅值取负、负值原样、0 不变，保证所有错误返回
 * 路径的返回值恒为「0 或负」，与 A-UEF 错误空间（负 int32_t）及
 * [SC] 辅助宏 AIRY_ERR_FAIL(err)=((err)<0) 语义一致。
 * 仅限传入无副作用的错误码宏。
 */
#define AIRY_ERR_NEG(code) ((code) > 0 ? -(code) : (code))

/**
 * @brief 错误码相等判断：err == -code（正幅值宏）或 err == code（负值码）。
 *
 * S-1 收敛（2026-08-14）：调用方拿到的返回值恒为 AIRY_ERR_NEG(code)
 * （0 或负），而 [SC] 错误码宏为正幅值（AIRY_EINVAL=5）。存量代码
 * 若直接写 err == AIRY_EINVAL 将永不成立（返回值 -5）。AIRY_ERR_EQ
 * 统一比较语义：AIRY_ERR_EQ(err, AIRY_EINVAL) 等价 err == -AIRY_EINVAL。
 */
#define AIRY_ERR_EQ(err, code) ((err) == AIRY_ERR_NEG(code))

/**
 * @brief 设置错误并返回（自动使用错误码字符串）
 *
 * 统一替代各模块自定义的 *_RET_ERR 宏（ATM_RET_ERR / CUP_RET_ERR / RQ_RET_ERR 等）。
 * 等价于 AIRY_ERROR(code, airy_err_str(code))。
 * 返回与压栈值均为 AIRY_ERR_NEG(code)（正幅值取负），保证错误链
 * 与实际返回值一致，airy_err_str 可解析。
 */
#define AIRY_RET_ERR(code)                                                                \
    do {                                                                                  \
        airy_err_push_ex(AIRY_ERR_NEG(code), __FILE__, __LINE__, __func__, "%s",          \
                         airy_err_str(AIRY_ERR_NEG(code)));                               \
        return AIRY_ERR_NEG(code);                                                        \
    } while (0)

/**
 * @brief 设置错误并返回
 */
#define AIRY_ERROR(code, msg)                                                            \
    do {                                                                                 \
        airy_err_push_ex(AIRY_ERR_NEG(code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return AIRY_ERR_NEG(code);                                                       \
    } while (0)

/**
 * @brief 设置格式化错误并返回
 */
#define AIRY_ERROR_FMT(code, fmt, ...)                                                          \
    do {                                                                                        \
        airy_err_push_ex(AIRY_ERR_NEG(code), __FILE__, __LINE__, __func__, (fmt), __VA_ARGS__); \
        return AIRY_ERR_NEG(code);                                                              \
    } while (0)

/**
 * @brief 设置错误并返回 NULL（用于返回指针的函数）
 *
 * 与 AIRY_ERROR 的区别：返回 NULL 而非错误码，适用于函数返回类型为指针的场景。
 * 错误码通过 error stack 传递，调用者可通过 airy_err_last() 获取。
 */
#define AIRY_ERROR_NULL(code, msg)                                                       \
    do {                                                                                 \
        airy_err_push_ex(AIRY_ERR_NEG(code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return NULL;                                                                     \
    } while (0)

/**
 * @brief 条件检查，失败时返回错误
 */
#define AIRY_CHECK(cond, code, msg)    \
    do {                               \
        if (!(cond)) {                 \
            AIRY_ERROR((code), (msg)); \
        }                              \
    } while (0)

/**
 * @brief 空指针检查
 */
#define AIRY_CHECK_NULL(ptr, name) AIRY_CHECK((ptr) != NULL, AIRY_ERR_NULL_POINTER, name " is NULL")

/**
 * @brief 内存分配检查
 */
#define AIRY_CHECK_ALLOC(ptr) \
    AIRY_CHECK((ptr) != NULL, AIRY_ERR_OUT_OF_MEMORY, "Memory allocation failed")

/**
 * @brief 错误传播宏
 */
#define AIRY_PROPAGATE(expr)                                                                    \
    do {                                                                                        \
        airy_err_t __err = (expr);                                                              \
        if (__err != AIRY_EOK) {                                                                \
            airy_err_push_ex(__err, __FILE__, __LINE__, __func__, "Propagated from %s", #expr); \
            return __err;                                                                       \
        }                                                                                       \
    } while (0)

/**
 * @brief 错误检查宏（返回错误码而非直接返回）
 */
#define AIRY_TRY(expr)             \
    do {                           \
        airy_err_t __err = (expr); \
        if (__err != AIRY_EOK) {   \
            return __err;          \
        }                          \
    } while (0)


#ifndef AIRY_ERROR_CONTEXT_T_DEFINED
#define AIRY_ERROR_CONTEXT_T_DEFINED
/**
 * @brief 错误上下文结构（完整版，含时间戳）
 * @note 与 atoms/coreloopthree/include/error_utils.h 保持一致
 */
typedef struct airy_err_context {
    airy_err_t code;
    char *message;
    char *file;
    int line;
    char *function;
    uint64_t timestamp_ns;
} airy_err_context_t;
#endif /* AIRY_ERROR_CONTEXT_T_DEFINED */

/**
 * @brief 错误处理回调函数类型
 * @deprecated 请使用新的错误链接口
 */
typedef void (*airy_err_handler_t)(airy_err_t err, const airy_err_context_t *context);

/**
 * @brief 设置错误处理回调（兼容旧代码）
 * @deprecated
 */
void airy_err_set_handler(airy_err_handler_t handler);

/**
 * @brief 兼容旧代码的错误处理宏
 * @deprecated 请使用 AIRY_ERROR
 */
#define AIRY_ERROR_HANDLE(code, msg)                                         \
    do {                                                                     \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
    } while (0)

#define AIRY_ERROR_PUSH_EX(code, msg) AIRY_ERROR_HANDLE(code, msg)

/**
 * @brief 兼容旧代码的错误处理宏（带上下文）
 * @deprecated
 */
#define AIRY_ERROR_HANDLE_CONTEXT(code, user_data, msg)                      \
    do {                                                                     \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        (void)(user_data);                                                   \
    } while (0)


/**
 * @brief 错误统计信息
 */
typedef struct {
    uint64_t total_errors;
    uint64_t errors_by_code[32];
    uint64_t last_error_time;
    airy_err_t last_error;
} airy_err_stats_t;

/**
 * @brief 获取错误统计
 * @param stats 统计信息输出
 */
void airy_err_get_stats(airy_err_stats_t *stats);

/**
 * @brief 重置错误统计
 */
void airy_err_reset_stats(void);


/**
 * @brief 支持的语言
 */
typedef enum {
    AIRY_LANG_EN_US = 0,
    AIRY_LANG_ZH_CN = 1,
    AIRY_LANG_ZH_TW = 2,
    AIRY_LANG_JA_JP = 3,
    AIRY_LANG_KO_KR = 4,
    AIRY_LANG_DE_DE = 5,
    AIRY_LANG_FR_FR = 6,
    AIRY_LANG_ES_ES = 7
} airy_language_t;

/**
 * @brief 多语言错误描述结构
 */
typedef struct {
    airy_err_t error_code;
    const char *descriptions[8];
} airy_err_i18n_entry_t;

/**
 * @brief 设置当前语言环境
 *
 * @param[in] lang 语言
 * @return 成功返回AIRY_EOK，失败返回错误码
 */
airy_err_t airy_err_set_language(airy_language_t lang);

/**
 * @brief 获取当前语言环境
 *
 * @return 当前语言
 */
airy_language_t airy_err_get_language(void);

/**
 * @brief 获取错误码的本地化描述
 *
 * @param[in] code 错误码
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return 本地化错误描述字符串
 */
const char *airy_err_str_i18n(airy_err_t code, airy_language_t lang);

/**
 * @brief 注册自定义错误码的本地化描述
 *
 * @param[in] entries 错误描述条目数组
 * @param[in] count 条目数量
 * @return 成功返回AIRY_EOK，失败返回错误码
 */
airy_err_t airy_err_register_i18n(const airy_err_i18n_entry_t *entries, size_t count);

/**
 * @brief 获取错误链的本地化JSON表示
 *
 * @param[in] chain 错误链
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return JSON字符串（需调用者释放）
 */
char *airy_err_chain_to_json_i18n(const airy_err_chain_t *chain, airy_language_t lang);


/**
 * @brief 错误链迭代器
 */
typedef struct {
    const airy_err_chain_t *chain;
    size_t current_index;
} airy_err_chain_iterator_t;

/**
 * @brief 初始化错误链迭代器
 *
 * @param[in] chain 错误链
 * @param[out] iter 迭代器
 */
void airy_err_chain_iter_init(const airy_err_chain_t *chain, airy_err_chain_iterator_t *iter);

/**
 * @brief 获取下一个错误上下文条目
 *
 * @param[inout] iter 迭代器
 * @return 下一个条目指针，如果没有更多条目返回NULL
 */
const airy_err_context_entry_t *airy_err_chain_iter_next(airy_err_chain_iterator_t *iter);

/**
 * @brief 重置错误链迭代器
 *
 * @param[inout] iter 迭代器
 */
void airy_err_chain_iter_reset(airy_err_chain_iterator_t *iter);

/**
 * @brief 获取错误链深度
 *
 * @param[in] chain 错误链
 * @return 链深度
 */
int airy_err_chain_get_depth(const airy_err_chain_t *chain);

/**
 * @brief 获取错误链中最早的错误码
 *
 * @param[in] chain 错误链
 * @return 最早的错误码
 */
airy_err_t airy_err_chain_get_root_error(const airy_err_chain_t *chain);

/**
 * @brief 获取错误链中最新的错误码
 *
 * @param[in] chain 错误链
 * @return 最新的错误码
 */
airy_err_t airy_err_chain_get_latest_error(const airy_err_chain_t *chain);

/**
 * @brief 将错误链格式化为可读字符串
 *
 * @param[in] chain 错误链
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return 格式化字符串（需调用者释放）
 */
char *airy_err_chain_format(const airy_err_chain_t *chain, airy_language_t lang);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_ERROR_H */
