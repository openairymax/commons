/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file err_class.h
 * @brief ErrorModel v1 错误分类唯一真相源（SSoT）：纯机制件。
 *
 * 三层责任模型（0.1.19 架构改进方案 §3.7.2 N1 线）：
 *   - L0 产生层（各 daemon）：每个失败点产出三元组 {class, code, diag}，
 *     禁止裸返回；class 由 airy_err_class_of() 从错误码确定性推导。
 *   - L1 传输层（IPC/gateway）：只标记自身失败（socket 断连/超时），绝不
 *     改写内层 class/code/diag。
 *   - L2 呈现层（CLI/TUI）：按 class 映射呈现样式，禁止跨 class 合并文案。
 *
 * 契约：
 *   - "transport" 是「服务不可达」文案唯一允许的呈现类（修复 B1/B2）；
 *     business/policy/config/internal 一律透传 code+diag。
 *   - 跨进程承载于 JSON-RPC 载荷 "err" 键（非 C 结构直传），旧消费者
 *     忽略该键即兼容。
 *   - 分类器对错误码做符号归一（正负幅值等价），仅对无歧义语义码显式
 *     分类，其余一律默认 "business"——绝不臆造「服务不可达」。
 *
 * 依据：0.1.19 架构改进方案 §3.7.2 / §3.7.5（N1）。
 */

#ifndef AIRY_RT_UTILS_ERROR_CLASS_H
#define AIRY_RT_UTILS_ERROR_CLASS_H

#include "../types/types.h"

#include <stddef.h>

/* cJSON 仅以不透明指针出现在契约中；由调用方包含 <cjson/cJSON.h>，
 * 避免 error 子域公共头对 JSON 实现产生编译期依赖。 */
struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 错误类别枚举（唯一权威）。
 *
 * 仅 AIRY_ERR_CLASS_TRANSPORT 允许呈现为「服务不可达 / 可重试」；
 * 其余类别必须透传具体诊断，禁止跨类合并文案。
 */
typedef enum {
    AIRY_ERR_CLASS_OK = 0,        /**< 成功，无错误 */
    AIRY_ERR_CLASS_TRANSPORT = 1, /**< 连接失败/超时/断连，可重试 */
    AIRY_ERR_CLASS_BUSINESS = 2,  /**< 工具/业务执行失败，不可重试 */
    AIRY_ERR_CLASS_POLICY = 3,    /**< 沙箱/审批/权限拒绝 */
    AIRY_ERR_CLASS_CONFIG = 4,    /**< 配置缺失/非法 */
    AIRY_ERR_CLASS_INTERNAL = 5   /**< 断言失败/bug，附 trace id 指引上报 */
} airy_err_class_t;

/**
 * @brief 类别 → 稳定小写字符串名。
 * @param cls 类别。
 * @return 静态常量字符串；越界返回 "unknown"。
 */
const char *airy_err_class_name(airy_err_class_t cls);

/**
 * @brief 字符串名 → 类别（json 反序列化用）。
 * @param name 类别名（NULL 或未知）。
 * @return 解析结果；NULL/未知一律返回 AIRY_ERR_CLASS_BUSINESS（安全默认，
 *         与 airy_err_class_of 默认一致，永不臆造 transport）。
 */
airy_err_class_t airy_err_class_parse(const char *name);

/**
 * @brief 错误码 → 类别（确定性推导，符号归一）。
 * @param code 错误码（正幅值或负值等价）。
 * @return 类别；0 返回 OK，无可判定语义者返回 BUSINESS。
 */
airy_err_class_t airy_err_class_of(airy_err_t code);

/**
 * @brief 将 {class, code, diag, [ctx]} 作为 "err" 键写入 parent 对象。
 * @param parent 目标 JSON 对象（须为 object），不可为 NULL。
 * @param cls    错误类别。
 * @param code   原始错误码。
 * @param diag   人类可读诊断（NULL 视为空串）；应含路径/参数等定位信息。
 * @param ctx    可选附加上下文对象（深拷贝），可为 NULL。
 * @return 0 成功；-1 失败（parent 未改动或已清理）。
 */
int airy_err_json_put(struct cJSON *parent, airy_err_class_t cls, airy_err_t code,
                      const char *diag, const struct cJSON *ctx);

/**
 * @brief 从 JSON 对象读取 "err" 键（缺省安全回退）。
 * @param errobj   承载 "err" 的对象（须为 object）；若含 "err" 子对象则下钻。
 * @param out_cls  输出类别，可为 NULL。
 * @param out_code 输出错误码，可为 NULL；缺省 AIRY_ERR_UNKNOWN。
 * @param diag_buf 输出诊断缓冲，可为 NULL。
 * @param diag_cap diag_buf 容量；不足时安全截断并 NUL 结尾。
 * @param out_ctx  输出 ctx 对象指针（借用，勿释放），可为 NULL。
 * @return 0 成功；-1 errobj 非对象或未找到错误信封（非错误路径）。
 */
int airy_err_json_get(const struct cJSON *errobj, airy_err_class_t *out_cls,
                      airy_err_t *out_code, char *diag_buf, size_t diag_cap,
                      const struct cJSON **out_ctx);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_ERROR_CLASS_H */
