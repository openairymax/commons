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


#include "../platform/include/platform.h"


#include <airymax/ipc.h> /* AIRY_IPC_MAGIC (0x41524531 'ARE1') */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Error code type
 * @details All error codes are negative; success is 0. SSoT scheme A
 *          (negative POSIX errno values). See
 *          docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.5.
 */
typedef int32_t airy_err_t;

/**
 * @brief Success return value
 * @note AIRY_EOK and AIRY_SUCCESS are equivalent, both 0. Prefer AIRY_EOK
 *       (consistent with the POSIX E* naming style).
 */
#define AIRY_SUCCESS 0
#define AIRY_EOK 0

/**
 * @brief Common error code definitions (authoritative, scheme A: negative POSIX errno)
 * @details Error codes with a POSIX errno counterpart use the negative POSIX
 *          errno value (see Linux errno.h); those without keep custom
 *          negative values (e.g. AIRY_ENOTINIT, AIRY_ECANCELLED).
 *          Cross-platform compatibility: values are fixed (independent of the
 *          target platform errno.h) and only aligned with Linux errno as a
 *          naming reference.
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
 * **Level 2: application-level IPC**
 * - Type: airy_ipc_message_t + airy_ipc_header_t
 * - Location: this file (authoritative definition)
 * - Purpose: cross-module, application-layer and inter-service communication
 * - Features:
 *   ✓ Full metadata (magic, version, source, target, etc.)
 *   ✓ Standardized interface (serialization and checksum support)
 *   ✓ Feature-rich (RPC, Pub/Sub, streaming)
 *   ✓ Cross-platform (Windows/Linux/macOS)
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
 * - Inside corekern modules → use airy_kernel_ipc_message_t
 * - In daemons/services/application layer → use airy_ipc_message_t
 * - Cross-layer communication → use the conversion functions (see below)
 */

/**
 * @brief IPC message header structure (authoritative)
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
 * @brief Application-level IPC message structure (authoritative)
 * @note This is the standard application-layer airy_ipc_message_t definition,
 *       distinct from the kernel-level airy_kernel_ipc_message_t
 */
typedef struct {
    airy_ipc_header_t header;
    void *payload;
    size_t payload_size;
} airy_ipc_message_t;


/*
 * Kernel-level IPC message type notes:
 *
 * Type name: airy_kernel_ipc_message_t
 * Definition location: corekern/include/ipc.h
 * Purpose: inter-process communication inside the microkernel
 *          (lightweight, high performance)
 *
 * Use cases:
 * - When a daemon service needs to convert an application-level message to a
 *   kernel-level message
 * - When bridging between different IPC layers
 *
 * Note: this type is only used inside the corekern module; the application
 * layer should use airy_ipc_message_t
 */


/**
 * @brief Task ID type
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