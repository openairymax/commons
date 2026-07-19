/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * ipc.h — [SC] Shared Contract Layer: IPC message header (Layout C v4)
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.7
 *       docs-closed/agentrt-linux/00-reviews/_review_v2.2/37-capability-folding-decision-and-roadmap.md §6.1
 * IRON-9 v2 [SC] layer — byte-identical shared between agentrt and agentrt-linux.
 *
 * Capability Folding v1.1 (Layout C v4) hard constraints (H1-H6):
 *   H1: Total length remains 128 bytes, magic remains 0x41524531 'ARE1'.
 *   H2: capability_badge field belongs to [SC] ipc.h; its validation is [SS] layer.
 *   H3: agentrt user-space: capability_badge is always 0 (uses traditional cap_t).
 *   H4: agentrt-linux kernel: capability_badge is compiled by sec_d, validated by fastpath C-S9.
 *   H5: Pure-C LSM module unchanged; Badge validation is its fastpath inline.
 *   H6: [DSL] degraded mode: capability_badge field exists but is ignored (value=0, skip C-S9).
 *
 * DO NOT redefine this structure in agentrt or agentrt-linux source.
 * Reference via: #include <airymax/ipc.h>
 */

#ifndef _AIRY_IPC_H
#define _AIRY_IPC_H

#include <airymax/uapi_compat.h>

/* IPC magic: 0x41524531 = 'ARE1' (Airymax Runtime Engine v1) */
#define AIRY_IPC_MAGIC	0x41524531u

/* Fixed 128-byte message header (Layout C v4) */
#define AIRY_IPC_HDR_SZ	128

/* SQE/CQE operation codes.
 *
 * v1.0 baseline opcodes (0..3) are preserved for backward compatibility
 * with existing agentrt internal code; v1.1 adds FREEZE/CAP_REQUEST/
 * CAP_RESPONSE per Capability Folding decision (37-capability-folding-
 * decision-and-roadmap.md §6.2). */
enum airy_ipc_op {
	AIRY_IPC_OP_SEND		= 0,	/* v1.0: single IPC message send */
	AIRY_IPC_OP_RECV		= 1,	/* v1.0: receive statement */
	AIRY_IPC_OP_SEND_BATCH	= 2,	/* v1.0: batch send (atomic multi-message) */
	AIRY_IPC_OP_CANCEL		= 3,	/* v1.0: cancel outstanding SQE */
	AIRY_IPC_OP_FREEZE		= 0x0005,	/* v1.1: freeze ring (ring->frozen=true) */
	AIRY_IPC_OP_CAP_REQUEST	= 0x0010,	/* v1.1: agent → sec_d request Badge (bootstrap) */
	AIRY_IPC_OP_CAP_RESPONSE	= 0x0011,	/* v1.1: sec_d → agent compiled Badge reply */
};

/* Message flags */
#define AIRY_IPC_F_NOWAIT	(1u << 0)
#define AIRY_IPC_F_SIGNAL	(1u << 1)

/* v1.1 Capability Folding flags (37 §6.x / 35 §0.5) */
#define AIRY_IPC_F_ZEROCOPY	(1u << 2)	/* zero-copy registered buffer */
#define AIRY_IPC_F_CAP_CARRY	(1u << 3)	/* carry capability_badge (H4) */
#define AIRY_IPC_F_ENCRYPT	(1u << 4)	/* payload encrypted */
#define AIRY_IPC_F_COMPRESS	(1u << 5)	/* payload compressed */
#define AIRY_IPC_F_BATCH_TAIL	(1u << 6)	/* last message in SEND_BATCH */

/* SQE flags for io_uring IPC operations */
#define AIRY_IPC_SQE_F_FIXED_BUF	(1u << 0)
#define AIRY_IPC_SQE_F_ASYNC		(1u << 1)
#define AIRY_IPC_SQE_F_BUF_SELECT	(1u << 2)
#define AIRY_IPC_SQE_F_SKIP_CQE	(1u << 3)

/* CQE flags for io_uring IPC operations */
#define AIRY_IPC_CQE_F_BUFFER	(1u << 0)
#define AIRY_IPC_CQE_F_MORE		(1u << 1)
#define AIRY_IPC_CQE_F_NOTIF		(1u << 2)

/* Ring capacity constants */
#define AIRY_IPC_RING_DEF_ENTRIES	256
#define AIRY_IPC_RING_MAX_ENTRIES	32768

/* ============================================================================
 * Capability Folding v1.1 — Badge 64-bit Native Word
 *
 * Layout (little-endian bit numbering, 37 §2.2):
 *   63                    48 47                16 15            0
 *   ┌──────────────────────┬─────────────────────┬──────────────┐
 *   │   Epoch (16 bits)    │  Random Tag (32 bits)│ Perms (16 bits)│
 *   └──────────────────────┴─────────────────────┴──────────────┘
 *
 *   Epoch (16 bits):      Global generation snapshot; revocation via atomic_inc.
 *   Random Tag (32 bits): per-Agent random tag, compiled by sec_d, anti-forgery.
 *   Perms (16 bits):      Permission bitmap (SEND/RECV/CALL/GRANT/REVOKE/FREEZE/BATCH).
 *
 * H3 (agentrt user-space): capability_badge is always 0 — uses traditional cap_t.
 * H4 (agentrt-linux kernel): capability_badge is compiled by sec_d, validated by
 *     fastpath C-S9 (~10ns, 3 READ_ONCE + bit ops).
 * H6 ([DSL] degraded mode): capability_badge field exists but is ignored (skip C-S9).
 * ============================================================================ */

/* Permission bits for airy_badge_perms (16-bit field) */
#define AIRY_CAP_PERM_SEND	(1u << 0)	/* SEND opcode allowed */
#define AIRY_CAP_PERM_RECV	(1u << 1)	/* RECV opcode allowed */
#define AIRY_CAP_PERM_CALL	(1u << 2)	/* CALL (sync send+recv) allowed */
#define AIRY_CAP_PERM_GRANT	(1u << 3)	/* GRANT (derive capability) allowed */
#define AIRY_CAP_PERM_REVOKE	(1u << 4)	/* REVOKE (revoke capability) allowed */
#define AIRY_CAP_PERM_FREEZE	(1u << 5)	/* FREEZE ring allowed */
#define AIRY_CAP_PERM_BATCH	(1u << 6)	/* SEND_BATCH allowed */
#define AIRY_CAP_PERM_ALL	(0x007Fu)	/* all 7 perms */

/* Badge field extraction macros (35-aipc-final-redesign.md §2.3). */
#define AIRY_BADGE_EPOCH(badge)		((__u16)(((__u64)(badge) >> 48) & 0xFFFFu))
#define AIRY_BADGE_RANDTAG(badge)	((__u32)(((__u64)(badge) >> 16) & 0xFFFFFFFFu))
#define AIRY_BADGE_PERMS(badge)		((__u16)((__u64)(badge) & 0xFFFFu))

/* Badge construction macro (sec_d compile path; agentrt user-space does not use) */
#define AIRY_BADGE_BUILD(epoch, randtag, perms) \
	(((__u64)((__u16)(epoch)) << 48) | \
	 ((__u64)((__u32)(randtag)) << 16) | \
	 ((__u64)((__u16)(perms))))

/**
 * struct airy_ipc_msg_hdr - IPC message header (128 bytes, [SC] shared, Layout C v4)
 * @magic: Must be AIRY_IPC_MAGIC (0x41524531 'ARE1').
 * @opcode: Operation code; see enum airy_ipc_op.
 * @flags: Message flags (AIRY_IPC_F_*).
 * @trace_id: Distributed trace identifier for cross-daemon tracing.
 * @timestamp_ns: Timestamp in nanoseconds (CLOCK_REALTIME).
 * @src_task: Source task identifier.
 * @dst_task: Destination task identifier.
 * @payload_len: Payload length in bytes (excludes this 128B header).
 * @capability_badge: Capability Folding v1.1 Badge (offset 44-51). H3: agentrt
 *     user-space always sets this to 0; H4: agentrt-linux sec_d compiles it;
 *     H6: [DSL] degraded mode ignores it.
 * @crc32: CRC32 covering header[0:52) + payload (offset 52-55, v1.1).
 * @reserved: 72 bytes padding, must be zero (offset 56-127, v1.1).
 *
 * Fixed 128-byte header, layout never changes. Shared between
 * agentrt (user-space) and agentrt-linux (kernel) via [SC] contract layer.
 * Recipients MUST validate magic before processing.
 *
 * Layout C v4 field map (37 §6.1):
 *   offset  0: magic            (4B)
 *   offset  4: opcode           (2B)
 *   offset  6: flags            (2B)
 *   offset  8: trace_id         (8B)
 *   offset 16: timestamp_ns     (8B)
 *   offset 24: src_task         (8B)
 *   offset 32: dst_task         (8B)
 *   offset 40: payload_len      (4B)
 *   offset 44: capability_badge (8B)  ← v1.1 Capability Folding physical carrier
 *   offset 52: crc32            (4B)  ← v1.1 integrity check
 *   offset 56: reserved[72]     (72B) ← v1.1 shrunk from v1.0 reserved[84]
 *   total:   128 bytes
 */
struct airy_ipc_msg_hdr {
	__u32	magic;			/* offset 0,  'ARE1' (0x41524531) */
	__u16	opcode;			/* offset 4,  操作码（见 enum airy_ipc_op） */
	__u16	flags;			/* offset 6,  消息标志（AIRY_IPC_F_*） */
	__u64	trace_id;		/* offset 8,  分布式追踪 ID（贯穿全链路） */
	__u64	timestamp_ns;		/* offset 16, 纳秒时间戳（CLOCK_REALTIME） */
	__u64	src_task;		/* offset 24, 源任务 ID（整型 task ID，非字符串） */
	__u64	dst_task;		/* offset 32, 目标任务 ID（整型 task ID，非字符串） */
	__u32	payload_len;		/* offset 40, 负载长度（不含本 128B 头） */
	__u64	capability_badge;	/* offset 44, Capability Folding Badge (H3: agentrt=0) */
	__u32	crc32;			/* offset 52, CRC32 覆盖 header[0:52)+payload */
	__u8	reserved[72];		/* offset 56, 72 字节保留，必须为零 */
} __attribute__((packed));

_Static_assert(sizeof(struct airy_ipc_msg_hdr) == 128,
	"IPC message header must be exactly 128 bytes (Layout C v4)");

/* Field offset assertions — guard against accidental layout drift. */
_Static_assert(offsetof(struct airy_ipc_msg_hdr, magic) == 0,
	"airy_ipc_msg_hdr.magic must be at offset 0");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, opcode) == 4,
	"airy_ipc_msg_hdr.opcode must be at offset 4");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, capability_badge) == 44,
	"airy_ipc_msg_hdr.capability_badge must be at offset 44 (Layout C v4)");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, crc32) == 52,
	"airy_ipc_msg_hdr.crc32 must be at offset 52 (Layout C v4)");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, reserved) == 56,
	"airy_ipc_msg_hdr.reserved must be at offset 56 (Layout C v4)");

#endif /* _AIRY_IPC_H */
