/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file error_codes.h
 * @brief AgentRT error code definitions.
 */

#ifndef AIRY_RT_UTILS_ERROR_CODES_H
#define AIRY_RT_UTILS_ERROR_CODES_H

#include "../../types/include/types.h"


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
 *   - 将与 POSIX errno 负值冲突的 AIRY_ERR_* 扩展码迁移至 -36~-40 和 -55~-60 区间
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

#ifndef AIRY_ENOTDIR
#define AIRY_ENOTDIR (-20) /* POSIX ENOTDIR=20 */
#endif
#ifndef AIRY_ENAMETOOLONG
#define AIRY_ENAMETOOLONG (-36) /* POSIX ENAMETOOLONG=36 */
#endif


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
#ifndef AIRY_ERR_INVALID_PARAM
#define AIRY_ERR_INVALID_PARAM (-36)
#endif
#ifndef AIRY_ERR_NULL_POINTER
#define AIRY_ERR_NULL_POINTER (-3)
#endif
#ifndef AIRY_ERR_OUT_OF_MEMORY
#define AIRY_ERR_OUT_OF_MEMORY (-59)
#endif
#ifndef AIRY_ERR_BUFFER_TOO_SMALL
#define AIRY_ERR_BUFFER_TOO_SMALL (-37)
#endif
#ifndef AIRY_ERR_NOT_FOUND
#define AIRY_ERR_NOT_FOUND (-6)
#endif
#ifndef AIRY_ERR_ALREADY_EXISTS
#define AIRY_ERR_ALREADY_EXISTS (-38)
#endif
#ifndef AIRY_ERR_TIMEOUT
#define AIRY_ERR_TIMEOUT (-8)
#endif
#ifndef AIRY_ERR_NOT_SUPPORTED
#define AIRY_ERR_NOT_SUPPORTED (-9)
#endif
#ifndef AIRY_ERR_PERMISSION_DENIED
#define AIRY_ERR_PERMISSION_DENIED (-39)
#endif
#ifndef AIRY_ERR_IO
#define AIRY_ERR_IO (-40)
#endif
#ifndef AIRY_ERR_PARSE_ERROR
#define AIRY_ERR_PARSE_ERROR (-55)
#endif
#ifndef AIRY_ERR_STATE_ERROR
#define AIRY_ERR_STATE_ERROR (-56)
#endif
#ifndef AIRY_ERR_OVERFLOW
#define AIRY_ERR_OVERFLOW (-60)
#endif
#ifndef AIRY_ERR_UNDERFLOW
#define AIRY_ERR_UNDERFLOW (-15)
#endif
#ifndef AIRY_ERR_CANCELED
#define AIRY_ERR_CANCELED (-57)
#endif
#ifndef AIRY_ERR_BUSY
#define AIRY_ERR_BUSY (-58)
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
/* S-1 收敛 (2026-08-14)：AIRY_ERR_FAIL 错误码改名 AIRY_ERR_GENERIC_FAIL——
 * [SC] airymax/error.h 定义了同名函数式辅助宏 AIRY_ERR_FAIL(err)（失败检测，
 * 展开为 ((err) < 0)），错误码若同名则 include [SC] 后 #ifndef 恒不触发，
 * 错误码丢失且调用点会误触发函数式宏。改名消除命名冲突（A-UEF 对齐）。 */
#ifndef AIRY_ERR_GENERIC_FAIL
#define AIRY_ERR_GENERIC_FAIL (-31)
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
#define AIRY_ERR_SYS_THREAD (-120)
#endif
#ifndef AIRY_ERR_SYS_MUTEX
#define AIRY_ERR_SYS_MUTEX (-105)
#endif
#ifndef AIRY_ERR_SYS_SEMAPHORE
#define AIRY_ERR_SYS_SEMAPHORE (-106)
#endif
#ifndef AIRY_ERR_SYS_CONDITION
#define AIRY_ERR_SYS_CONDITION (-121)
#endif
#ifndef AIRY_ERR_SYS_ATOMIC
#define AIRY_ERR_SYS_ATOMIC (-108)
#endif
#ifndef AIRY_ERR_SYS_SOCKET
#define AIRY_ERR_SYS_SOCKET (-109)
#endif
#ifndef AIRY_ERR_SYS_PIPE
#define AIRY_ERR_SYS_PIPE (-122)
#endif
#ifndef AIRY_ERR_SYS_PROCESS
#define AIRY_ERR_SYS_PROCESS (-123)
#endif
#ifndef AIRY_ERR_SYS_FILE
#define AIRY_ERR_SYS_FILE (-112)
#endif
#ifndef AIRY_ERR_SYS_TIME
#define AIRY_ERR_SYS_TIME (-113)
#endif


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
#ifndef AIRY_ERR_SVC_CYCLE
#define AIRY_ERR_SVC_CYCLE (-308)
#endif
#define AIRY_ERR_CYCLE_DETECTED AIRY_ERR_SVC_CYCLE


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
