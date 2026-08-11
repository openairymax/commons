/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

/*
 *
 * A-TD (Airymax Task Descriptor) — [SC] shared contract header.
 *
 * 128-byte task descriptor Layout（与 A-IPC 128 字节消息头 Layout C v4 对齐风格）：
 *   magic: 0x41475453 ('AGTS')，独立于 IPC 消息头 magic 0x41524531 ('ARE1')。
 *   CRC32 覆盖 header[0:72) + payload（crc32 字段自身不参与计算），
 *   算法与 IPC C-S12 一致（CRC-32 IEEE 802.3，多项式 0xEDB88320）。
 *
 * 完整性校验：airy_task_desc_validate() 依次检查 magic/version/reserved/CRC32，
 * 与 agentrt-linux 协作契约保持一致（见 CHANGELOG 任务描述符完整性校验）。
 */

#ifndef _UAPI_AIRYMAX_TASK_DESC_H
#define _UAPI_AIRYMAX_TASK_DESC_H

#include <airymax/uapi_compat.h>
#include <airymax/error.h>

#include <stddef.h> /* size_t */
/* ─── Constants ──────────────────────────────────────────────────────── */
#define AIRY_TASK_DESC_MAGIC 0x41475453u /* 'AGTS' */
#define AIRY_TASK_DESC_VERSION 1
#define AIRY_TASK_DESC_HDR_SIZE 128

/* ─── Task Opcodes ───────────────────────────────────────────────────── */
#define AIRY_TASK_OP_CREATE 0x0001
#define AIRY_TASK_OP_EXECUTE 0x0002
#define AIRY_TASK_OP_CANCEL 0x0003
#define AIRY_TASK_OP_STATUS 0x0004
/* ─── Task Flags ─────────────────────────────────────────────────────── */
#define AIRY_TASK_FLAG_CRITICAL 0x0001
#define AIRY_TASK_FLAG_DETACHED 0x0002
#define AIRY_TASK_FLAG_RESERVED 0xFFFC

struct airy_task_desc {
    __u32 magic; /* offset  0:  AIRY_TASK_DESC_MAGIC ('AGTS') */
    __u16 version;
    __u16 opcode;
    __u64 task_id;
    __u64 parent_task_id;
    __u64 submit_time_ns;
    __u64 deadline_ns;
    __u64 src_task;
    __u64 dst_task;
    __u32 payload_len;
    __u32 flags;
    __u32 priority;
    __u32 crc32;
    __u8 reserved[56];
} __attribute__((aligned(64)));

_Static_assert(sizeof(struct airy_task_desc) == AIRY_TASK_DESC_HDR_SIZE,
               "airy_task_desc must be exactly 128 bytes");

_Static_assert(offsetof(struct airy_task_desc, magic) == 0,
               "airy_task_desc.magic must be at offset 0");
_Static_assert(offsetof(struct airy_task_desc, version) == 4,
               "airy_task_desc.version must be at offset 4");
_Static_assert(offsetof(struct airy_task_desc, opcode) == 6,
               "airy_task_desc.opcode must be at offset 6");
_Static_assert(offsetof(struct airy_task_desc, task_id) == 8,
               "airy_task_desc.task_id must be at offset 8");
_Static_assert(offsetof(struct airy_task_desc, payload_len) == 56,
               "airy_task_desc.payload_len must be at offset 56");
_Static_assert(offsetof(struct airy_task_desc, crc32) == 68,
               "airy_task_desc.crc32 must be at offset 68");
_Static_assert(offsetof(struct airy_task_desc, reserved) == 72,
               "airy_task_desc.reserved must be at offset 72");

/* ─── API ────────────────────────────────────────────────────────────── */
/**
 * @brief 计算 CRC-32（IEEE 802.3）校验和
 *
 * 与 IPC 消息头校验同一算法（多项式 0xEDB88320 反射形式、
 * 初始值/最终异或 0xFFFFFFFF、输入输出反射）。
 *
 * @param data 数据指针（NULL 且 len=0 时返回 0x00000000）
 * @param len  数据长度（字节）
 * @return CRC32 校验值
 */
__u32 airy_task_desc_crc32(const void *data, size_t len);

/**
 * @brief 创建任务描述符
 *
 * 填充 magic（'AGTS'）、version、opcode、任务标识、时间戳、payload 长度、
 * flags、priority，并计算 CRC32（覆盖 header[0:72) + payload）。
 *
 * @param desc            输出描述符（调用方提供 128 字节对齐缓冲区）
 * @param opcode          任务操作码（AIRY_TASK_OP_*）
 * @param task_id         任务标识符
 * @param parent_task_id  父任务标识符（0=根任务）
 * @param deadline_ns     截止时间戳（0=无截止）
 * @param src_task        源任务标识
 * @param dst_task        目标任务标识
 * @param payload         任务负载（可为 NULL，此时 payload_len 必须为 0）
 * @param payload_len     负载长度（字节）
 * @param flags           任务标志（保留位必须为 0）
 * @param priority        调度优先级
 * @return AIRY_EOK 成功；AIRY_EINVAL 参数无效
 */
airy_err_t airy_task_desc_create(struct airy_task_desc *desc, __u16 opcode, __u64 task_id,
                                 __u64 parent_task_id, __u64 deadline_ns, __u64 src_task,
                                 __u64 dst_task, const void *payload, __u32 payload_len,
                                 __u32 flags, __u32 priority);

/**
 * @brief 校验任务描述符完整性
 *
 * 依次检查：
 *   1. 指针/长度参数有效（AIRY_EINVAL）
 *   2. magic == AIRY_TASK_DESC_MAGIC（AIRY_EIPC_MAGIC）
 *   3. version == AIRY_TASK_DESC_VERSION（AIRY_ECFGVERSION）
 *   4. flags 保留位为零（AIRY_EIPC_FLAGS）
 *   5. reserved[56] 全为零（AIRY_EIPC_RESERVED）
 *   6. payload_len 与提供的 payload 匹配且未超界（AIRY_EIPC_PAYLOAD）
 *   7. CRC32（header[0:72) + payload）匹配（AIRY_EIPC_CRC32）
 *
 * @param desc        描述符指针
 * @param payload     对应负载（可为 NULL，此时要求 desc->payload_len == 0）
 * @param payload_len 提供的负载长度（必须 >= desc->payload_len）
 * @return AIRY_EOK 校验通过；否则返回对应错误码
 */
airy_err_t airy_task_desc_validate(const struct airy_task_desc *desc, const void *payload,
                                   __u32 payload_len);

#endif /* _UAPI_AIRYMAX_TASK_DESC_H */
