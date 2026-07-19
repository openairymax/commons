/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file error.h
 * @brief 统一错误处理框架
 *
 * 设计原则：
 * 1. 所有错误码为负值，成功为0
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

/* ==================== 错误码类型 ==================== */

/* airy_err_t 已在 airy_types.h 中定义（BAN-196 权威源），此处不再重复定义 */

/* ==================== 成功/失败基础 ==================== */

/*
 * AIRY_OK 兼容宏（v4.0 修复 v3.0 副作用）：
 * v3.0 曾移除 AIRY_OK 定义以统一使用 AIRY_EOK，但 39 文件/241 处仍引用
 * AIRY_OK，导致全仓库编译破坏。v4.0 恢复 AIRY_OK 作为兼容宏，与
 * AIRY_EOK/AIRY_SUCCESS 等价（均 = 0）。
 * 新代码推荐使用 AIRY_EOK；AIRY_OK 保留供存量代码兼容，M1 阶段渐进迁移。
 */
#ifndef AIRY_OK
#define AIRY_OK 0
#endif
#ifndef AIRY_ERR_UNKNOWN
#define AIRY_ERR_UNKNOWN (-99)
#endif

/* ==================== 向后兼容别名 ==================== */
/*
 * 权威源说明（v3.0 SSoT 统一收敛，方案 A：POSIX errno 负值）：
 *
 * POSIX 风格错误码（AIRY_EINVAL / AIRY_ENOMEM / AIRY_EBUSY 等）
 * 的权威定义位于 airy_types.h（硬定义，无 #ifndef 保护），使用 POSIX errno
 * 负值（如 AIRY_EINVAL=-22、AIRY_ENOMEM=-12、AIRY_ETIMEDOUT=-110）。
 *
 * 本文件原先用 #ifndef 为上述 POSIX 码提供"别名到 AIRY_ERR_* 扩展码"
 * 的兼容层，但因 types.h 总是先被 include（本文件 line 27），#ifndef 恒为
 * false，这些别名均为死代码。已于 G2.1 清理。
 *
 * v3.0 SSoT 统一收敛变更：
 *   - 移除 #undef AIRY_EPERM 重定义，让 airy_types.h 的 AIRY_EPERM=(-1)（POSIX EPERM）生效
 *   - 移除 AIRY_OK 定义，统一使用 AIRY_EOK（=0）
 *   - 将与 POSIX errno 负值冲突的 AIRY_ERR_* 扩展码迁移至 -40~-48 区间
 *     （AIRY_ERR_SYS_THREAD/SYS_CONDITION/SYS_PIPE/SYS_PROCESS 迁移至 -120~-123，
 *      避免与 ECONNRESET=-104/ENOTCONN=-107/ETIMEDOUT=-110/ECONNREFUSED=-111 冲突）
 *   - AIRY_ERR_UNKNOWN 从 -1 迁移至 -99（避免与 AIRY_EPERM=-1 冲突）
 *
 * 下方仅保留两类活跃定义：
 *   1. types.h 未定义的扩展别名（EBADF/ERESOURCE/ESECURITY/ESANITIZE/
 *      ECANCELED/ENOTDIR/ENAMETOOLONG）——本文件为唯一源
 *      （AIRY_EINTR/AIRY_EFAULT 已于 v4.0 纳入 airy_types.h SSoT 硬定义）
 *   2. ENOTDIR/ENAMETOOLONG 等 types.h 未定义的 POSIX 别名
 *
 * 已知技术债（计划 1.0.1 M1 消除）：AIRY_ERR_* 扩展码与 AIRY_E* POSIX 码
 * 在数值区间仍有部分语义重叠（如 AIRY_ERR_PROTOCOL=-900 与 AIRY_EPROTO=-71）。
 * 调用方应始终使用语义宏，严禁与字面量直接比较。
 */
#ifndef AIRY_ECANCELED
#define AIRY_ECANCELED AIRY_ERR_CANCELED
#endif
/* AIRY_EINTR 已在 airy_types.h 中硬定义为 (-4)（POSIX EINTR=4），本别名已成死代码，保留仅为向后兼容 */
#ifndef AIRY_EINTR
#define AIRY_EINTR AIRY_ERR_INTERRUPTED
#endif
#ifndef AIRY_EBADF
#define AIRY_EBADF AIRY_ERR_SYS_FILE
#endif
#ifndef AIRY_ERESOURCE
#define AIRY_ERESOURCE AIRY_ERR_SYS_RESOURCE
#endif
#ifndef AIRY_ESECURITY
#define AIRY_ESECURITY AIRY_ERR_ESECURITY
#endif
#ifndef AIRY_ESANITIZE
#define AIRY_ESANITIZE AIRY_ERR_ESANITIZE
#endif
/* AIRY_EPERM 不再重定义：airy_types.h 的 AIRY_EPERM=(-1)（POSIX EPERM）为权威值 */
#ifndef AIRY_ENOTDIR
#define AIRY_ENOTDIR (-20)          /* POSIX ENOTDIR=20 */
#endif
#ifndef AIRY_ENAMETOOLONG
#define AIRY_ENAMETOOLONG (-36)     /* POSIX ENAMETOOLONG=36 */
#endif

/* ==================== 错误码分段 ==================== */
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
 * AIRY_EINTR/AIRY_EFAULT）。未冲突的保留原值。 */
#ifndef AIRY_ERR_INVALID_PARAM
#define AIRY_ERR_INVALID_PARAM (-40)   /* 原 -2，迁移避免与 AIRY_ENOENT(-2) 冲突 */
#endif
#ifndef AIRY_ERR_NULL_POINTER
#define AIRY_ERR_NULL_POINTER (-3)
#endif
#ifndef AIRY_ERR_OUT_OF_MEMORY
#define AIRY_ERR_OUT_OF_MEMORY (-49)   /* 原 -4，迁移避免与 AIRY_EINTR(-4) 冲突 */
#endif
#ifndef AIRY_ERR_BUFFER_TOO_SMALL
#define AIRY_ERR_BUFFER_TOO_SMALL (-41) /* 原 -5，迁移避免与 AIRY_EIO(-5) 冲突 */
#endif
#ifndef AIRY_ERR_NOT_FOUND
#define AIRY_ERR_NOT_FOUND (-6)
#endif
#ifndef AIRY_ERR_ALREADY_EXISTS
#define AIRY_ERR_ALREADY_EXISTS (-42)  /* 原 -7，迁移避免与 AIRY_E2BIG(-7) 冲突 */
#endif
#ifndef AIRY_ERR_TIMEOUT
#define AIRY_ERR_TIMEOUT (-8)
#endif
#ifndef AIRY_ERR_NOT_SUPPORTED
#define AIRY_ERR_NOT_SUPPORTED (-9)
#endif
#ifndef AIRY_ERR_PERMISSION_DENIED
#define AIRY_ERR_PERMISSION_DENIED (-43) /* 原 -10，迁移避免与 AIRY_ECANCELLED(-10) 冲突 */
#endif
#ifndef AIRY_ERR_IO
#define AIRY_ERR_IO (-44)            /* 原 -11，迁移避免与 AIRY_EAGAIN(-11) 冲突 */
#endif
#ifndef AIRY_ERR_PARSE_ERROR
#define AIRY_ERR_PARSE_ERROR (-45)   /* 原 -12，迁移避免与 AIRY_ENOMEM(-12) 冲突 */
#endif
#ifndef AIRY_ERR_STATE_ERROR
#define AIRY_ERR_STATE_ERROR (-46)   /* 原 -13，迁移避免与 AIRY_EACCES(-13) 冲突 */
#endif
#ifndef AIRY_ERR_OVERFLOW
#define AIRY_ERR_OVERFLOW (-50)        /* 原 -14，迁移避免与 AIRY_EFAULT(-14) 冲突 */
#endif
#ifndef AIRY_ERR_UNDERFLOW
#define AIRY_ERR_UNDERFLOW (-15)
#endif
#ifndef AIRY_ERR_CANCELED
#define AIRY_ERR_CANCELED (-47)      /* 原 -16，迁移避免与 AIRY_EBUSY(-16) 冲突 */
#endif
#ifndef AIRY_ERR_BUSY
#define AIRY_ERR_BUSY (-48)          /* 原 -17，迁移避免与 AIRY_EEXIST(-17) 冲突 */
#endif
#ifndef AIRY_ERR_WOULD_BLOCK
#define AIRY_ERR_WOULD_BLOCK (-18)
#endif
#ifndef AIRY_ERR_INTERRUPTED
#define AIRY_ERR_INTERRUPTED (-19)
#endif

#ifndef AIRY_ERR_NOT_IMPLEMENTED
#define AIRY_ERR_NOT_IMPLEMENTED (-30)
#endif
#ifndef AIRY_ERR_FAIL
#define AIRY_ERR_FAIL (-31)
#endif

/* 系统与平台错误 (-100 到 -199)
 *
 * v3.0 SSoT 统一收敛：AIRY_ERR_SYS_THREAD/CONDITION/PIPE/PROCESS 原值
 * -104/-107/-110/-111 与 airy_types.h POSIX 码（ECONNRESET/ENOTCONN/
 * ETIMEDOUT/ECONNREFUSED）冲突，迁移至 -120~-123。 */
#ifndef AIRY_ERR_SYS_BASE
#define AIRY_ERR_SYS_BASE (-100)
#endif
#ifndef AIRY_ERR_SYS_NOT_INIT
#define AIRY_ERR_SYS_NOT_INIT (-101)
#endif
#ifndef AIRY_ERR_SYS_RESOURCE
#define AIRY_ERR_SYS_RESOURCE (-102)
#endif
#ifndef AIRY_ERR_SYS_DEADLOCK
#define AIRY_ERR_SYS_DEADLOCK (-103)
#endif
#ifndef AIRY_ERR_SYS_THREAD
#define AIRY_ERR_SYS_THREAD (-120)    /* 原 -104，迁移避免与 AIRY_ECONNRESET(-104) 冲突 */
#endif
#ifndef AIRY_ERR_SYS_MUTEX
#define AIRY_ERR_SYS_MUTEX (-105)
#endif
#ifndef AIRY_ERR_SYS_SEMAPHORE
#define AIRY_ERR_SYS_SEMAPHORE (-106)
#endif
#ifndef AIRY_ERR_SYS_CONDITION
#define AIRY_ERR_SYS_CONDITION (-121) /* 原 -107，迁移避免与 AIRY_ENOTCONN(-107) 冲突 */
#endif
#ifndef AIRY_ERR_SYS_ATOMIC
#define AIRY_ERR_SYS_ATOMIC (-108)
#endif
#ifndef AIRY_ERR_SYS_SOCKET
#define AIRY_ERR_SYS_SOCKET (-109)
#endif
#ifndef AIRY_ERR_SYS_PIPE
#define AIRY_ERR_SYS_PIPE (-122)      /* 原 -110，迁移避免与 AIRY_ETIMEDOUT(-110) 冲突 */
#endif
#ifndef AIRY_ERR_SYS_PROCESS
#define AIRY_ERR_SYS_PROCESS (-123)   /* 原 -111，迁移避免与 AIRY_ECONNREFUSED(-111) 冲突 */
#endif
#ifndef AIRY_ERR_SYS_FILE
#define AIRY_ERR_SYS_FILE (-112)
#endif
#ifndef AIRY_ERR_SYS_TIME
#define AIRY_ERR_SYS_TIME (-113)
#endif

/* 内核层错误 (-200 到 -299) */
#ifndef AIRY_ERR_KERN_BASE
#define AIRY_ERR_KERN_BASE (-200)
#endif
#ifndef AIRY_ERR_KERN_IPC
#define AIRY_ERR_KERN_IPC (-201)
#endif
#ifndef AIRY_ERR_KERN_TASK
#define AIRY_ERR_KERN_TASK (-202)
#endif
#ifndef AIRY_ERR_KERN_SYNC
#define AIRY_ERR_KERN_SYNC (-203)
#endif
#ifndef AIRY_ERR_KERN_LOCK
#define AIRY_ERR_KERN_LOCK (-204)
#endif
#ifndef AIRY_ERR_KERN_MEM
#define AIRY_ERR_KERN_MEM (-205)
#endif
#ifndef AIRY_ERR_KERN_SCHED
#define AIRY_ERR_KERN_SCHED (-206)
#endif
#ifndef AIRY_ERR_KERN_TIMER
#define AIRY_ERR_KERN_TIMER (-207)
#endif
#ifndef AIRY_ERR_KERN_INTERRUPT
#define AIRY_ERR_KERN_INTERRUPT (-208)
#endif

/* 服务层错误 (-300 到 -399) */
#ifndef AIRY_ERR_SVC_BASE
#define AIRY_ERR_SVC_BASE (-300)
#endif
#ifndef AIRY_ERR_SVC_NOT_READY
#define AIRY_ERR_SVC_NOT_READY (-301)
#endif
#ifndef AIRY_ERR_SVC_BUSY
#define AIRY_ERR_SVC_BUSY (-302)
#endif
#ifndef AIRY_ERR_SVC_STOPPED
#define AIRY_ERR_SVC_STOPPED (-303)
#endif
#ifndef AIRY_ERR_SVC_CONFIG
#define AIRY_ERR_SVC_CONFIG (-304)
#endif
#ifndef AIRY_ERR_SVC_DEPENDENCY
#define AIRY_ERR_SVC_DEPENDENCY (-305)
#endif
#ifndef AIRY_ERR_SVC_HEALTH
#define AIRY_ERR_SVC_HEALTH (-306)
#endif
#ifndef AIRY_ERR_SVC_LOADBALANCE
#define AIRY_ERR_SVC_LOADBALANCE (-307)
#endif

/* LLM/AI服务错误 (-400 到 -499) */
#ifndef AIRY_ERR_LLM_BASE
#define AIRY_ERR_LLM_BASE (-400)
#endif
#ifndef AIRY_ERR_LLM_NO_PROVIDER
#define AIRY_ERR_LLM_NO_PROVIDER (-401)
#endif
#ifndef AIRY_ERR_LLM_PROVIDER_FAIL
#define AIRY_ERR_LLM_PROVIDER_FAIL (-402)
#endif
#ifndef AIRY_ERR_LLM_RATE_LIMIT
#define AIRY_ERR_LLM_RATE_LIMIT (-403)
#endif
#ifndef AIRY_ERR_LLM_CONTEXT_LEN
#define AIRY_ERR_LLM_CONTEXT_LEN (-404)
#endif
#ifndef AIRY_ERR_LLM_INVALID_MODEL
#define AIRY_ERR_LLM_INVALID_MODEL (-405)
#endif
#ifndef AIRY_ERR_LLM_AUTH_FAIL
#define AIRY_ERR_LLM_AUTH_FAIL (-406)
#endif
#ifndef AIRY_ERR_LLM_TOKEN_LIMIT
#define AIRY_ERR_LLM_TOKEN_LIMIT (-407)
#endif
#ifndef AIRY_ERR_LLM_PARSE_RESP
#define AIRY_ERR_LLM_PARSE_RESP (-408)
#endif
#ifndef AIRY_ERR_LLM_EMPTY_RESP
#define AIRY_ERR_LLM_EMPTY_RESP (-409)
#endif
#ifndef AIRY_ERR_LLM_COST_EXCEED
#define AIRY_ERR_LLM_COST_EXCEED (-410)
#endif

/* 执行/工具错误 (-500 到 -599) */
#ifndef AIRY_ERR_EXEC_BASE
#define AIRY_ERR_EXEC_BASE (-500)
#endif
#ifndef AIRY_ERR_EXEC_NOT_FOUND
#define AIRY_ERR_EXEC_NOT_FOUND (-501)
#endif
#ifndef AIRY_ERR_EXEC_FAIL
#define AIRY_ERR_EXEC_FAIL (-502)
#endif
#ifndef AIRY_ERR_EXEC_TIMEOUT
#define AIRY_ERR_EXEC_TIMEOUT (-503)
#endif
#ifndef AIRY_ERR_EXEC_VALIDATION
#define AIRY_ERR_EXEC_VALIDATION (-504)
#endif
#ifndef AIRY_ERR_EXEC_SANDBOX
#define AIRY_ERR_EXEC_SANDBOX (-505)
#endif
#ifndef AIRY_ERR_EXEC_PERMISSION
#define AIRY_ERR_EXEC_PERMISSION (-506)
#endif
#ifndef AIRY_ERR_EXEC_ARGS
#define AIRY_ERR_EXEC_ARGS (-507)
#endif
#ifndef AIRY_ERR_EXEC_ENV
#define AIRY_ERR_EXEC_ENV (-508)
#endif

/* 记忆/存储错误 (-600 到 -699) */
#ifndef AIRY_ERR_MEM_BASE
#define AIRY_ERR_MEM_BASE (-600)
#endif
#ifndef AIRY_ERR_MEM_WRITE
#define AIRY_ERR_MEM_WRITE (-601)
#endif
#ifndef AIRY_ERR_MEM_READ
#define AIRY_ERR_MEM_READ (-602)
#endif
#ifndef AIRY_ERR_MEM_QUERY
#define AIRY_ERR_MEM_QUERY (-603)
#endif
#ifndef AIRY_ERR_MEM_EVOLVE
#define AIRY_ERR_MEM_EVOLVE (-604)
#endif
#ifndef AIRY_ERR_MEM_FULL
#define AIRY_ERR_MEM_FULL (-605)
#endif
#ifndef AIRY_ERR_MEM_CORRUPT
#define AIRY_ERR_MEM_CORRUPT (-606)
#endif
#ifndef AIRY_ERR_MEM_NOT_INIT
#define AIRY_ERR_MEM_NOT_INIT (-607)
#endif

/* 安全/沙箱错误 (-700 到 -799) */
#ifndef AIRY_ERR_SEC_BASE
#define AIRY_ERR_SEC_BASE (-700)
#endif
#ifndef AIRY_ERR_SEC_VIOLATION
#define AIRY_ERR_SEC_VIOLATION (-701)
#endif
#ifndef AIRY_ERR_SEC_SANITIZE
#define AIRY_ERR_SEC_SANITIZE (-702)
#endif
#ifndef AIRY_ERR_SEC_AUDIT
#define AIRY_ERR_SEC_AUDIT (-703)
#endif
#ifndef AIRY_ERR_SEC_PERMISSION
#define AIRY_ERR_SEC_PERMISSION (-704)
#endif
#ifndef AIRY_ERR_SEC_VALIDATION
#define AIRY_ERR_SEC_VALIDATION (-705)
#endif
#ifndef AIRY_ERR_SEC_QUOTA
#define AIRY_ERR_SEC_QUOTA (-706)
#endif
#ifndef AIRY_ERR_SEC_TEMP_DIR
#define AIRY_ERR_SEC_TEMP_DIR (-707)
#endif
#ifndef AIRY_ERR_SEC_SYMLINK
#define AIRY_ERR_SEC_SYMLINK (-708)
#endif
#ifndef AIRY_ERR_SEC_PATH_TRAV
#define AIRY_ERR_SEC_PATH_TRAV (-709)
#endif
#ifndef AIRY_ERR_ESECURITY
#define AIRY_ERR_ESECURITY (-710)
#endif
#ifndef AIRY_ERR_ESANITIZE
#define AIRY_ERR_ESANITIZE (-711)
#endif

/* Cupolas 安全穹顶专属错误码 (-712 到 -799)
 *
 * P0.25.4 (ACC-STD06)：任务清单原要求 -700~-705 段，但 -700~-711 已被
 * AIRY_ERR_SEC_* 占用（v3.4 之前已定义）。为避免数值冲突，Cupolas 专属
 * 错误码段调整为 -712~-799。Cupolas 公共 API 仍可通过 cupolas_ERR_* enum
 * （数值与 AIRY_ERR_* 通用码一致，如 cupolas_ERR_OUT_OF_MEMORY=-49）
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
#ifndef AIRY_ERR_CUPOLAS_BASE
#define AIRY_ERR_CUPOLAS_BASE (-712)
#endif
#ifndef AIRY_ERR_CUPOLAS_DENIED
#define AIRY_ERR_CUPOLAS_DENIED (-713)
#endif
#ifndef AIRY_ERR_CUPOLAS_QUARANTINE
#define AIRY_ERR_CUPOLAS_QUARANTINE (-714)
#endif
#ifndef AIRY_ERR_CUPOLAS_POLICY
#define AIRY_ERR_CUPOLAS_POLICY (-715)
#endif
#ifndef AIRY_ERR_CUPOLAS_SANDBOX
#define AIRY_ERR_CUPOLAS_SANDBOX (-716)
#endif
#ifndef AIRY_ERR_CUPOLAS_AUDIT
#define AIRY_ERR_CUPOLAS_AUDIT (-717)
#endif
#ifndef AIRY_ERR_CUPOLAS_TAMPERED
#define AIRY_ERR_CUPOLAS_TAMPERED (-718)
#endif
#ifndef AIRY_ERR_CUPOLAS_SIGNATURE
#define AIRY_ERR_CUPOLAS_SIGNATURE (-719)
#endif
#ifndef AIRY_ERR_CUPOLAS_VAULT
#define AIRY_ERR_CUPOLAS_VAULT (-720)
#endif
#ifndef AIRY_ERR_CUPOLAS_ENTITLEMENT
#define AIRY_ERR_CUPOLAS_ENTITLEMENT (-721)
#endif
#ifndef AIRY_ERR_CUPOLAS_RUNTIME
#define AIRY_ERR_CUPOLAS_RUNTIME (-722)
#endif
#ifndef AIRY_ERR_CUPOLAS_NETWORK
#define AIRY_ERR_CUPOLAS_NETWORK (-723)
#endif
#ifndef AIRY_ERR_CUPOLAS_AUTH_FAILED
#define AIRY_ERR_CUPOLAS_AUTH_FAILED (-724)
#endif
#ifndef AIRY_ERR_CUPOLAS_CERT_INVALID
#define AIRY_ERR_CUPOLAS_CERT_INVALID (-725)
#endif
#ifndef AIRY_ERR_CUPOLAS_CERT_EXPIRED
#define AIRY_ERR_CUPOLAS_CERT_EXPIRED (-726)
#endif

/* 协调/规划错误 (-800 到 -899) */
#ifndef AIRY_ERR_COORD_BASE
#define AIRY_ERR_COORD_BASE (-800)
#endif
#ifndef AIRY_ERR_COORD_PLAN_FAIL
#define AIRY_ERR_COORD_PLAN_FAIL (-801)
#endif
#ifndef AIRY_ERR_COORD_SYNC_FAIL
#define AIRY_ERR_COORD_SYNC_FAIL (-802)
#endif
#ifndef AIRY_ERR_COORD_DISPATCH
#define AIRY_ERR_COORD_DISPATCH (-803)
#endif
#ifndef AIRY_ERR_COORD_INTENT
#define AIRY_ERR_COORD_INTENT (-804)
#endif
#ifndef AIRY_ERR_COORD_COMPENSATE
#define AIRY_ERR_COORD_COMPENSATE (-805)
#endif
#ifndef AIRY_ERR_COORD_RETRY_EXCEED
#define AIRY_ERR_COORD_RETRY_EXCEED (-806)
#endif

/* 协议/校验错误 (-900 到 -909)
 *
 * P0.22.1 (ARE L2)：IPC Bus 统一消息头校验失败的专属错误码段。
 * - AIRY_ERR_PROTOCOL  magic/version/reserved 字段不匹配（消息必须丢弃）
 * - AIRY_ERR_CHECKSUM  CRC32 校验和不匹配（消息必须丢弃，不得回复 ERROR）
 * 详见 Docs/Capital_Specifications/are_standards/L2_service_protocol.md §2.3
 */
#ifndef AIRY_ERR_PROTOCOL
#define AIRY_ERR_PROTOCOL (-900)
#endif
#ifndef AIRY_ERR_CHECKSUM
#define AIRY_ERR_CHECKSUM (-901)
#endif

/* ============================================================================
 * Capability Folding v1.1 — IPC / Capability / Fault 错误码空间
 *
 * SSoT: docs-closed/agentrt-linux/00-reviews/_review_v2.2/37-capability-folding-
 *       decision-and-roadmap.md §6.5
 * 命名规范：与 AirymaxOS [SC] error.h (kernel/include/airymax/error.h) 对齐，
 *     使用 AIRY_EIPC_* / AIRY_ECAP_* / AIRY_FAULT_* 前缀（无下划线分隔符）。
 *
 * 码空间分配：
 *   IPC 码空间 [-41, -70]    — IPC 协议层错误（fastpath C-S0~C-S11）
 *   Capability 码空间 [-71, -100] — Capability Folding Badge 校验错误（C-S9）
 *   Fault 码空间 [0x1000, 0x1FFF] — 非可恢复故障（触发 USV Fault Handler）
 *
 * 注意：agentrt 现有 AIRY_ERR_* 扩展码（-40 ~ -50）与 IPC 码空间 [-41, -70]
 * 在数值上有部分重叠，但命名前缀不同（AIRY_ERR_* vs AIRY_EIPC_*），故无宏重定义
 * 冲突。调用方应使用语义宏（AIRY_EIPC_*），严禁与字面量直接比较。
 *
 * H3 约束：agentrt 用户态 capability_badge 始终为 0，理论上不会触发
 *     AIRY_ECAP_* 错误（这些错误由 agentrt-linux 内核态 fastpath 抛出）。
 *     此处定义仅为 [SC] 契约对齐与单元测试断言使用。
 * ============================================================================ */

/* ---- IPC 协议错误码 (-41..-70, v1.1) ---- */
#ifndef AIRY_EIPC_MAGIC
#define AIRY_EIPC_MAGIC        (-41)   /* C-S1: magic 不匹配 (期望 0x41524531 'ARE1') */
#endif
#ifndef AIRY_EIPC_OPCODE
#define AIRY_EIPC_OPCODE       (-42)   /* C-S2: opcode 非法或未注册 */
#endif
#ifndef AIRY_EIPC_PAYLOAD
#define AIRY_EIPC_PAYLOAD      (-43)   /* C-S3: payload_len 超过 registered buffer */
#endif
#ifndef AIRY_EIPC_HDRSIZE
#define AIRY_EIPC_HDRSIZE      (-44)   /* C-S0: header 总长不等于 128 字节 */
#endif
#ifndef AIRY_EIPC_RESERVED
#define AIRY_EIPC_RESERVED     (-45)   /* C-S4: reserved 字段非零 */
#endif
#ifndef AIRY_EIPC_FLAGS
#define AIRY_EIPC_FLAGS        (-46)   /* C-S10: flags 非法组合 */
#endif
#ifndef AIRY_EIPC_NOTSUPP
#define AIRY_EIPC_NOTSUPP      (-47)   /* C-S11: opcode 不支持 */
#endif
#ifndef AIRY_EIPC_KFIFO
#define AIRY_EIPC_KFIFO        (-48)   /* kfifo 投递失败 */
#endif
#ifndef AIRY_EIPC_RECLAIM
#define AIRY_EIPC_RECLAIM      (-49)   /* registered buffer 回收失败 */
#endif
#ifndef AIRY_EIPC_CONTEXT
#define AIRY_EIPC_CONTEXT      (-50)   /* IPC 上下文非法 */
#endif
#ifndef AIRY_EIPC_CRC32
#define AIRY_EIPC_CRC32        (-51)   /* C-S12: CRC32 校验失败（覆盖 header[0:52)+payload） */
#endif
#ifndef AIRY_EIPC_TIMEOUT
#define AIRY_EIPC_TIMEOUT      (-52)   /* IPC 操作超时 */
#endif

/* ---- Capability / Badge 错误码 (-71..-100, v1.1) ----
 *
 * H3 约束：agentrt 用户态不感知 Badge，capability_badge 始终为 0。
 * 这些错误码主要由 agentrt-linux 内核态 fastpath C-S9 抛出。
 * agentrt 侧定义仅为 [SC] 契约对齐与跨端错误码翻译使用。
 */
#ifndef AIRY_ECAP_MISSING
#define AIRY_ECAP_MISSING      (-71)   /* capability 不存在 */
#endif
#ifndef AIRY_ECAP_REVOKED
#define AIRY_ECAP_REVOKED      (-72)   /* capability 已被撤销（atomic_inc 触发） */
#endif
#ifndef AIRY_ECAP_EXPIRED
#define AIRY_ECAP_EXPIRED      (-73)   /* capability lease 已过期 */
#endif
#ifndef AIRY_ECAP_MISMATCH
#define AIRY_ECAP_MISMATCH     (-74)   /* capability 派生链不匹配 */
#endif
#ifndef AIRY_ECAP_LSM_DENIED
#define AIRY_ECAP_LSM_DENIED   (-75)   /* LSM 钩子拒绝操作 */
#endif
#ifndef AIRY_ECAP_RADIX_MISS
#define AIRY_ECAP_RADIX_MISS   (-76)   /* radix tree 查找失败（遗留，v1.1 改用静态数组） */
#endif
#ifndef AIRY_ECAP_STATIC_KEY
#define AIRY_ECAP_STATIC_KEY   (-77)   /* 静态密钥校验失败 */
#endif
#ifndef AIRY_ECAP_BADGE
#define AIRY_ECAP_BADGE        (-78)   /* Badge 字段非法 */
#endif
#ifndef AIRY_ECAP_EPOCH
#define AIRY_ECAP_EPOCH        (-79)   /* C-S9: Badge Epoch 与全局 Epoch 不匹配 */
#endif
#ifndef AIRY_ECAP_FORGED
#define AIRY_ECAP_FORGED       (-80)   /* C-S9: Badge Random Tag 不匹配（防伪造）→ 触发 Fault */
#endif
#ifndef AIRY_ECAP_PERM
#define AIRY_ECAP_PERM         (-81)   /* C-S9: Badge Perms 权限位不满足 */
#endif
#ifndef AIRY_ECAP_FROZEN
#define AIRY_ECAP_FROZEN       (-82)   /* capability 已冻结（FREEZE opcode） */
#endif

/* ---- Fault 故障码 (0x1000+, v1.1, 非可恢复) ----
 *
 * Fault 码不是函数返回值，而是通过 fault 通知通道（eventfd / die_notifier）
 * 传递给 USV Fault Handler。agentrt 用户态无 fault 机制，这些定义仅为
 * [SC] 契约对齐与文档引用使用。
 */
#ifndef AIRY_FAULT_CAP_FORGED
#define AIRY_FAULT_CAP_FORGED     (0x1001u)   /* Badge 伪造检测（C-S9 RandomTag 不匹配） */
#endif
#ifndef AIRY_FAULT_CAP_LEAK
#define AIRY_FAULT_CAP_LEAK       (0x1002u)   /* capability 泄漏检测 */
#endif
#ifndef AIRY_FAULT_RING_CORRUPT
#define AIRY_FAULT_RING_CORRUPT   (0x1003u)   /* IPC ring buffer 损坏 */
#endif

/* ==================== 错误上下文 ==================== */

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

/* ==================== 错误处理接口 ==================== */

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

/* ==================== 便捷宏 ==================== */

/**
 * @brief 设置错误并返回（自动使用错误码字符串）
 *
 * 统一替代各模块自定义的 *_RET_ERR 宏（ATM_RET_ERR / CUP_RET_ERR / RQ_RET_ERR 等）。
 * 等价于 AIRY_ERROR(code, airy_err_str(code))。
 */
#define AIRY_RET_ERR(code)                                                     \
    do {                                                                          \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, "%s",         \
                         airy_err_str(code));                                 \
        return (code);                                                            \
    } while (0)

/**
 * @brief 设置错误并返回
 */
#define AIRY_ERROR(code, msg)                                                  \
    do {                                                                          \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return (code);                                                            \
    } while (0)

/**
 * @brief 设置格式化错误并返回
 */
#define AIRY_ERROR_FMT(code, fmt, ...)                                                \
    do {                                                                                 \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, (fmt), __VA_ARGS__); \
        return (code);                                                                   \
    } while (0)

/**
 * @brief 设置错误并返回 NULL（用于返回指针的函数）
 *
 * 与 AIRY_ERROR 的区别：返回 NULL 而非错误码，适用于函数返回类型为指针的场景。
 * 错误码通过 error stack 传递，调用者可通过 airy_err_last() 获取。
 */
#define AIRY_ERROR_NULL(code, msg)                                             \
    do {                                                                          \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return NULL;                                                              \
    } while (0)

/**
 * @brief 条件检查，失败时返回错误
 */
#define AIRY_CHECK(cond, code, msg)    \
    do {                                  \
        if (!(cond)) {                    \
            AIRY_ERROR((code), (msg)); \
        }                                 \
    } while (0)

/**
 * @brief 空指针检查
 */
#define AIRY_CHECK_NULL(ptr, name) \
    AIRY_CHECK((ptr) != NULL, AIRY_ERR_NULL_POINTER, name " is NULL")

/**
 * @brief 内存分配检查
 */
#define AIRY_CHECK_ALLOC(ptr) \
    AIRY_CHECK((ptr) != NULL, AIRY_ERR_OUT_OF_MEMORY, "Memory allocation failed")

/**
 * @brief 错误传播宏
 */
#define AIRY_PROPAGATE(expr)                                                              \
    do {                                                                                     \
        airy_err_t __err = (expr);                                                      \
        if (__err != AIRY_EOK) {                                                           \
            airy_err_push_ex(__err, __FILE__, __LINE__, __func__, "Propagated from %s", \
                                  #expr);                                                    \
            return __err;                                                                    \
        }                                                                                    \
    } while (0)

/**
 * @brief 错误检查宏（返回错误码而非直接返回）
 */
#define AIRY_TRY(expr)               \
    do {                                \
        airy_err_t __err = (expr); \
        if (__err != AIRY_EOK) {      \
            return __err;               \
        }                               \
    } while (0)

/* ==================== 向后兼容接口（已废弃） ==================== */

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
typedef void (*airy_err_handler_t)(airy_err_t err,
                                        const airy_err_context_t *context);

/**
 * @brief 设置错误处理回调（兼容旧代码）
 * @deprecated
 */
void airy_err_set_handler(airy_err_handler_t handler);

/**
 * @brief 兼容旧代码的错误处理宏
 * @deprecated 请使用 AIRY_ERROR
 */
#define AIRY_ERROR_HANDLE(code, msg)                                           \
    do {                                                                          \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
    } while (0)

#define AIRY_ERROR_PUSH_EX(code, msg) AIRY_ERROR_HANDLE(code, msg)

/**
 * @brief 兼容旧代码的错误处理宏（带上下文）
 * @deprecated
 */
#define AIRY_ERROR_HANDLE_CONTEXT(code, user_data, msg)                        \
    do {                                                                          \
        airy_err_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        (void)(user_data);                                                        \
    } while (0)

/* ==================== 错误统计 ==================== */

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

/* ==================== 多语言支持 ==================== */

/**
 * @brief 支持的语言
 */
typedef enum {
    AIRY_LANG_EN_US = 0, /**< 英语（美国） */
    AIRY_LANG_ZH_CN = 1, /**< 简体中文 */
    AIRY_LANG_ZH_TW = 2, /**< 繁体中文 */
    AIRY_LANG_JA_JP = 3, /**< 日语 */
    AIRY_LANG_KO_KR = 4, /**< 韩语 */
    AIRY_LANG_DE_DE = 5, /**< 德语 */
    AIRY_LANG_FR_FR = 6, /**< 法语 */
    AIRY_LANG_ES_ES = 7  /**< 西班牙语 */
} airy_language_t;

/**
 * @brief 多语言错误描述结构
 */
typedef struct {
    airy_err_t error_code;  /**< 错误码 */
    const char *descriptions[8]; /**< 各语言描述（按airy_language_t顺序） */
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
airy_err_t airy_err_register_i18n(const airy_err_i18n_entry_t *entries,
                                            size_t count);

/**
 * @brief 获取错误链的本地化JSON表示
 *
 * @param[in] chain 错误链
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return JSON字符串（需调用者释放）
 */
char *airy_err_chain_to_json_i18n(const airy_err_chain_t *chain, airy_language_t lang);

/* ==================== 错误链增强功能 ==================== */

/**
 * @brief 错误链迭代器
 */
typedef struct {
    const airy_err_chain_t *chain; /**< 错误链 */
    size_t current_index;               /**< 当前索引 */
} airy_err_chain_iterator_t;

/**
 * @brief 初始化错误链迭代器
 *
 * @param[in] chain 错误链
 * @param[out] iter 迭代器
 */
void airy_err_chain_iter_init(const airy_err_chain_t *chain,
                                   airy_err_chain_iterator_t *iter);

/**
 * @brief 获取下一个错误上下文条目
 *
 * @param[inout] iter 迭代器
 * @return 下一个条目指针，如果没有更多条目返回NULL
 */
const airy_err_context_entry_t *
airy_err_chain_iter_next(airy_err_chain_iterator_t *iter);

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
