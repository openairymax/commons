/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * syscalls.h — [SC] Shared Contract Layer: syscall number assignments
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.8
 * IRON-9 v2 [SC] layer — byte-identical shared between agentrt and agentrt-linux.
 *
 * DO NOT redefine these numbers in agentrt or agentrt-linux source.
 * Reference via: #include <airymax/syscalls.h>
 */

#ifndef _AIRY_SYSCALLS_H
#define _AIRY_SYSCALLS_H

/*
 * Agent syscall architecture: 12 core + 12 reserved = 24 slots total
 *
 * Core 12:
 *   IPC Primitives (8): Call/Send/Recv/NBSend/NBRecv/ReplyRecv/Yield/Reply
 *   Control Primitives (3): RovolCtl/SchedCtl/CltNotify
 *   Notification (1): Notify
 *
 * LsmCtl and WasmLoad are subsumed under Call via capability invocation:
 *   - LSM policy load -> airy_sys_call(security_cap, &msg)
 *   - Wasm module load -> airy_sys_call(module_cap, &msg)
 *
 * Inspired by seL4's 8-activity syscall model — all capability
 * operations are encoded in IPC messages, not as separate syscalls.
 * Data plane I/O is handled by io_uring (zero syscall).
 * Reply completes the seL4 8-activity set (Reply without Recv).
 * Notify provides async inter-agent signaling (seL4 Notification).
 */
#define AIRY_SYS_CALL		0	/* Unified capability invocation */
#define AIRY_SYS_SEND		1	/* Blocking synchronous send */
#define AIRY_SYS_RECV		2	/* Blocking synchronous receive */
#define AIRY_SYS_NBSEND	3	/* Non-blocking send */
#define AIRY_SYS_NBRECV	4	/* Non-blocking receive */
#define AIRY_SYS_REPLY_RECV	5	/* Reply and wait for next */
#define AIRY_SYS_YIELD	6	/* Yield CPU */
#define AIRY_SYS_ROVOL_CTL	7	/* Memory snapshot/restore/tier */
#define AIRY_SYS_SCHED_CTL	8	/* Scheduling policy config */
#define AIRY_SYS_CLT_NOTIFY	9	/* CoreLoopThree phase + kthread */
#define AIRY_SYS_REPLY	10	/* Standalone reply (no wait) */
#define AIRY_SYS_NOTIFY	11	/* Async notification signal */
/* Reserved slots 12-23 for future expansion */

#endif /* _AIRY_SYSCALLS_H */
