/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

/*
 *
 * A-TD (Airymax Task Descriptor) — [SC] shared contract header.
 *
 * 128-byte task descriptor layout (aligned with the A-IPC 128-byte message
 * header Layout C v4 style):
 *   magic: 0x41475453 ('AGTS'), independent of the IPC header magic
 *   0x41524531 ('ARE1').
 *   CRC32 covers header[0:72) + payload (the crc32 field itself is excluded
 *   from computation), using the same algorithm as IPC C-S12
 *   (CRC-32 IEEE 802.3, polynomial 0xEDB88320).
 *
 * Integrity check: airy_task_desc_validate() checks magic/version/reserved/
 * CRC32 in order, consistent with the agent-linux cooperation contract
 * (see CHANGELOG "task descriptor integrity check").
 *
 * 命名说明（2026-08-21）：struct 命名为 airy_task_desc_hdr，与
 * airymax/sched.h 的 64B struct airy_task_desc（调度运行时描述符，
 * AIRY_TASK_MAGIC）区分——两结构 magic 值相同（'AGTS'）但布局不同
 * （本头 128B 任务提交/分发描述符 vs sched.h 64B 调度描述符），
 * 属不同契约域，不得混用。
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

struct airy_task_desc_hdr {
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

_Static_assert(sizeof(struct airy_task_desc_hdr) == AIRY_TASK_DESC_HDR_SIZE,
               "airy_task_desc must be exactly 128 bytes");

_Static_assert(offsetof(struct airy_task_desc_hdr, magic) == 0,
               "airy_task_desc.magic must be at offset 0");
_Static_assert(offsetof(struct airy_task_desc_hdr, version) == 4,
               "airy_task_desc.version must be at offset 4");
_Static_assert(offsetof(struct airy_task_desc_hdr, opcode) == 6,
               "airy_task_desc.opcode must be at offset 6");
_Static_assert(offsetof(struct airy_task_desc_hdr, task_id) == 8,
               "airy_task_desc.task_id must be at offset 8");
_Static_assert(offsetof(struct airy_task_desc_hdr, payload_len) == 56,
               "airy_task_desc.payload_len must be at offset 56");
_Static_assert(offsetof(struct airy_task_desc_hdr, crc32) == 68,
               "airy_task_desc.crc32 must be at offset 68");
_Static_assert(offsetof(struct airy_task_desc_hdr, reserved) == 72,
               "airy_task_desc.reserved must be at offset 72");

/* ─── API ────────────────────────────────────────────────────────────── */
/**
 * @brief Compute CRC-32 (IEEE 802.3) checksum
 *
 * Same algorithm as the IPC message header checksum (reflected form of
 * polynomial 0xEDB88320, init/final xor 0xFFFFFFFF, input/output reflected).
 *
 * @param data data pointer (returns 0x00000000 when NULL and len=0)
 * @param len  data length in bytes
 * @return CRC32 checksum value
 */
__u32 airy_task_desc_crc32(const void *data, size_t len);

/**
 * @brief Create a task descriptor
 *
 * Fills magic ('AGTS'), version, opcode, task identifiers, timestamps,
 * payload length, flags and priority, then computes CRC32
 * (covering header[0:72) + payload).
 *
 * @param desc            output descriptor (caller provides a 128-byte aligned buffer)
 * @param opcode          task opcode (AIRY_TASK_OP_*)
 * @param task_id         task identifier
 * @param parent_task_id  parent task identifier (0 = root task)
 * @param deadline_ns     deadline timestamp (0 = no deadline)
 * @param src_task        source task identifier
 * @param dst_task        destination task identifier
 * @param payload         task payload (may be NULL, then payload_len must be 0)
 * @param payload_len     payload length in bytes
 * @param flags           task flags (reserved bits must be 0)
 * @param priority        scheduling priority
 * @return AIRY_EOK on success; AIRY_EINVAL on invalid arguments
 */
airy_err_t airy_task_desc_create(struct airy_task_desc_hdr *desc, __u16 opcode, __u64 task_id,
                                 __u64 parent_task_id, __u64 deadline_ns, __u64 src_task,
                                 __u64 dst_task, const void *payload, __u32 payload_len,
                                 __u32 flags, __u32 priority);

/**
 * @brief Validate task descriptor integrity
 *
 * Checks in order:
 *   1. Pointer/length arguments valid (AIRY_EINVAL)
 *   2. magic == AIRY_TASK_DESC_MAGIC (AIRY_EIPC_MAGIC)
 *   3. version == AIRY_TASK_DESC_VERSION (AIRY_ECFGVERSION)
 *   4. flags reserved bits zero (AIRY_EIPC_FLAGS)
 *   5. reserved[56] all zero (AIRY_EIPC_RESERVED)
 *   6. payload_len matches the provided payload and is within bounds
 *      (AIRY_EIPC_PAYLOAD)
 *   7. CRC32 (header[0:72) + payload) matches (AIRY_EIPC_CRC32)
 *
 * @param desc        descriptor pointer
 * @param payload     corresponding payload (may be NULL, then desc->payload_len must be 0)
 * @param payload_len provided payload length (must be >= desc->payload_len)
 * @return AIRY_EOK on success; otherwise the corresponding error code
 */
airy_err_t airy_task_desc_validate(const struct airy_task_desc_hdr *desc, const void *payload,
                                   __u32 payload_len);

#endif /* _UAPI_AIRYMAX_TASK_DESC_H */
