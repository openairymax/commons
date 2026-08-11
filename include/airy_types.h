/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * airy_types.h - AgentRT 统一类型定义权威源
 *
 * 作为全项目唯一的类型定义权威源，解决模块间类型定义冲突。
 * 遵循标准化统一方案，确保跨平台编译兼容性。
 *
 * 设计原则：
 * 1. 权威性：commons作为唯一权威基础库
 * 2. 统一性：全项目使用统一的类型定义和接口契约
 * 3. 兼容性：确保Windows、Linux、macOS三平台兼容
 *
 */

#ifndef AIRY_RT_UNIFIED_TYPES_H
#define AIRY_RT_UNIFIED_TYPES_H


#include "../platform/include/platform.h"


#include <airymax/ipc.h> /* AIRY_IPC_MAGIC (0x41524531 'ARE1') */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief 错误码类型
 * @details 所有错误码为负值，成功为0。SSoT 方案 A（POSIX errno 负值）。
 *          详见 docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.5。
 */
typedef int32_t airy_err_t;

/**
 * @brief 成功返回值
 * @note AIRY_EOK 与 AIRY_SUCCESS 等价，均为 0。推荐使用 AIRY_EOK（与 POSIX E* 命名风格一致）。
 */
#define AIRY_SUCCESS 0
#define AIRY_EOK 0

/**
 * @brief 通用错误码定义（权威定义，方案 A：POSIX errno 负值）
 * @details 有对应 POSIX errno 的错误码使用 POSIX errno 负值（参考 Linux errno.h）；
 *          无对应 POSIX errno 的保留自定义负值（如 AIRY_ENOTINIT、AIRY_ECANCELLED 等）。
 *          跨平台兼容：数值固定（不依赖目标平台 errno.h），仅与 Linux errno 值对齐作为命名参考。
 */
#define AIRY_EPERM (-1)
#define AIRY_ENOENT (-2)
#define AIRY_EINTR (-4)
#define AIRY_EIO (-5)
#define AIRY_E2BIG (-7)
#define AIRY_EAGAIN (-11)
#define AIRY_ENOMEM (-12)
#define AIRY_EACCES (-13)
#define AIRY_EFAULT (-14)
#define AIRY_EBUSY (-16)
#define AIRY_EEXIST (-17)
#define AIRY_EINVAL (-22)
#define AIRY_ENOSPC (-28)
#define AIRY_ERANGE (-34)
#define AIRY_EDEADLK (-35)
#define AIRY_ENOSYS (-38)
#define AIRY_EPROTO (-71)
#define AIRY_EOVERFLOW (-75)
#define AIRY_EMSGSIZE (-90)
#define AIRY_EPROTONOSUPPORT (-93)
#define AIRY_ENOTSUP (-95)
#define AIRY_ECONNRESET (-104)
#define AIRY_ENOTCONN (-107)
#define AIRY_ETIMEDOUT (-110)
#define AIRY_ECONNREFUSED (-111)
#define AIRY_EALREADY (-114)

#define AIRY_ENOTINIT (-9)
#define AIRY_ECANCELLED (-10)
#define AIRY_EUNAVAILABLE (-25)
#define AIRY_EQUOTA (-26)
#define AIRY_EPLATFORM (-27)
#define AIRY_ESERVICE (-29)
#define AIRY_EFAIL (-31)
#define AIRY_EUNKNOWN (-99)


/*
 * 以下类型在platform.h中定义，此处仅作声明引用：
 * - airy_thread_t
 * - airy_thread_id_t
 * - airy_mtx_t
 * - airy_cond_t
 * - airy_sock_t
 * - airy_process_t
 * - airy_pid_t
 */


/**
 * @section IPC类型架构说明
 *
 * AgentRT采用**分层IPC架构**，遵循微内核设计原则（Liedtke微内核原则）：
 *
 * **Level 1: 内核级IPC (Kernel-Level)**
 * - 类型：airy_kernel_ipc_message_t
 * - 位置：corekern/include/ipc.h
 * - 用途：微内核内部进程间通信
 * - 特点：
 *   ✓ 轻量级结构（40字节）：code, data, size, fd, msg_id
 *   ✓ 零外部依赖（不依赖commons）
 *   ✓ 极致性能（微秒级延迟）
 *   ✓ 简单易用（适合内核态编程）
 *
 * **Level 2: 应用级IPC (Application-Level)**
 * - 类型：airy_ipc_message_t + airy_ipc_header_t
 * - 位置：本文件（权威定义）
 * - 用途：跨模块、应用层、服务间通信
 * - 特点：
 *   ✓ 完整元数据（magic, version, source, target等）
 *   ✓ 标准化接口（支持序列化、校验和）
 *   ✓ 功能丰富（RPC、Pub/Sub、流式传输）
 *   ✓ 跨平台兼容（Windows/Linux/macOS）
 *
 * **Level 3: IPC模块内部类型 (Implementation Detail)**
 * - 类型：ipc_message_t + ipc_message_header_t
 * - 位置：commons/utils/ipc/include/ipc_common.h
 * - 用途：IPC子系统内部实现
 * - 特点：包含实现细节字段（reserved等），不应在公共API中使用
 *
 * **设计决策理由：**
 * 1. **微内核纯净性**：corekern不依赖任何外部库，保持最小化
 * 2. **性能优化**：内核级IPC避免不必要的内存拷贝和解析开销
 * 3. **职责分离**：内核关注机制，应用层关注策略和功能
 * 4. **向前兼容**：两级架构允许独立演进，不影响对方
 *
 * **使用指南：**
 * - 在corekern模块内 → 使用 airy_kernel_ipc_message_t
 * - 在daemons/services/应用层 → 使用 airy_ipc_message_t
 * - 跨层通信 → 使用转换函数（见下方）
 */

/**
 * @brief IPC消息头结构（权威定义）
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t type;
    uint32_t flags;
    uint64_t msg_id;
    uint64_t correlation_id;
    char source[64];
    char target[64];
    uint32_t payload_len;
    uint32_t checksum;
    uint64_t timestamp;
} airy_ipc_header_t;

/**
 * @brief 应用级IPC消息结构（权威定义）
 * @note 这是应用层标准的airy_ipc_message_t定义，与内核级airy_kernel_ipc_message_t区分
 */
typedef struct {
    airy_ipc_header_t header;
    void *payload;
    size_t payload_size;
} airy_ipc_message_t;


/*
 * 内核级IPC消息类型说明：
 *
 * 类型名：airy_kernel_ipc_message_t
 * 定义位置：corekern/include/ipc.h
 * 用途：微内核内部进程间通信（轻量级、高性能）
 *
 * 使用场景：
 * - 当daemon服务需要将应用级消息转换为内核级消息时
 * - 当需要在不同IPC层次间桥接时
 *
 * 注意：此类型仅在corekern模块内使用，应用层应使用airy_ipc_message_t
 */


/**
 * @brief 任务ID类型
 */
typedef uint64_t airy_task_id_t;

/**
 * @brief 消息ID类型
 */
typedef uint64_t airy_message_id_t;


/*
 * 函数接口契约标准：
 * 1. 所有平台相关函数返回int类型（0成功，负数错误码）
 * 2. 参数顺序：输出参数在前，输入参数在后（遵循C语言惯例）
 * 3. 错误处理：使用统一的错误码定义
 */

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UNIFIED_TYPES_H */