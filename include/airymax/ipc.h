/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */
/*
 *
 * A-IPC (Unified Airymax IPC Fabric) — [SC] shared contract header.
 *
 * 128-byte message header Layout C v4 with Capability Folding.
 * Badge 64-bit Native Word layout: Epoch<<48 | RandomTag<<16 | Perms.
 * magic: 0x41524531 ('ARE1').
 */

#ifndef _UAPI_AIRYMAX_IPC_H
#define _UAPI_AIRYMAX_IPC_H

#include <airymax/uapi_compat.h>
#include <airymax/security_types.h>

/* ─── Constants ──────────────────────────────────────────────────────── */
#define AIRY_IPC_MAGIC          0x41524531u /* 'ARE1' */
#define AIRY_IPC_HDR_SIZE       128

/* ─── IPC Opcodes ────────────────────────────────────────────────────── */
#define AIRY_IPC_OP_SEND        0x0001  /* Unicast send */
#define AIRY_IPC_OP_RECV        0x0002  /* Unicast receive */
#define AIRY_IPC_OP_SEND_BATCH  0x0003  /* Batch send */
#define AIRY_IPC_OP_CANCEL      0x0004  /* Cancel pending operation */
#define AIRY_IPC_OP_FREEZE      0x0005  /* Freeze IPC ring */
#define AIRY_IPC_OP_CAP_REQUEST 0x0010  /* Capability request (bootstrap) */
#define AIRY_IPC_OP_CAP_RESPONSE 0x0011 /* Capability response */

/* ─── IPC Flags ────────────────────────────────────────────────────────
 * SSoT: docs/AirymaxOS/30-interfaces/02-ipc-protocol.md §3.1
 *
 * Bit assignment (16-bit __u16 flags field at offset 6):
 *   bits 0-4:   active flags (5 defined)
 *   bits 5-15:  reserved, must be zero (C-S10 validates via RESERVED mask)
 *
 * v1.0.1: NOWAIT/SIGNAL removed — superseded by io_uring IOSQE_ASYNC and
 * IORING_CQE_F_NOTIF (see 20-modules/01-kernel.md §6.3).
 */
#define AIRY_IPC_FLAG_ZEROCOPY   0x0001  /* Zero-copy path enabled */
#define AIRY_IPC_FLAG_CAP_CARRY  0x0002  /* Carrying a capability */
#define AIRY_IPC_FLAG_ENCRYPT    0x0004  /* Payload is encrypted (reserved, 0.1.1 inactive) */
#define AIRY_IPC_FLAG_COMPRESS   0x0008  /* Payload is compressed (reserved, 0.1.1 inactive) */
#define AIRY_IPC_FLAG_BATCH_TAIL 0x0010  /* Last SQE in a batch */
#define AIRY_IPC_FLAG_RESERVED   0xFFE0  /* Bits 5-15: must be zero (C-S10 check) */

/* ─── Badge 64-bit Native Word Bit Layout ────────────────────────────── */
#define AIRY_BADGE_EPOCH_SHIFT  48
#define AIRY_BADGE_RANDTAG_SHIFT 16
#define AIRY_BADGE_PERMS_SHIFT  0

#define AIRY_BADGE_EPOCH_MASK   ((__u64)0xFFFF << AIRY_BADGE_EPOCH_SHIFT)
#define AIRY_BADGE_RANDTAG_MASK ((__u64)0xFFFFFFFF << AIRY_BADGE_RANDTAG_SHIFT)
#define AIRY_BADGE_PERMS_MASK   ((__u64)0xFFFF << AIRY_BADGE_PERMS_SHIFT)

#define AIRY_BADGE_COMPILE(epoch, randtag, perms) \
	(((__u64)((epoch) & 0xFFFF) << AIRY_BADGE_EPOCH_SHIFT) | \
	 ((__u64)((randtag) & 0xFFFFFFFF) << AIRY_BADGE_RANDTAG_SHIFT) | \
	 ((__u64)((perms) & 0xFFFF) << AIRY_BADGE_PERMS_SHIFT))

#define AIRY_BADGE_EPOCH(b)   (((b) & AIRY_BADGE_EPOCH_MASK) >> AIRY_BADGE_EPOCH_SHIFT)
#define AIRY_BADGE_RANDTAG(b) (((b) & AIRY_BADGE_RANDTAG_MASK) >> AIRY_BADGE_RANDTAG_SHIFT)
#define AIRY_BADGE_PERMS(b)   (((b) & AIRY_BADGE_PERMS_MASK) >> AIRY_BADGE_PERMS_SHIFT)

/* ─── IPC Message Header Layout C v4 ─────────────────────────────────── */
struct airy_ipc_msg_hdr {
	__u32   magic;             /* offset 0:  AIRY_IPC_MAGIC */
	__u16   opcode;            /* offset 4:  IPC opcode */
	__u16   flags;             /* offset 6:  message flags */
	__u64   trace_id;          /* offset 8:  distributed trace ID */
	__u64   timestamp_ns;      /* offset 16: monotonic ns timestamp */
	__u64   src_task;          /* offset 24: source task identifier */
	__u64   dst_task;          /* offset 32: destination task identifier */
	__u64   capability_badge;  /* offset 40: Capability Folding badge (64-bit) */
	__u32   payload_len;       /* offset 48: payload length in bytes */
	__u32   crc32;             /* offset 52: CRC32 of payload */
	__u8    reserved[72];      /* offset 56: reserved for future use */
} AIRY_ALIGNED(64);

_Static_assert(sizeof(struct airy_ipc_msg_hdr) == AIRY_IPC_HDR_SIZE,
	       "airy_ipc_msg_hdr must be exactly 128 bytes");

_Static_assert(offsetof(struct airy_ipc_msg_hdr, magic) == 0,
	       "airy_ipc_msg_hdr.magic must be at offset 0");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, opcode) == 4,
	       "airy_ipc_msg_hdr.opcode must be at offset 4");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, capability_badge) == 40,
	       "capability_badge must be at offset 40 (8-byte aligned, D-9 fix)");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, payload_len) == 48,
	       "payload_len must be at offset 48");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, crc32) == 52,
	       "crc32 must be at offset 52");
_Static_assert(offsetof(struct airy_ipc_msg_hdr, reserved) == 56,
	       "reserved must be at offset 56");

/* ─── [DSL] Degraded Survival Layer Fallback Block ──────────────────────
 * When AIRY_SC_FALLBACK is defined, IPC degrades to a minimal 128-byte
 * header overlay (airy_ipc_msg_hdr_min) with capability_badge fixed to 0
 * (H6 hard constraint: fastpath C-S9 skips Badge validation). Only
 * SEND/RECV opcodes are supported; FREEZE/CAP_REQUEST/CAP_RESPONSE are
 * mapped to SEND to preserve compilability. See [DSL] §2.2 and §4.4.
 */
#ifdef AIRY_SC_FALLBACK
	/* H6: capability_badge fixed to 0 — fastpath C-S9 goto cap_pass */
	#define AIRY_DSL_CAPABILITY_BADGE   0ULL

	/* Only SEND/RECV are real opcodes in [DSL]; others map to SEND. */
	#define AIRY_DSL_IPC_OP_SEND         AIRY_IPC_OP_SEND
	#define AIRY_DSL_IPC_OP_RECV         AIRY_IPC_OP_RECV
	#define AIRY_DSL_IPC_OP_SEND_BATCH   AIRY_IPC_OP_SEND
	#define AIRY_DSL_IPC_OP_CANCEL       AIRY_IPC_OP_SEND
	#define AIRY_DSL_IPC_OP_FREEZE       AIRY_IPC_OP_SEND
	#define AIRY_DSL_IPC_OP_CAP_REQUEST  AIRY_IPC_OP_SEND
	#define AIRY_DSL_IPC_OP_CAP_RESPONSE AIRY_IPC_OP_SEND
	#define AIRY_DSL_IPC_OPCODES         2  /* Only SEND + RECV retained */

	/* Minimal 128-byte header overlay (Layout C v4 compatible). */
	struct airy_ipc_msg_hdr_min {
		__u32   magic;             /* offset  0: AIRY_IPC_MAGIC */
		__u16   opcode;            /* offset  4 */
		__u8    _pad0[34];         /* offset  6-39: zeroed (trace/ts/src/dst) */
		__u64   capability_badge;  /* offset 40: fixed 0 (H6) */
		__u32   payload_len;       /* offset 48 */
		__u32   crc32;             /* offset 52 */
		__u8    _pad1[72];         /* offset 56-127: zeroed reserved */
	} AIRY_ALIGNED(64);

	_Static_assert(offsetof(struct airy_ipc_msg_hdr_min, capability_badge) == 40,
		       "H1: [DSL] capability_badge offset must be 40");
	_Static_assert(sizeof(struct airy_ipc_msg_hdr_min) == AIRY_IPC_HDR_SIZE,
		       "[DSL] airy_ipc_msg_hdr_min must overlay airy_ipc_msg_hdr (128 bytes)");

	#warning "AIRY_SC_FALLBACK active: ipc.h degraded to minimal 128B header, capability_badge=0 (H6), only SEND/RECV opcodes"
#endif /* AIRY_SC_FALLBACK */

/* ─── Payload Protocol Layer (02-ipc-protocol.md §3) ───────────────────
 * Wire form: [8B discriminator (struct airy_ipc_payload)][body struct].
 * The 128B header carries no type field — the payload protocol type is
 * carried by the payload's leading discriminator, not by `opcode`
 * (opcode is the transport SQE/CQE operation; the two are different layers).
 *
 * Legacy note: JSON-RPC text payloads that predate the typed framing carry
 * no discriminator (their first byte is '{'); parse helpers must map them
 * to the implicit REQUEST/RESPONSE pair instead of failing.
 */
#define AIRY_IPC_PT_REQUEST  0x0001u /* request side of request-response */
#define AIRY_IPC_PT_RESPONSE 0x0002u /* response side of request-response */
#define AIRY_IPC_PT_EVENT    0x0003u /* publish-subscribe notification */
#define AIRY_IPC_PT_STREAM   0x0004u /* bidirectional stream data */
#define AIRY_IPC_PT_CONTROL  0x0005u /* link management */

/* 8-byte discriminator prepended to every typed payload body. */
struct airy_ipc_payload {
	__u32 type;     /* offset 0: AIRY_IPC_PT_* */
	__u32 reserved; /* offset 4: must be zero */
	__u8  body[];   /* offset 8: body struct, see below */
};

_Static_assert(sizeof(struct airy_ipc_payload) == 8,
	       "airy_ipc_payload discriminator must be exactly 8 bytes");
_Static_assert(offsetof(struct airy_ipc_payload, type) == 0,
	       "payload type must be the first field");
_Static_assert(offsetof(struct airy_ipc_payload, body) == 8,
	       "payload body must start at offset 8");

/* ── REQUEST body (§3.1) ── */
struct airy_ipc_request {
	__u64   request_id; /* correlates with RESPONSE.request_id */
	__u32   method_id;  /* RPC method number */
	__u32   timeout_ms; /* timeout in milliseconds */
	__u8    params[];   /* method parameters (flexible) */
};

/* ── RESPONSE body (§3.2) ── */
struct airy_ipc_response {
	__u64   request_id; /* correlates with REQUEST.request_id */
	__s32   status;     /* 0 success, <0 AIRY_E* error */
	__u32   reserved;   /* must be zero */
	__u8    result[];   /* result data (flexible) */
};

/* ── EVENT body (§3.3) ── */
struct airy_ipc_event {
	__u64   event_id;  /* event identifier */
	__u32   topic_id;  /* topic assigned at subscribe time */
	__u32   priority;  /* event priority (0-139) */
	__u8    payload[]; /* event data (flexible) */
};

/* ── STREAM body (§3.4) ── */
struct airy_ipc_stream {
	__u64   stream_id; /* stream identifier */
	__u32   seq;       /* sequence number (monotonic) */
	__u32   flags;     /* AIRY_IPC_STREAM_FLAG_* */
	__u8    chunk[];   /* stream data chunk (flexible) */
};

#define AIRY_IPC_STREAM_FLAG_FIN  0x00000001u /* final chunk */
#define AIRY_IPC_STREAM_FLAG_RST  0x00000002u /* abort the stream */
#define AIRY_IPC_STREAM_FLAG_MORE 0x00000004u /* more chunks follow */

/* ── CONTROL body (§3.5) ── */
struct airy_ipc_control {
	__u32   opcode; /* AIRY_IPC_CTRL_* */
	__u32   arg;    /* operation argument */
	__u8    data[]; /* additional data (flexible) */
};

#define AIRY_IPC_CTRL_HELLO    0x0001u /* handshake */
#define AIRY_IPC_CTRL_BYE      0x0002u /* close */
#define AIRY_IPC_CTRL_PING     0x0003u /* heartbeat */
#define AIRY_IPC_CTRL_PONG     0x0004u /* heartbeat reply */
#define AIRY_IPC_CTRL_FLOW_OFF 0x0005u /* flow control: pause */
#define AIRY_IPC_CTRL_FLOW_ON  0x0006u /* flow control: resume */

_Static_assert(offsetof(struct airy_ipc_request, request_id) == 0,
	       "REQUEST body first field must be 8-byte aligned");
_Static_assert(offsetof(struct airy_ipc_response, request_id) == 0,
	       "RESPONSE body first field must be 8-byte aligned");
_Static_assert(offsetof(struct airy_ipc_event, event_id) == 0,
	       "EVENT body first field must be 8-byte aligned");
_Static_assert(offsetof(struct airy_ipc_stream, stream_id) == 0,
	       "STREAM body first field must be 8-byte aligned");
_Static_assert(offsetof(struct airy_ipc_control, opcode) == 0 &&
		       offsetof(struct airy_ipc_control, arg) == 4,
	       "CONTROL body layout must match 02-ipc-protocol.md §3.5");

#endif /* _UAPI_AIRYMAX_IPC_H */
