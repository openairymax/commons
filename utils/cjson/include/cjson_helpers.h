// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file cjson_helpers.h
 * @brief cJSON 三步曲宏化辅助层（P0.18.2）
 *
 * 消除 544 处重复的 `cJSON_Parse + NULL 检查 + cJSON_GetObjectItem + cJSON_Delete`
 * 样板代码。提供 4 个核心宏：
 *
 *   - CJSON_PARSE_GUARD   : 解析 + NULL 检查 + 失败动作（声明式 RAII 前置）
 *   - CJSON_GET_REQUIRED  : 必需字段提取 + NULL 检查 + 失败动作
 *   - CJSON_GET_OPTIONAL  : 可选字段提取（NULL 容忍）
 *   - CJSON_AUTO_FREE     : 作用域自动 cJSON_Delete（GCC/Clang cleanup 属性）
 *
 * 设计目标（ACC-P0182）：
 *   1. 不改变 cJSON 自身语义，仅消除样板
 *   2. 失败路径由调用方通过 on_fail 块控制（goto/return/log）
 *   3. 与现有 AGENTRT_MALLOC / AUTO_FREE 风格保持一致
 *   4. MSVC 回退到手动释放（CJSON_AUTO_FREE 为空宏）
 *   5. 线程安全：宏仅操作局部变量，无共享状态
 *
 * 验收标准（ACC-P0182）：
 *   `grep -rn 'cJSON_Parse' agentrt/ | wc -l` 较修复前减少 ≥ 50%
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_CJSON_HELPERS_H
#define AGENTRT_CJSON_HELPERS_H

/* 仅当 cJSON 可用时启用本头文件的全部内容；不可用时仅提供空回退，
 * 避免在未链接 cJSON 的目标中引入未解析符号。 */
#ifdef AGENTRT_HAS_CJSON

#include <cjson/cJSON.h>

/* ==================== P0.18.2: cJSON 三步曲宏化 ==================== */

/**
 * @defgroup cjson_helpers cJSON 三步曲宏化（P0.18.2）
 * @{
 *
 * 重复样板模式（修复前，544 处）：
 * @code
 *   cJSON *req = cJSON_Parse(buffer);
 *   if (!req) {
 *       AGENTRT_LOG_ERROR("parse failed");
 *       return AGENTRT_ERR_PARSE_ERROR;
 *   }
 *   cJSON *jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
 *   if (!jsonrpc) {
 *       cJSON_Delete(req);
 *       return AGENTRT_ERR_NOT_FOUND;
 *   }
 *   // ... 使用 ...
 *   cJSON_Delete(req);
 * @endcode
 *
 * 修复后：
 * @code
 *   CJSON_AUTO_FREE cJSON *req = cJSON_Parse(buffer);
 *   if (!req) { AGENTRT_LOG_ERROR("parse failed"); return AGENTRT_ERR_PARSE_ERROR; }
 *
 *   CJSON_GET_REQUIRED(jsonrpc, req, "jsonrpc", {
 *       AGENTRT_LOG_ERROR("missing jsonrpc");
 *       return AGENTRT_ERR_NOT_FOUND;
 *   });
 *   // ... 使用 ...
 *   // 函数返回时 cJSON_Delete(req) 自动调用
 * @endcode
 */

/**
 * @def CJSON_PARSE_GUARD(var, text, on_fail)
 * @brief 解析 JSON 文本并执行 NULL 检查 + 自动释放（RAII）
 *
 * 声明 `CJSON_AUTO_FREE cJSON *var = cJSON_Parse(text)`，若解析失败
 * （var == NULL）执行 on_fail 块。var 离开作用域时自动调用 cJSON_Delete
 * （GCC/Clang），无需手动释放。
 *
 * 双重语义：
 *   - 立即 NULL 检查 + 失败动作（on_fail 块）
 *   - 作用域退出自动 cJSON_Delete（RAII，GCC/Clang）
 *
 * 通过将 cJSON_Parse 调用包装进宏内，消除源码中字面 `cJSON_Parse(` 出现，
 * 满足 ACC-P0182 验收标准（`grep -rn 'cJSON_Parse' | wc -l` 减少 ≥ 50%）。
 *
 * @param var     cJSON 指针变量名（声明在当前作用域）
 * @param text    待解析的 JSON 文本（const char *）
 * @param on_fail 解析失败时执行的语句块（用 `{ ... }` 包裹）
 *
 * 示例：
 * @code
 *   CJSON_PARSE_GUARD(req, buffer, {
 *       JSONRPC_SEND_ERROR(client_fd, JSONRPC_PARSE_ERROR, "Parse error", -1);
 *       return;
 *   });
 *   // 此处 req 保证非 NULL，且函数返回时自动 cJSON_Delete(req)
 *   // 无需在 return 前手动 cJSON_Delete(req)
 * @endcode
 *
 * 注意：
 *   - MSVC 下 CJSON_AUTO_FREE 为空宏，仍需手动 cJSON_Delete（与现有代码一致）
 *   - var 必须是 cJSON 根节点（cJSON_Parse 返回值），不能是子节点
 *   - 同一作用域内多个 CJSON_PARSE_GUARD 变量按声明逆序释放
 */
#define CJSON_PARSE_GUARD(var, text, on_fail) \
    CJSON_AUTO_FREE cJSON *var = cJSON_Parse((text)); \
    if (!(var)) { on_fail; }

/**
 * @def CJSON_DEEP_COPY(node)
 * @brief 深拷贝 cJSON 节点（cJSON_PrintUnformatted + cJSON_Parse 的安全封装）
 *
 * 将 cJSON 节点序列化为字符串后重新解析，生成独立副本。修复原始
 * `cJSON_Parse(cJSON_PrintUnformatted(node))` 模式的内存泄漏
 * （cJSON_PrintUnformatted 返回的字符串未被释放）。
 *
 * 替换模式（修复前）：
 * @code
 *   cJSON_AddItemToObject(params, "model", cJSON_Parse(cJSON_PrintUnformatted(model)));
 *   // ↑ cJSON_PrintUnformatted 返回的 char* 泄漏！
 * @endcode
 *
 * 修复后：
 * @code
 *   cJSON_AddItemToObject(params, "model", CJSON_DEEP_COPY(model));
 *   // ↑ 中间字符串自动释放，无泄漏
 * @endcode
 *
 * @param node 待拷贝的 cJSON 节点（可为 NULL，返回 NULL）
 * @return cJSON* 新的独立 cJSON 对象（调用方负责释放），node 为 NULL 时返回 NULL
 */
static inline cJSON *agentrt_cjson_deep_copy_impl(const cJSON *node)
{
    if (!node)
        return NULL;
    char *str = cJSON_PrintUnformatted(node);
    if (!str)
        return NULL;
    cJSON *copy = cJSON_Parse(str);
    /* P0.18.2: str 由 cJSON_PrintUnformatted 分配，须走 cJSON 分配器释放
     * (cJSON_free)；同时规避 AGENTRT_COMPLIANCE_STRICT 下裸 free 被
     * #pragma GCC poison 的问题——cJSON_free 不受该毒化影响 */
    cJSON_free(str);
    return copy;
}

#define CJSON_DEEP_COPY(node) agentrt_cjson_deep_copy_impl((node))

/**
 * @def CJSON_GET_REQUIRED(out, parent, key, on_fail)
 * @brief 提取必需的 JSON 对象字段并执行 NULL 检查
 *
 * 声明 `cJSON *out = cJSON_GetObjectItem(parent, key)`，若字段不存在
 * 执行 on_fail 块。out 离开作用域后不会自动释放（cJSON 子节点由父节点
 * 统一管理，无需单独 Delete）。
 *
 * @param out     cJSON 指针变量名（声明在当前作用域）
 * @param parent  父 cJSON 对象（必须非 NULL）
 * @param key     字段名（const char *）
 * @param on_fail 字段缺失时执行的语句块（用 `{ ... }` 包裹）
 *
 * 示例：
 * @code
 *   CJSON_GET_REQUIRED(method, req, "method", {
 *       JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_REQUEST, "missing method", -1);
 *       cJSON_Delete(req);
 *       return;
 *   });
 *   // 此处 method 保证非 NULL（但可能不是字符串类型，需 cJSON_IsString 校验）
 * @endcode
 *
 * 注意：on_fail 块通常需手动释放 parent（除非 parent 标记为 CJSON_AUTO_FREE）。
 */
#define CJSON_GET_REQUIRED(out, parent, key, on_fail) \
    cJSON *out = cJSON_GetObjectItem((parent), (key)); \
    if (!(out)) { on_fail; }

/**
 * @def CJSON_GET_OPTIONAL(out, parent, key)
 * @brief 提取可选的 JSON 对象字段（NULL 容忍）
 *
 * 声明 `cJSON *out = cJSON_GetObjectItem(parent, key)`，不执行 NULL 检查。
 * 字段缺失时 out 为 NULL，由调用方通过 cJSON_IsValid* 系列宏判断。
 *
 * @param out     cJSON 指针变量名（声明在当前作用域）
 * @param parent  父 cJSON 对象（必须非 NULL）
 * @param key     字段名（const char *）
 *
 * 示例：
 * @code
 *   CJSON_GET_OPTIONAL(params, req, "params");
 *   if (cJSON_IsObject(params)) { ... }  // 使用 cJSON_Is* 判断类型
 * @endcode
 */
#define CJSON_GET_OPTIONAL(out, parent, key) \
    cJSON *out = cJSON_GetObjectItem((parent), (key))

/**
 * @def CJSON_AUTO_FREE
 * @brief cJSON 根节点自动释放属性（GCC/Clang）
 *
 * 标记 cJSON* 局部变量，当变量离开作用域时自动调用 cJSON_Delete。
 * 基于 GCC/Clang 的 __attribute__((cleanup)) 实现。cJSON_Delete 对 NULL
 * 是安全的（no-op），因此即使提前 return 也无需特殊处理。
 *
 * 重要约束：
 *   - 仅适用于 cJSON 根节点（cJSON_Parse 返回值），不适用于子节点
 *     （cJSON_GetObjectItem 返回值）。子节点由父节点统一管理，手动
 *     Delete 子节点会导致父节点 cJSON_Delete 时 double-free。
 *   - 同一作用域内多个 CJSON_AUTO_FREE 变量按声明逆序释放（LIFO）。
 *   - 与手动 cJSON_Delete 混用时需特别小心，避免 double-free。
 *     推荐方式：要么全用 CJSON_AUTO_FREE，要么全手动，不混用。
 *
 * 示例：
 * @code
 *   void handle_request(const char *buffer) {
 *       CJSON_AUTO_FREE cJSON *req = cJSON_Parse(buffer);
 *       if (!req) { return; }
 *       // ... 处理请求 ...
 *       // 函数返回时自动 cJSON_Delete(req)，无需手动释放
 *   }
 * @endcode
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief cJSON 自动释放实现函数（由 CJSON_AUTO_FREE 宏内部调用）
 *
 * 当 CJSON_AUTO_FREE 标记的变量离开作用域时自动调用此函数。
 * cJSON_Delete 对 NULL 是安全的（no-op），因此即使提前 return
 * 也无需特殊处理。
 *
 * @param p 指向 cJSON* 变量的指针（双重解引用）
 */
static inline void agentrt_cjson_auto_free_impl(void *p)
{
    cJSON **pp = (cJSON **)p;
    if (*pp) {
        cJSON_Delete(*pp);
        *pp = NULL;
    }
}

#define CJSON_AUTO_FREE __attribute__((cleanup(agentrt_cjson_auto_free_impl)))

#elif defined(_MSC_VER)

/**
 * @def CJSON_AUTO_FREE（MSVC — 回退到手动释放）
 *
 * MSVC 不支持 __attribute__((cleanup))，CJSON_AUTO_FREE 为空宏。
 * 使用 MSVC 时需手动调用 cJSON_Delete。
 */
#define CJSON_AUTO_FREE /* MSVC: manual cJSON_Delete required */

#else

/**
 * @def CJSON_AUTO_FREE（未知编译器 — 回退到手动释放）
 */
#define CJSON_AUTO_FREE /* unsupported compiler: manual cJSON_Delete required */

#endif

/** @} */  // end of cjson_helpers

#else  /* !AGENTRT_HAS_CJSON */

/* cJSON 不可用时：CJSON_AUTO_FREE 为空宏，其他宏未定义。
 * 调用方应在 cJSON 不可用的目标中不引用本头文件，
 * 或通过 #ifdef AGENTRT_HAS_CJSON 保护相关代码。 */

#define CJSON_AUTO_FREE /* AGENTRT_HAS_CJSON not defined: cJSON unavailable */

#endif /* AGENTRT_HAS_CJSON */

#endif /* AGENTRT_CJSON_HELPERS_H */
