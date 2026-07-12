/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * ipc.h — [SC] Shared Contract Layer: IPC message header
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.7
 * IRON-9 v2 [SC] layer — byte-identical shared between agentrt and agentrt-linux.
 *
 * DO NOT redefine this structure in agentrt or agentrt-linux source.
 * Reference via: #include <airymax/ipc.h>
 */

#ifndef _AIRY_IPC_H
#define _AIRY_IPC_H

#include <airymax/uapi_compat.h>

/* IPC magic: 0x41524531 = 'ARE1' (Airymax Runtime Engine v1) */
#define AIRY_IPC_MAGIC	0x41524531u

/* Fixed 128-byte message header */
#define AIRY_IPC_HDR_SZ	128

/* SQE/CQE operation codes */
enum airy_ipc_op {
	AIRY_IPC_OP_SEND		= 0,
	AIRY_IPC_OP_RECV		= 1,
	AIRY_IPC_OP_SEND_BATCH	= 2,
	AIRY_IPC_OP_CANCEL		= 3,
};

/* Message flags */
#define AIRY_IPC_F_NOWAIT	(1u << 0)
#define AIRY_IPC_F_SIGNAL	(1u << 1)

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

/**
 * struct airy_ipc_msg_hdr - IPC message header (128 bytes, [SC] shared)
 * @magic: Must be AIRY_IPC_MAGIC (0x41524531 'ARE1').
 * @opcode: Operation code; see enum airy_ipc_op.
 * @flags: Message flags (AIRY_IPC_F_*).
 * @trace_id: Distributed trace identifier for cross-daemon tracing.
 * @timestamp_ns: Timestamp in nanoseconds (CLOCK_REALTIME).
 * @src_task: Source task identifier.
 * @dst_task: Destination task identifier.
 * @payload_len: Payload length in bytes (excludes this 128B header).
 * @reserved: 84 bytes padding, must be zero.
 *
 * Fixed 128-byte header, layout never changes. Shared between
 * agentrt (user-space) and agentrt-linux (kernel) via [SC] contract layer.
 * Recipients MUST validate magic before processing.
 */
struct airy_ipc_msg_hdr {
	__u32	magic;			/* offset 0, 'ARE1' (0x41524531) */
	__u16	opcode;			/* offset 4 */
	__u16	flags;			/* offset 6 */
	__u64	trace_id;		/* offset 8 */
	__u64	timestamp_ns;		/* offset 16 */
	__u64	src_task;		/* offset 24 */
	__u64	dst_task;		/* offset 32 */
	__u32	payload_len;		/* offset 40 */
	__u8	reserved[84];		/* offset 44, 84 bytes */
} __attribute__((packed));

_Static_assert(sizeof(struct airy_ipc_msg_hdr) == 128,
	"IPC message header must be exactly 128 bytes");

#endif /* _AIRY_IPC_H */
