/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file error_codes.h
 * @brief AgentRT error code definitions.
 */

#ifndef AIRY_RT_UTILS_ERROR_CODES_H
#define AIRY_RT_UTILS_ERROR_CODES_H

#include "../types/types.h"

/*
 * 守卫策略（fail-closed，禁止改回 #ifndef 弱守卫）：
 * 本头全部宏采用冲突检测式守卫——#ifdef 命中即 #error。头卫兵保证
 * 本头单次处理，#ifdef 命中只可能来自头外影子定义（0.1.19 D 组
 * cupolas_ERROR_WOULD_BLOCK 三重定义遮蔽缺陷的同类温床），编译期
 * 立即硬失败。取值以本头为唯一权威（SSoT）。
 */

/*
 * AIRY_OK 兼容宏（v4.0 修复 v3.0 副作用）：
 * v3.0 曾移除 AIRY_OK 定义以统一使用 AIRY_EOK，但 39 文件/241 处仍引用
 * AIRY_OK，导致全仓库编译破坏。v4.0 恢复 AIRY_OK 作为兼容宏，与
 * AIRY_EOK/AIRY_SUCCESS 等价（均 = 0）。
 * 新代码推荐使用 AIRY_EOK；AIRY_OK 保留供存量代码兼容，M1 阶段渐进迁移。
 */
#ifdef AIRY_OK
#error "AIRY_OK: duplicate SSoT definition"
#endif
#define AIRY_OK 0

#ifdef AIRY_ERR_UNKNOWN
#error "AIRY_ERR_UNKNOWN: duplicate SSoT definition"
#endif
#define AIRY_ERR_UNKNOWN (-99)

/*
 * 权威源说明（SSoT 分层）：
 *
 * POSIX 风格错误码（AIRY_EINVAL / AIRY_ENOMEM / AIRY_EBUSY 等）的权威
 * 定义位于 airy_types.h（经 types.h 先于本头引入；对 airymax/error.h
 * [SC] 正幅值做 #undef + 负幅值重定），本头不重复。历史 #ifndef 别名层
 * 中 AIRY_ECANCELED/AIRY_EINTR 因恒被 airy_types.h 硬定义遮蔽，确认
 * 死代码并删除（0.1.19 E2 取证：airy_types.h:72/-125、:46/-4）。
 *
 * 本头仅保有两类活跃别名（均为全项目唯一定义点）：
 *   1. types.h 未定义的扩展别名：AIRY_EBADF/AIRY_ERESOURCE/
 *      AIRY_ESECURITY/AIRY_ESANITIZE（锚定下方 AIRY_ERR_* 扩展码）
 *   2. types.h 未定义的 POSIX 别名：AIRY_ENOTDIR/AIRY_ENAMETOOLONG
 *
 * 已知技术债（计划 1.0.1 M1 消除）：AIRY_ERR_* 扩展码与 AIRY_E* POSIX 码
 * 在数值区间仍有部分语义重叠（如 AIRY_ERR_PROTOCOL=-900 与 AIRY_EPROTO=-71）。
 * 调用方应始终使用语义宏，严禁与字面量直接比较。
 */
#ifdef AIRY_EBADF
#error "AIRY_EBADF: duplicate SSoT definition"
#endif
#define AIRY_EBADF AIRY_ERR_SYS_FILE
#ifdef AIRY_ERESOURCE
#error "AIRY_ERESOURCE: duplicate SSoT definition"
#endif
#define AIRY_ERESOURCE AIRY_ERR_SYS_RESOURCE
#ifdef AIRY_ESECURITY
#error "AIRY_ESECURITY: duplicate SSoT definition"
#endif
#define AIRY_ESECURITY AIRY_ERR_ESECURITY
#ifdef AIRY_ESANITIZE
#error "AIRY_ESANITIZE: duplicate SSoT definition"
#endif
#define AIRY_ESANITIZE AIRY_ERR_ESANITIZE

#ifdef AIRY_ENOTDIR
#error "AIRY_ENOTDIR: duplicate SSoT definition"
#endif
#define AIRY_ENOTDIR (-20) /* POSIX ENOTDIR=20 */
#ifdef AIRY_ENAMETOOLONG
#error "AIRY_ENAMETOOLONG: duplicate SSoT definition"
#endif
#define AIRY_ENAMETOOLONG (-36) /* POSIX ENAMETOOLONG=36 */

/*
 * 错误码分段规划：
 *   -1 到 -99:      通用基础错误
 *   -100 到 -999:   系统与平台错误
 *   -1000 到 -1999: 内核层错误
 *   -2000 到 -2999: 服务层错误
 *   -3000 到 -3999: LLM/AI服务错误
 *   -4000 到 -4999: 执行/工具错误
 *   -5000 到 -5999: 调度错误
 *   -6000 到 -6999: 记忆/存储错误
 *   -7000 到 -7999: 安全/沙箱错误
 */

/* 通用基础错误 (-1 到 -99)
 *
 * v3.0 SSoT 统一收敛：与 POSIX errno 负值冲突的 AIRY_ERR_* 扩展码
 * 已迁移至 -40~-50 区间（原 -2/-5/-7/-10/-11/-12/-13/-16/-17 与
 * airy_types.h POSIX 码冲突；v4.0 追加 -4/-14 迁移至 -49/-50 以避让
 * AIRY_EINTR/AIRY_EFAULT）。
 *
 * v4.3 二次迁移：-40~-50 区间与 [SC] IPC 码空间 [-41, -70] 存在
 * 值碰撞（AIRY_ERR_* vs AIRY_EIPC_* 同为 -45~-50），迁移至
 * -36~-40（5 个）和 -55~-60（6 个）两个子区间，彻底消除碰撞。
 * 未冲突的保留原值。 */
#ifdef AIRY_ERR_INVALID_PARAM
#error "AIRY_ERR_INVALID_PARAM: duplicate SSoT definition"
#endif
#define AIRY_ERR_INVALID_PARAM (-36)
#ifdef AIRY_ERR_NULL_POINTER
#error "AIRY_ERR_NULL_POINTER: duplicate SSoT definition"
#endif
#define AIRY_ERR_NULL_POINTER (-3)
#ifdef AIRY_ERR_OUT_OF_MEMORY
#error "AIRY_ERR_OUT_OF_MEMORY: duplicate SSoT definition"
#endif
#define AIRY_ERR_OUT_OF_MEMORY (-59)
#ifdef AIRY_ERR_BUFFER_TOO_SMALL
#error "AIRY_ERR_BUFFER_TOO_SMALL: duplicate SSoT definition"
#endif
#define AIRY_ERR_BUFFER_TOO_SMALL (-37)
#ifdef AIRY_ERR_NOT_FOUND
#error "AIRY_ERR_NOT_FOUND: duplicate SSoT definition"
#endif
#define AIRY_ERR_NOT_FOUND (-6)
#ifdef AIRY_ERR_ALREADY_EXISTS
#error "AIRY_ERR_ALREADY_EXISTS: duplicate SSoT definition"
#endif
#define AIRY_ERR_ALREADY_EXISTS (-38)
#ifdef AIRY_ERR_TIMEOUT
#error "AIRY_ERR_TIMEOUT: duplicate SSoT definition"
#endif
#define AIRY_ERR_TIMEOUT (-8)
#ifdef AIRY_ERR_NOT_SUPPORTED
#error "AIRY_ERR_NOT_SUPPORTED: duplicate SSoT definition"
#endif
#define AIRY_ERR_NOT_SUPPORTED (-9)
#ifdef AIRY_ERR_PERMISSION_DENIED
#error "AIRY_ERR_PERMISSION_DENIED: duplicate SSoT definition"
#endif
#define AIRY_ERR_PERMISSION_DENIED (-39)
#ifdef AIRY_ERR_IO
#error "AIRY_ERR_IO: duplicate SSoT definition"
#endif
#define AIRY_ERR_IO (-40)
#ifdef AIRY_ERR_PARSE_ERROR
#error "AIRY_ERR_PARSE_ERROR: duplicate SSoT definition"
#endif
#define AIRY_ERR_PARSE_ERROR (-55)
#ifdef AIRY_ERR_STATE_ERROR
#error "AIRY_ERR_STATE_ERROR: duplicate SSoT definition"
#endif
#define AIRY_ERR_STATE_ERROR (-56)
#ifdef AIRY_ERR_OVERFLOW
#error "AIRY_ERR_OVERFLOW: duplicate SSoT definition"
#endif
#define AIRY_ERR_OVERFLOW (-60)
#ifdef AIRY_ERR_UNDERFLOW
#error "AIRY_ERR_UNDERFLOW: duplicate SSoT definition"
#endif
#define AIRY_ERR_UNDERFLOW (-15)
#ifdef AIRY_ERR_CANCELED
#error "AIRY_ERR_CANCELED: duplicate SSoT definition"
#endif
#define AIRY_ERR_CANCELED (-57)
#ifdef AIRY_ERR_BUSY
#error "AIRY_ERR_BUSY: duplicate SSoT definition"
#endif
#define AIRY_ERR_BUSY (-58)
#ifdef AIRY_ERR_WOULD_BLOCK
#error "AIRY_ERR_WOULD_BLOCK: duplicate SSoT definition"
#endif
#define AIRY_ERR_WOULD_BLOCK (-18)
#ifdef AIRY_ERR_INTERRUPTED
#error "AIRY_ERR_INTERRUPTED: duplicate SSoT definition"
#endif
#define AIRY_ERR_INTERRUPTED (-19)

#ifdef AIRY_ERR_NOT_IMPLEMENTED
#error "AIRY_ERR_NOT_IMPLEMENTED: duplicate SSoT definition"
#endif
#define AIRY_ERR_NOT_IMPLEMENTED (-30)
/* S-1 收敛 (2026-08-14)：AIRY_ERR_FAIL 错误码改名 AIRY_ERR_GENERIC_FAIL——
 * [SC] airymax/error.h 定义了同名函数式辅助宏 AIRY_ERR_FAIL(err)（失败检测，
 * 展开为 ((err) < 0)），错误码若同名则 include [SC] 后 #ifndef 恒不触发，
 * 错误码丢失且调用点会误触发函数式宏。改名消除命名冲突（A-UEF 对齐）。 */
#ifdef AIRY_ERR_GENERIC_FAIL
#error "AIRY_ERR_GENERIC_FAIL: duplicate SSoT definition"
#endif
#define AIRY_ERR_GENERIC_FAIL (-31)

/* 系统与平台错误 (-100 到 -199)
 *
 * v3.0 SSoT 统一收敛：AIRY_ERR_SYS_THREAD/CONDITION/PIPE/PROCESS 原值
 * -104/-107/-110/-111 与 airy_types.h POSIX 码（ECONNRESET/ENOTCONN/
 * ETIMEDOUT/ECONNREFUSED）冲突，迁移至 -120~-123。 */
#ifdef AIRY_ERR_SYS_BASE
#error "AIRY_ERR_SYS_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_BASE (-100)
#ifdef AIRY_ERR_SYS_NOT_INIT
#error "AIRY_ERR_SYS_NOT_INIT: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_NOT_INIT (-101)
#ifdef AIRY_ERR_SYS_RESOURCE
#error "AIRY_ERR_SYS_RESOURCE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_RESOURCE (-102)
#ifdef AIRY_ERR_SYS_DEADLOCK
#error "AIRY_ERR_SYS_DEADLOCK: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_DEADLOCK (-103)
#ifdef AIRY_ERR_SYS_THREAD
#error "AIRY_ERR_SYS_THREAD: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_THREAD (-120)
#ifdef AIRY_ERR_SYS_MUTEX
#error "AIRY_ERR_SYS_MUTEX: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_MUTEX (-105)
#ifdef AIRY_ERR_SYS_SEMAPHORE
#error "AIRY_ERR_SYS_SEMAPHORE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_SEMAPHORE (-106)
#ifdef AIRY_ERR_SYS_CONDITION
#error "AIRY_ERR_SYS_CONDITION: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_CONDITION (-121)
#ifdef AIRY_ERR_SYS_ATOMIC
#error "AIRY_ERR_SYS_ATOMIC: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_ATOMIC (-108)
#ifdef AIRY_ERR_SYS_SOCKET
#error "AIRY_ERR_SYS_SOCKET: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_SOCKET (-109)
#ifdef AIRY_ERR_SYS_PIPE
#error "AIRY_ERR_SYS_PIPE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_PIPE (-122)
#ifdef AIRY_ERR_SYS_PROCESS
#error "AIRY_ERR_SYS_PROCESS: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_PROCESS (-123)
#ifdef AIRY_ERR_SYS_FILE
#error "AIRY_ERR_SYS_FILE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_FILE (-112)
#ifdef AIRY_ERR_SYS_TIME
#error "AIRY_ERR_SYS_TIME: duplicate SSoT definition"
#endif
#define AIRY_ERR_SYS_TIME (-113)

#ifdef AIRY_ERR_KERN_BASE
#error "AIRY_ERR_KERN_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_BASE (-200)
#ifdef AIRY_ERR_KERN_IPC
#error "AIRY_ERR_KERN_IPC: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_IPC (-201)
#ifdef AIRY_ERR_KERN_TASK
#error "AIRY_ERR_KERN_TASK: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_TASK (-202)
#ifdef AIRY_ERR_KERN_SYNC
#error "AIRY_ERR_KERN_SYNC: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_SYNC (-203)
#ifdef AIRY_ERR_KERN_LOCK
#error "AIRY_ERR_KERN_LOCK: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_LOCK (-204)
#ifdef AIRY_ERR_KERN_MEM
#error "AIRY_ERR_KERN_MEM: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_MEM (-205)
#ifdef AIRY_ERR_KERN_SCHED
#error "AIRY_ERR_KERN_SCHED: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_SCHED (-206)
#ifdef AIRY_ERR_KERN_TIMER
#error "AIRY_ERR_KERN_TIMER: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_TIMER (-207)
#ifdef AIRY_ERR_KERN_INTERRUPT
#error "AIRY_ERR_KERN_INTERRUPT: duplicate SSoT definition"
#endif
#define AIRY_ERR_KERN_INTERRUPT (-208)

#ifdef AIRY_ERR_SVC_BASE
#error "AIRY_ERR_SVC_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_BASE (-300)
#ifdef AIRY_ERR_SVC_NOT_READY
#error "AIRY_ERR_SVC_NOT_READY: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_NOT_READY (-301)
#ifdef AIRY_ERR_SVC_BUSY
#error "AIRY_ERR_SVC_BUSY: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_BUSY (-302)
#ifdef AIRY_ERR_SVC_STOPPED
#error "AIRY_ERR_SVC_STOPPED: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_STOPPED (-303)
#ifdef AIRY_ERR_SVC_CONFIG
#error "AIRY_ERR_SVC_CONFIG: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_CONFIG (-304)
#ifdef AIRY_ERR_SVC_DEPENDENCY
#error "AIRY_ERR_SVC_DEPENDENCY: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_DEPENDENCY (-305)
#ifdef AIRY_ERR_SVC_HEALTH
#error "AIRY_ERR_SVC_HEALTH: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_HEALTH (-306)
#ifdef AIRY_ERR_SVC_LOADBALANCE
#error "AIRY_ERR_SVC_LOADBALANCE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_LOADBALANCE (-307)
#ifdef AIRY_ERR_SVC_CYCLE
#error "AIRY_ERR_SVC_CYCLE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SVC_CYCLE (-308)
#ifdef AIRY_ERR_CYCLE_DETECTED
#error "AIRY_ERR_CYCLE_DETECTED: duplicate SSoT definition"
#endif
#define AIRY_ERR_CYCLE_DETECTED AIRY_ERR_SVC_CYCLE

#ifdef AIRY_ERR_LLM_BASE
#error "AIRY_ERR_LLM_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_BASE (-400)
#ifdef AIRY_ERR_LLM_NO_PROVIDER
#error "AIRY_ERR_LLM_NO_PROVIDER: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_NO_PROVIDER (-401)
#ifdef AIRY_ERR_LLM_PROVIDER_FAIL
#error "AIRY_ERR_LLM_PROVIDER_FAIL: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_PROVIDER_FAIL (-402)
#ifdef AIRY_ERR_LLM_RATE_LIMIT
#error "AIRY_ERR_LLM_RATE_LIMIT: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_RATE_LIMIT (-403)
#ifdef AIRY_ERR_LLM_CONTEXT_LEN
#error "AIRY_ERR_LLM_CONTEXT_LEN: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_CONTEXT_LEN (-404)
#ifdef AIRY_ERR_LLM_INVALID_MODEL
#error "AIRY_ERR_LLM_INVALID_MODEL: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_INVALID_MODEL (-405)
#ifdef AIRY_ERR_LLM_AUTH_FAIL
#error "AIRY_ERR_LLM_AUTH_FAIL: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_AUTH_FAIL (-406)
#ifdef AIRY_ERR_LLM_TOKEN_LIMIT
#error "AIRY_ERR_LLM_TOKEN_LIMIT: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_TOKEN_LIMIT (-407)
#ifdef AIRY_ERR_LLM_PARSE_RESP
#error "AIRY_ERR_LLM_PARSE_RESP: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_PARSE_RESP (-408)
#ifdef AIRY_ERR_LLM_EMPTY_RESP
#error "AIRY_ERR_LLM_EMPTY_RESP: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_EMPTY_RESP (-409)
#ifdef AIRY_ERR_LLM_COST_EXCEED
#error "AIRY_ERR_LLM_COST_EXCEED: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_COST_EXCEED (-410)

/* GCCP 目标完备确认（-411）：控制流信号，非真实错误。
 *
 * 认知引擎 Phase 0 在 airy_gccp_probe 判定 need_interaction=1 且产品层
 * 交互回调返回 AIRY_GCCP_INTERACT_PENDING 哨兵时返回本码，表示"本轮处理
 * 挂起，等待用户补充答案"：引擎中止后续 Phase（1~4）避免在降级目标上
 * 浪费 token；调用方（think_d 等）捕获后把问题集回给客户端，待答案就绪
 * 携带 gccp_answers 重新发起处理（两段式交互协议）。 */
#ifdef AIRY_ERR_GCCP_INTERACTION
#error "AIRY_ERR_GCCP_INTERACTION: duplicate SSoT definition"
#endif
#define AIRY_ERR_GCCP_INTERACTION (-411)

/* Provider 请求体被拒绝（HTTP 400/422）：请求非法（如消息含无效 UTF-8
 * 或 JSON 结构错误），属确定性失败——重试不会成功，且与"网络不可达"
 * 有本质区别（见 llm_d 的 openai_rate_limit.c / provider_stream.c）。
 * 取值 -412 紧随 LLM 块（-400..-410；-411 已由 GCCP 交互信号占用），
 * 不复用 -402 AIRY_ERR_LLM_PROVIDER_FAIL：后者在 sched_d DAG 被归类为
 * 瞬态可重试，复用会让非法请求体被反复重试。 */
#ifdef AIRY_ERR_LLM_BAD_REQUEST
#error "AIRY_ERR_LLM_BAD_REQUEST: duplicate SSoT definition"
#endif
#define AIRY_ERR_LLM_BAD_REQUEST (-412)

#ifdef AIRY_ERR_EXEC_BASE
#error "AIRY_ERR_EXEC_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_BASE (-500)
#ifdef AIRY_ERR_EXEC_NOT_FOUND
#error "AIRY_ERR_EXEC_NOT_FOUND: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_NOT_FOUND (-501)
#ifdef AIRY_ERR_EXEC_FAIL
#error "AIRY_ERR_EXEC_FAIL: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_FAIL (-502)
#ifdef AIRY_ERR_EXEC_TIMEOUT
#error "AIRY_ERR_EXEC_TIMEOUT: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_TIMEOUT (-503)
#ifdef AIRY_ERR_EXEC_VALIDATION
#error "AIRY_ERR_EXEC_VALIDATION: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_VALIDATION (-504)
#ifdef AIRY_ERR_EXEC_SANDBOX
#error "AIRY_ERR_EXEC_SANDBOX: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_SANDBOX (-505)
#ifdef AIRY_ERR_EXEC_PERMISSION
#error "AIRY_ERR_EXEC_PERMISSION: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_PERMISSION (-506)
#ifdef AIRY_ERR_EXEC_ARGS
#error "AIRY_ERR_EXEC_ARGS: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_ARGS (-507)
#ifdef AIRY_ERR_EXEC_ENV
#error "AIRY_ERR_EXEC_ENV: duplicate SSoT definition"
#endif
#define AIRY_ERR_EXEC_ENV (-508)

#ifdef AIRY_ERR_MEM_BASE
#error "AIRY_ERR_MEM_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_BASE (-600)
#ifdef AIRY_ERR_MEM_WRITE
#error "AIRY_ERR_MEM_WRITE: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_WRITE (-601)
#ifdef AIRY_ERR_MEM_READ
#error "AIRY_ERR_MEM_READ: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_READ (-602)
#ifdef AIRY_ERR_MEM_QUERY
#error "AIRY_ERR_MEM_QUERY: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_QUERY (-603)
#ifdef AIRY_ERR_MEM_EVOLVE
#error "AIRY_ERR_MEM_EVOLVE: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_EVOLVE (-604)
#ifdef AIRY_ERR_MEM_FULL
#error "AIRY_ERR_MEM_FULL: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_FULL (-605)
#ifdef AIRY_ERR_MEM_CORRUPT
#error "AIRY_ERR_MEM_CORRUPT: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_CORRUPT (-606)
#ifdef AIRY_ERR_MEM_NOT_INIT
#error "AIRY_ERR_MEM_NOT_INIT: duplicate SSoT definition"
#endif
#define AIRY_ERR_MEM_NOT_INIT (-607)

#ifdef AIRY_ERR_SEC_BASE
#error "AIRY_ERR_SEC_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_BASE (-700)
#ifdef AIRY_ERR_SEC_VIOLATION
#error "AIRY_ERR_SEC_VIOLATION: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_VIOLATION (-701)
#ifdef AIRY_ERR_SEC_SANITIZE
#error "AIRY_ERR_SEC_SANITIZE: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_SANITIZE (-702)
#ifdef AIRY_ERR_SEC_AUDIT
#error "AIRY_ERR_SEC_AUDIT: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_AUDIT (-703)
#ifdef AIRY_ERR_SEC_PERMISSION
#error "AIRY_ERR_SEC_PERMISSION: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_PERMISSION (-704)
#ifdef AIRY_ERR_SEC_VALIDATION
#error "AIRY_ERR_SEC_VALIDATION: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_VALIDATION (-705)
#ifdef AIRY_ERR_SEC_QUOTA
#error "AIRY_ERR_SEC_QUOTA: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_QUOTA (-706)
#ifdef AIRY_ERR_SEC_TEMP_DIR
#error "AIRY_ERR_SEC_TEMP_DIR: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_TEMP_DIR (-707)
#ifdef AIRY_ERR_SEC_SYMLINK
#error "AIRY_ERR_SEC_SYMLINK: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_SYMLINK (-708)
#ifdef AIRY_ERR_SEC_PATH_TRAV
#error "AIRY_ERR_SEC_PATH_TRAV: duplicate SSoT definition"
#endif
#define AIRY_ERR_SEC_PATH_TRAV (-709)
#ifdef AIRY_ERR_ESECURITY
#error "AIRY_ERR_ESECURITY: duplicate SSoT definition"
#endif
#define AIRY_ERR_ESECURITY (-710)
#ifdef AIRY_ERR_ESANITIZE
#error "AIRY_ERR_ESANITIZE: duplicate SSoT definition"
#endif
#define AIRY_ERR_ESANITIZE (-711)

/* Cupolas 安全穹顶专属错误码 (-712 到 -799)
 *
 * P0.25.4 (ACC-STD06)：任务清单原要求 -700~-705 段，但 -700~-711 已被
 * AIRY_ERR_SEC_* 占用（v3.4 之前已定义）。为避免数值冲突，Cupolas 专属
 * 错误码段调整为 -712~-799。Cupolas 公共 API 仍可通过 cupolas_ERR_* enum
 * （数值与 AIRY_ERR_* 通用码一致，如 cupolas_ERR_OUT_OF_MEMORY=-59）
 * 返回通用错误码；本段仅定义 Cupolas 特有的语义错误（如沙箱隔离、策略拒绝、
 * 审计失败等），供 cupolas 模块内部和调用方区分错误来源。
 *
 * 段分配：
 *   -712  AIRY_ERR_CUPOLAS_BASE       段基址
 *   -713  AIRY_ERR_CUPOLAS_DENIED     权限/策略拒绝（Cupolas 决策）
 *   -714  AIRY_ERR_CUPOLAS_QUARANTINE 隔离/隔离区
 *   -715  AIRY_ERR_CUPOLAS_POLICY     策略评估失败
 *   -716  AIRY_ERR_CUPOLAS_SANDBOX    沙箱执行失败/逃逸检测
 *   -717  AIRY_ERR_CUPOLAS_AUDIT      审计日志写入失败
 *   -718  AIRY_ERR_CUPOLAS_TAMPERED   篡改检测
 *   -719  AIRY_ERR_CUPOLAS_SIGNATURE  签名验证失败
 *   -720  AIRY_ERR_CUPOLAS_VAULT      Vault 凭据访问失败
 *   -721  AIRY_ERR_CUPOLAS_ENTITLEMENT 权限声明无效
 *   -722  AIRY_ERR_CUPOLAS_RUNTIME    运行时保护违规
 *   -723  AIRY_ERR_CUPOLAS_NETWORK    网络安全策略拒绝
 *   -724~-799 预留扩展
 */
#ifdef AIRY_ERR_CUPOLAS_BASE
#error "AIRY_ERR_CUPOLAS_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_BASE (-712)
#ifdef AIRY_ERR_CUPOLAS_DENIED
#error "AIRY_ERR_CUPOLAS_DENIED: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_DENIED (-713)
#ifdef AIRY_ERR_CUPOLAS_QUARANTINE
#error "AIRY_ERR_CUPOLAS_QUARANTINE: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_QUARANTINE (-714)
#ifdef AIRY_ERR_CUPOLAS_POLICY
#error "AIRY_ERR_CUPOLAS_POLICY: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_POLICY (-715)
#ifdef AIRY_ERR_CUPOLAS_SANDBOX
#error "AIRY_ERR_CUPOLAS_SANDBOX: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_SANDBOX (-716)
#ifdef AIRY_ERR_CUPOLAS_AUDIT
#error "AIRY_ERR_CUPOLAS_AUDIT: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_AUDIT (-717)
#ifdef AIRY_ERR_CUPOLAS_TAMPERED
#error "AIRY_ERR_CUPOLAS_TAMPERED: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_TAMPERED (-718)
#ifdef AIRY_ERR_CUPOLAS_SIGNATURE
#error "AIRY_ERR_CUPOLAS_SIGNATURE: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_SIGNATURE (-719)
#ifdef AIRY_ERR_CUPOLAS_VAULT
#error "AIRY_ERR_CUPOLAS_VAULT: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_VAULT (-720)
#ifdef AIRY_ERR_CUPOLAS_ENTITLEMENT
#error "AIRY_ERR_CUPOLAS_ENTITLEMENT: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_ENTITLEMENT (-721)
#ifdef AIRY_ERR_CUPOLAS_RUNTIME
#error "AIRY_ERR_CUPOLAS_RUNTIME: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_RUNTIME (-722)
#ifdef AIRY_ERR_CUPOLAS_NETWORK
#error "AIRY_ERR_CUPOLAS_NETWORK: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_NETWORK (-723)
#ifdef AIRY_ERR_CUPOLAS_AUTH_FAILED
#error "AIRY_ERR_CUPOLAS_AUTH_FAILED: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_AUTH_FAILED (-724)
#ifdef AIRY_ERR_CUPOLAS_CERT_INVALID
#error "AIRY_ERR_CUPOLAS_CERT_INVALID: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_CERT_INVALID (-725)
#ifdef AIRY_ERR_CUPOLAS_CERT_EXPIRED
#error "AIRY_ERR_CUPOLAS_CERT_EXPIRED: duplicate SSoT definition"
#endif
#define AIRY_ERR_CUPOLAS_CERT_EXPIRED (-726)

#ifdef AIRY_ERR_COORD_BASE
#error "AIRY_ERR_COORD_BASE: duplicate SSoT definition"
#endif
#define AIRY_ERR_COORD_BASE (-800)
#ifdef AIRY_ERR_COORD_PLAN_FAIL
#error "AIRY_ERR_COORD_PLAN_FAIL: duplicate SSoT definition"
#endif
#define AIRY_ERR_COORD_PLAN_FAIL (-801)
#ifdef AIRY_ERR_COORD_SYNC_FAIL
#error "AIRY_ERR_COORD_SYNC_FAIL: duplicate SSoT definition"
#endif
#define AIRY_ERR_COORD_SYNC_FAIL (-802)
#ifdef AIRY_ERR_COORD_DISPATCH
#error "AIRY_ERR_COORD_DISPATCH: duplicate SSoT definition"
#endif
#define AIRY_ERR_COORD_DISPATCH (-803)
#ifdef AIRY_ERR_COORD_INTENT
#error "AIRY_ERR_COORD_INTENT: duplicate SSoT definition"
#endif
#define AIRY_ERR_COORD_INTENT (-804)
#ifdef AIRY_ERR_COORD_COMPENSATE
#error "AIRY_ERR_COORD_COMPENSATE: duplicate SSoT definition"
#endif
#define AIRY_ERR_COORD_COMPENSATE (-805)
#ifdef AIRY_ERR_COORD_RETRY_EXCEED
#error "AIRY_ERR_COORD_RETRY_EXCEED: duplicate SSoT definition"
#endif
#define AIRY_ERR_COORD_RETRY_EXCEED (-806)

/* 协议/校验错误 (-900 到 -909)
 *
 * P0.22.1 (ARE L2)：IPC Bus 统一消息头校验失败的专属错误码段。
 * - AIRY_ERR_PROTOCOL  magic/version/reserved 字段不匹配（消息必须丢弃）
 * - AIRY_ERR_CHECKSUM  CRC32 校验和不匹配（消息必须丢弃，不得回复 ERROR）
 * 详见 Docs/Capital_Specifications/are_standards/L2_service_protocol.md §2.3
 */
#ifdef AIRY_ERR_PROTOCOL
#error "AIRY_ERR_PROTOCOL: duplicate SSoT definition"
#endif
#define AIRY_ERR_PROTOCOL (-900)
#ifdef AIRY_ERR_CHECKSUM
#error "AIRY_ERR_CHECKSUM: duplicate SSoT definition"
#endif
#define AIRY_ERR_CHECKSUM (-901)

/* ============================================================================
 * AIRY_EIPC_* / AIRY_ECAP_* / AIRY_FAULT_* —— 统一引用 A-UEF [SC] 唯一权威
 *
 * S-1 收敛 (2026-08-14)：此前本文件以负值镜像重复定义 [SC] 同名码
 * （如 AIRY_EIPC_MAGIC=-41 对应 [SC] AIRY_EIPC_MAGIC=41），属双权威漂移源
 * （同一编译单元 include 顺序不同将得到不同值），且全仓 0 调用，已删除。
 *
 * 命名规范：与 AirymaxOS [SC] error.h (kernel/include/airymax/error.h) 对齐，
 *     使用 AIRY_EIPC_* / AIRY_ECAP_* / AIRY_FAULT_* 前缀（无下划线分隔符）。
 * 权威定义：commons/include/airymax/error.h（正幅值 10 子空间 + Fault 码空间，
 *     返回 -AIRY_E*），经 airy_types.h → types.h 引入。
 *
 * 码空间分配（[SC]）：
 *   IPC 码空间 [41, 70]         — IPC 协议层错误（fastpath C-S0~C-S11）
 *   Capability 码空间 [71, 100] — Capability Folding Badge 校验错误（C-S9）
 *   Fault 码空间 [0x1000, 0x1FFF] — 非可恢复故障（触发 USV Fault Handler）
 *
 * H3 约束：agentrt 用户态 capability_badge 始终为 0，理论上不会触发
 *     AIRY_ECAP_* 错误（这些错误由 agent-linux 内核态 fastpath 抛出）。
 *     用户态如需引用，用 -AIRY_ECAP_* 等（[SC] 正幅值取负）。
 * ============================================================================ */

#endif /* AIRY_RT_UTILS_ERROR_CODES_H */
