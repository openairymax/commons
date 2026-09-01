/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file ipc_common.h
 * @brief Inter-process communication module: cross-platform IPC abstraction.
 *
 * 0.1.6 大文件拆分：本文件保留为聚合入口（向后兼容），实际声明分布到：
 *   - ipc_types.h        类型/常量/枚举/结构体/回调
 *   - ipc_core_api.h     核心 API（init/channel/send/receive/broadcast）
 *   - ipc_server_api.h   Server API
 *   - ipc_client_api.h   Client API
 *   - ipc_shm_api.h      Shared Memory API
 *   - ipc_mq_api.h       Message Queue API
 *   - ipc_message_api.h  消息序列化/校验
 *   - ipc_rpc_api.h      RPC API
 *   - ipc_util_api.h     工具与类型转换
 */

#ifndef AIRY_RT_IPC_COMMON_H
#define AIRY_RT_IPC_COMMON_H

#include <error.h>
#include <types.h>
#include <airymax/ipc.h> /* [SC] SSoT: AIRY_IPC_MAGIC (P0-05 convergence) */

#ifdef __cplusplus
extern "C" {
#endif
#include "ipc_types.h"
#include "ipc_core_api.h"
#include "ipc_server_api.h"
#include "ipc_client_api.h"
#include "ipc_shm_api.h"
#include "ipc_mq_api.h"
#include "ipc_message_api.h"
#include "ipc_rpc_api.h"
#include "ipc_util_api.h"
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_IPC_COMMON_H */
