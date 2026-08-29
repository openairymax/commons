/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * airy_types.h - authoritative source for unified type definitions
 *
 * Serves as the single source of truth for type definitions across the
 * project, resolving conflicts between modules. Follows the standardized
 * unification scheme to ensure cross-platform compile compatibility.
 *
 * Design principles:
 * 1. Authority: commons is the single authoritative base library
 * 2. Uniformity: the whole project uses unified type definitions and
 *    interface contracts
 * 3. Compatibility: ensure Windows, Linux and macOS compatibility
 *
 */

#ifndef AIRY_RT_UNIFIED_TYPES_H
#define AIRY_RT_UNIFIED_TYPES_H

/* [SC] 契约层自包含（0.1.6 P1-2 依赖图去环）：本头不再 include platform.h。
 * 下方注释引用的 airy_thread_t/airy_mtx_t 等平台类型由使用者自行包含
 * platform.h 获得；[SC] 契约头须能独立编译（含内核态）。 */

/* S-1 收敛 (2026-08-14, 用户决策): 错误码统一收敛至 A-UEF [SC] 唯一权威
 * commons/include/airymax/error.h。按 [SC] 契约自身声明（error.h 顶部注释）：
 *   - [SC] 共享契约头（agent-linux 内核态 UAPI）：错误码为正幅值，
 *     调用方返回 -AIRY_E*，供跨态契约/内核态编译单元使用；
 *   - agentrt 用户态通用错误码权威源为本文件（POSIX errno 负值，
 *     直接返回），遵循 Linux errno 哲学——两体系物理隔离，
 *     用户态编译单元恒见负值，[SC] 头文件本身不被修改。
 * 用户态 include [SC] 后立即 #undef 同名 POSIX 码并重定义为负值
 * （sc-dual-ci 逐字节校验的是 [SC] 头自身，不受本文件影响）。
 * AIRY_EIPC_*、AIRY_ECAP_*、AIRY_FAULT_* 等 [SC] 专有码保留正幅值
 * （跨态专用，用户态引用时取负返回）。 */
#include "airymax/error.h"

/* 用户态 POSIX errno 负值（重定义 [SC] 同名码；值域与 Linux errno 一致，
 * 即 S-1 收敛前的用户态权威值，全部存量调用点（return AIRY_E* 族）恢复正确） */
#undef AIRY_EPERM
#define AIRY_EPERM (-1)
#undef AIRY_ENOENT
#define AIRY_ENOENT (-2)
#undef AIRY_EINTR
#define AIRY_EINTR (-4)
#undef AIRY_EIO
#define AIRY_EIO (-5)
#undef AIRY_EAGAIN
#define AIRY_EAGAIN (-11)
#undef AIRY_ENOMEM
#define AIRY_ENOMEM (-12)
#undef AIRY_EACCES
#define AIRY_EACCES (-13)
#undef AIRY_EFAULT
#define AIRY_EFAULT (-14)
#undef AIRY_EBUSY
#define AIRY_EBUSY (-16)
#undef AIRY_EEXIST
#define AIRY_EEXIST (-17)
#undef AIRY_EINVAL
#define AIRY_EINVAL (-22)
#undef AIRY_ENOSPC
#define AIRY_ENOSPC (-28)
#undef AIRY_ERANGE
#define AIRY_ERANGE (-34)
#undef AIRY_ENOTSUP
#define AIRY_ENOTSUP (-95)
#undef AIRY_EISDIR
#define AIRY_EISDIR (-21) /* POSIX EISDIR=21（用户态新增负值，[SC] 原为正幅值 7） */
#undef AIRY_ECANCELED
#define AIRY_ECANCELED (-125) /* POSIX ECANCELED=125（用户态新增负值，[SC] 原为正幅值 19） */

#include <airymax/ipc.h> /* AIRY_IPC_MAGIC (0x41524531 'ARE1') */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Success return value
 * @note AIRY_EOK（[SC] airymax/error.h 提供，=0）与 AIRY_SUCCESS 等价。
 *       AIRY_SUCCESS 为用户态兼容宏（[SC] 无此命名），保留于此。
 */
#define AIRY_SUCCESS 0

/* ────────────────────────────────────────────────────────────────────────────
 * AIRY_E* 错误码（S-1 收敛后定稿，2026-08-14）：
 * 用户态 POSIX errno 负值码（含上方 #undef 重定义的 14 个 [SC] 同名码）
 * 统一在本文件定义，值域与 Linux errno 一致（AIRY_EINVAL=-22 等），
 * 全部存量调用点（return AIRY_E* 族 / 错误码比较）语义保持正确。
 * AIRY_ERR_* 用户态扩展码权威源为 commons/utils/error/include/error_codes.h。
 * [SC] 正幅值码（airymax/error.h）仅供跨态契约/内核态编译单元使用。
 * ──────────────────────────────────────────────────────────────────────────── */

/* [SC] 无同名的用户态专属码（POSIX errno 负值） */
#ifndef AIRY_E2BIG
#define AIRY_E2BIG (-7)
#endif
#ifndef AIRY_EDEADLK
#define AIRY_EDEADLK (-35)
#endif
#ifndef AIRY_ENOSYS
#define AIRY_ENOSYS (-38)
#endif
#ifndef AIRY_EPROTO
#define AIRY_EPROTO (-71)
#endif
#ifndef AIRY_EOVERFLOW
#define AIRY_EOVERFLOW (-75)
#endif
#ifndef AIRY_EMSGSIZE
#define AIRY_EMSGSIZE (-90)
#endif
#ifndef AIRY_EPROTONOSUPPORT
#define AIRY_EPROTONOSUPPORT (-93)
#endif
#ifndef AIRY_ECONNRESET
#define AIRY_ECONNRESET (-104)
#endif
#ifndef AIRY_ENOTCONN
#define AIRY_ENOTCONN (-107)
#endif
#ifndef AIRY_ETIMEDOUT
#define AIRY_ETIMEDOUT (-110)
#endif
#ifndef AIRY_ECONNREFUSED
#define AIRY_ECONNREFUSED (-111)
#endif
#ifndef AIRY_EALREADY
#define AIRY_EALREADY (-114)
#endif
#ifndef AIRY_ENOTFOUND
#define AIRY_ENOTFOUND (-18)
#endif

/* [SC] 无同名的用户态专属码（自定义负值） */
#ifndef AIRY_ENOTINIT
#define AIRY_ENOTINIT (-9)
#endif
#ifndef AIRY_ECANCELLED
#define AIRY_ECANCELLED (-10)
#endif
#ifndef AIRY_EUNAVAILABLE
#define AIRY_EUNAVAILABLE (-25)
#endif
#ifndef AIRY_EQUOTA
#define AIRY_EQUOTA (-26)
#endif
#ifndef AIRY_EPLATFORM
#define AIRY_EPLATFORM (-27)
#endif
#ifndef AIRY_ESERVICE
#define AIRY_ESERVICE (-29)
#endif
#ifndef AIRY_EFAIL
#define AIRY_EFAIL (-31)
#endif
#ifndef AIRY_EUNKNOWN
#define AIRY_EUNKNOWN (-99)
#endif

/* ================================================================
 * 品牌化 ID（阶段 3）：trace_id / msg_id 从不透明结构体（类型层不可互赋）
 *
 * 类型层隔离：airy_trace_id_t 与 airy_msg_id_t 是两个不透明结构体，
 * 编译器阻止相互赋值/混用（Branded<B> 语义，plan §2.1-5）。IPC 头
 * （Layout C v4）字段仍为 __u64（[SC] 128B 布局冻结），序列化时取
 * .value。结构化命名：
 *   trace_id 格式 "tr-<16 hex>"（W3C 风格 64 位，可读可解析）
 *   msg_id   格式 "msg-<ts:08x>-<seq:08x>"（高 32 位秒时间戳 +
 *             低 32 位进程内单调序列，可排序可读）
 * ================================================================ */

typedef struct airy_trace_id {
    uint64_t value;
} airy_trace_id_t;

typedef struct airy_msg_id {
    uint64_t value;
} airy_msg_id_t;

#define AIRY_TRACE_ID_NULL ((airy_trace_id_t){0})
#define AIRY_MSG_ID_NULL ((airy_msg_id_t){0})
#define AIRY_TRACE_ID_STR_MAX 24 /* "tr-" + 16 hex + NUL */
#define AIRY_MSG_ID_STR_MAX 24   /* "msg-" + 8 + '-' + 8 + NUL */

/**
 * @brief 生成全局唯一 trace_id（64 位熵：时间 + ASLR 地址混合）。
 * @threadsafe yes
 */
airy_trace_id_t airy_trace_id_generate(void);

/**
 * @brief 生成结构化 msg_id（高 32 位秒时间戳 | 低 32 位单调序列）。
 * @threadsafe yes
 */
airy_msg_id_t airy_msg_id_generate(void);

int airy_trace_id_eq(airy_trace_id_t a, airy_trace_id_t b);
int airy_msg_id_eq(airy_msg_id_t a, airy_msg_id_t b);

/**
 * @brief 结构化命名（写入 out，须 >= *_STR_MAX）。
 * @param out [out] 输出缓冲（终止符保证）
 * @param out_cap [in] 输出容量（< *_STR_MAX 时安全截断/返回空串）
 */
void airy_trace_id_to_string(airy_trace_id_t id, char *out, size_t out_cap);
void airy_msg_id_to_string(airy_msg_id_t id, char *out, size_t out_cap);

/**
 * @brief 从结构化命名解析（"tr-<16 hex>" / "msg-<ts>-<seq>"）。
 * @return 解析成功返回 ID；格式非法返回 *_NULL
 */
airy_trace_id_t airy_trace_id_from_string(const char *str);
airy_msg_id_t airy_msg_id_from_string(const char *str);


/*
 * The following types are defined in platform.h and only referenced here:
 * - airy_thread_t
 * - airy_thread_id_t
 * - airy_mtx_t
 * - airy_cond_t
 * - airy_sock_t
 * - airy_process_t
 * - airy_pid_t
 */


/**
 * @section IPC type architecture
 *
 * AgentRT uses a **layered IPC architecture** following microkernel design
 * principles (Liedtke's microkernel principles):
 *
 * **Level 1: kernel-level IPC**
 * - Type: airy_kernel_ipc_message_t
 * - Location: corekern/include/ipc.h
 * - Purpose: inter-process communication inside the microkernel
 * - Features:
 *   ✓ Lightweight structure (40 bytes): code, data, size, fd, msg_id
 *   ✓ Zero external dependencies (does not depend on commons)
 *   ✓ Extreme performance (microsecond latency)
 *   ✓ Simple to use (suited for kernel-mode programming)
 *
 * **Level 2: application-level IPC ([SC] 共享契约，SSoT)**
 * - Type: airy_ipc_msg_hdr (Layout C v4, 128B, magic 0x41524531 'ARE1')
 * - Location: commons/include/airymax/ipc.h (单一权威源，跨态字节级共享)
 * - Purpose: cross-module, application-layer and inter-service communication
 * - Features:
 *   ✓ 128B 定长头 + magic ARE1 + trace_id + capability_badge + crc32
 *   ✓ _Static_assert 逐字段偏移校验
 *   ✓ 与 agent-linux (AirymaxOS) 内核态字节级一致（IRON-9 [SC] 契约）
 *
 * **Level 3: IPC module internal types (implementation detail)**
 * - Type: ipc_message_t + ipc_message_header_t
 * - Location: commons/utils/ipc/include/ipc_common.h
 * - Purpose: IPC subsystem internal implementation
 * - Features: contains implementation-detail fields (reserved, etc.); must not
 *   be used in public APIs
 *
 * **Design rationale:**
 * 1. **Microkernel purity**: corekern depends on no external library, keeping
 *    it minimal
 * 2. **Performance**: kernel-level IPC avoids unnecessary memory copies and
 *    parsing overhead
 * 3. **Separation of concerns**: the kernel provides mechanisms; the
 *    application layer handles policy and features
 * 4. **Forward compatibility**: the two-level architecture allows independent
 *    evolution without affecting each other
 *
 * **Usage guide:**
 * - Inside corekern modules → use airy_kernel_ipc_message_t (corekern/ipc.h)
 * - Application/cross-service layer → use airy_ipc_msg_hdr ([SC] airymax/ipc.h)
 * - Cross-layer communication → use the conversion functions (see below)
 */
typedef uint64_t airy_task_id_t;

/**
 * @brief Message ID type
 */
typedef uint64_t airy_message_id_t;


/*
 * Function interface contract standards:
 * 1. All platform-related functions return int (0 on success, negative error code)
 * 2. Parameter order: output parameters first, input parameters last (C convention)
 * 3. Error handling: use the unified error code definitions
 */

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UNIFIED_TYPES_H */