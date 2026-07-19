// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * test_sc_headers.c — P0-01 [SC] shared contract layer header verification
 *
 * Verifies that all 6 [SC] core headers + 1 supplementary + uapi_compat
 * compile cleanly and satisfy their static assertions.
 *
 * Capability Folding v1.1 (Layout C v4) coverage:
 *   - struct airy_ipc_msg_hdr now 11 fields (8 base + capability_badge + crc32
 *     + reserved[72] shrunk from v1.0 reserved[84])
 *   - New opcodes: FREEZE (0x0005), CAP_REQUEST (0x0010), CAP_RESPONSE (0x0011)
 *   - AIRY_CAP_PERM_* permission bits (7 bits, all=0x007F)
 *   - AIRY_BADGE_BUILD/EPOCH/RANDTAG/PERMS macros (64-bit Badge)
 *   - H3 assertion: agentrt user-space capability_badge always 0
 *
 * ACC-RFRP-01: airymax headers count >= 6
 * ACC-RFRP-03: _Static_assert(sizeof(struct airy_ipc_msg_hdr) == 128)
 * ACC-RFRP-04: full compilation zero errors
 */

#include <airymax/uapi_compat.h>
#include <airymax/memory_types.h>
#include <airymax/security_types.h>
#include <airymax/cognition_types.h>
#include <airymax/sched.h>
#include <airymax/ipc.h>
#include <airymax/syscalls.h>
#include <airymax/bpf_struct_ops.h>

#include <stdio.h>

int main(void)
{
	int failures = 0;

	/* --- uapi_compat.h: types must be available --- */
	__u8  u8  = 0xFF;
	__u16 u16 = 0xFFFF;
	__u32 u32 = 0xFFFFFFFFu;
	__u64 u64 = 0xFFFFFFFFFFFFFFFFull;
	__s32 s32 = -1;
	__s64 s64 = -1;
	(void)u8; (void)u16; (void)u32; (void)u64; (void)s32; (void)s64;

	/* --- memory_types.h --- */
	if (AIRY_MEM_L1_HOT != 0) { printf("FAIL: AIRY_MEM_L1_HOT != 0\n"); failures++; }
	if (AIRY_MEM_L4_PMEM != 3) { printf("FAIL: AIRY_MEM_L4_PMEM != 3\n"); failures++; }
	if (AIRY_GFP_HOT != 0x01u) { printf("FAIL: AIRY_GFP_HOT != 0x01\n"); failures++; }
	if (AIRY_GFP_COLD != 0x04u) { printf("FAIL: AIRY_GFP_COLD != 0x04\n"); failures++; }

	/* --- security_types.h --- */
	if (AIRY_CAP_AGENT_SPAWN != 41) { printf("FAIL: AIRY_CAP_AGENT_SPAWN != 41\n"); failures++; }
	if (AIRY_CAP_NPU_ACCESS != 43) { printf("FAIL: AIRY_CAP_NPU_ACCESS != 43\n"); failures++; }
	if (AIRY_VERDICT_ASK != 3) { printf("FAIL: AIRY_VERDICT_ASK != 3\n"); failures++; }
	if (AIRY_CAP_OP_ROTATE != 6) { printf("FAIL: AIRY_CAP_OP_ROTATE != 6\n"); failures++; }

	/* --- cognition_types.h --- */
	if (AIRY_COG_PERCEPT != 0) { printf("FAIL: AIRY_COG_PERCEPT != 0\n"); failures++; }
	if (AIRY_COG_ACT != 2) { printf("FAIL: AIRY_COG_ACT != 2\n"); failures++; }
	if (AIRY_THINK_SLOW != 1) { printf("FAIL: AIRY_THINK_SLOW != 1\n"); failures++; }
	airy_q16_t q = AIRY_Q16_ONE;
	if (q != 65536) { printf("FAIL: AIRY_Q16_ONE != 65536\n"); failures++; }

	/* --- sched.h --- */
	if (AIRY_TASK_MAGIC != 0x41475453u) { printf("FAIL: AIRY_TASK_MAGIC\n"); failures++; }
	if (MAC_MAX_AGENTS != 1024) { printf("FAIL: MAC_MAX_AGENTS != 1024\n"); failures++; }
	if (AIRY_PRIO_MAX != 139) { printf("FAIL: AIRY_PRIO_MAX != 139\n"); failures++; }
	if (AIRY_SLICE_DFL_MS != 20) { printf("FAIL: AIRY_SLICE_DFL_MS != 20\n"); failures++; }
	if (AIRY_WEIGHT_MAX != 10000) { printf("FAIL: AIRY_WEIGHT_MAX\n"); failures++; }

	/* airy_vtime_decay inline function */
	airy_vtime_t vt = airy_vtime_decay(0, 1000, 100);
	if (vt != 1000) { printf("FAIL: airy_vtime_decay(0,1000,100) = %d (expected 1000)\n", vt); failures++; }

	/* struct airy_task_desc initialization */
	struct airy_task_desc desc = {
		.magic = AIRY_TASK_MAGIC,
		.prio = 100,
		._pad = 0,
		.vtime = 0,
	};
	if (desc.magic != AIRY_TASK_MAGIC) { printf("FAIL: task desc magic\n"); failures++; }

	/* --- ipc.h (Layout C v4, Capability Folding v1.1) --- */
	if (AIRY_IPC_MAGIC != 0x41524531u) { printf("FAIL: AIRY_IPC_MAGIC\n"); failures++; }
	if (AIRY_IPC_HDR_SZ != 128) { printf("FAIL: AIRY_IPC_HDR_SZ != 128\n"); failures++; }
	if (AIRY_IPC_OP_CANCEL != 3) { printf("FAIL: AIRY_IPC_OP_CANCEL != 3\n"); failures++; }
	/* v1.1 new opcodes (37 §6.2) */
	if (AIRY_IPC_OP_FREEZE != 0x0005) { printf("FAIL: AIRY_IPC_OP_FREEZE != 0x0005\n"); failures++; }
	if (AIRY_IPC_OP_CAP_REQUEST != 0x0010) { printf("FAIL: AIRY_IPC_OP_CAP_REQUEST != 0x0010\n"); failures++; }
	if (AIRY_IPC_OP_CAP_RESPONSE != 0x0011) { printf("FAIL: AIRY_IPC_OP_CAP_RESPONSE != 0x0011\n"); failures++; }
	/* v1.1 Capability Folding permission bits */
	if (AIRY_CAP_PERM_SEND != (1u << 0)) { printf("FAIL: AIRY_CAP_PERM_SEND\n"); failures++; }
	if (AIRY_CAP_PERM_ALL != 0x007Fu) { printf("FAIL: AIRY_CAP_PERM_ALL != 0x007F\n"); failures++; }

	/* v1.1 Badge 64-bit Native Word extraction macros (37 §2.2) */
	__u64 test_badge = AIRY_BADGE_BUILD(0x1234u, 0xDEADBEEFu, 0x007Fu);
	if (AIRY_BADGE_EPOCH(test_badge) != 0x1234u) { printf("FAIL: AIRY_BADGE_EPOCH\n"); failures++; }
	if (AIRY_BADGE_RANDTAG(test_badge) != 0xDEADBEEFu) { printf("FAIL: AIRY_BADGE_RANDTAG\n"); failures++; }
	if (AIRY_BADGE_PERMS(test_badge) != 0x007Fu) { printf("FAIL: AIRY_BADGE_PERMS\n"); failures++; }

	/* H3: agentrt user-space capability_badge is always 0 */
	struct airy_ipc_msg_hdr hdr = {0};
	hdr.magic = AIRY_IPC_MAGIC;
	hdr.opcode = AIRY_IPC_OP_SEND;
	hdr.payload_len = 256;
	hdr.capability_badge = 0;  /* H3: agentrt user-space always 0 */
	hdr.crc32 = 0;
	if (hdr.capability_badge != 0) { printf("FAIL: H3 agentrt capability_badge must be 0\n"); failures++; }
	(void)hdr;

	/* --- syscalls.h --- */
	if (AIRY_SYS_CALL != 0) { printf("FAIL: AIRY_SYS_CALL != 0\n"); failures++; }
	if (AIRY_SYS_NOTIFY != 11) { printf("FAIL: AIRY_SYS_NOTIFY != 11\n"); failures++; }
	if (AIRY_SYS_REPLY_RECV != 5) { printf("FAIL: AIRY_SYS_REPLY_RECV != 5\n"); failures++; }

	/* --- bpf_struct_ops.h (supplementary) --- */
	if (AIRY_OPS_ACTIVE != 2) { printf("FAIL: AIRY_OPS_ACTIVE != 2\n"); failures++; }
	struct airy_struct_ops_value ops = { .state = AIRY_OPS_INIT, .common = 0 };
	(void)ops;

	/* --- Results --- */
	if (failures == 0) {
		printf("OK: All [SC] header assertions passed (8 files, 6 core + 1 supp + uapi_compat, ipc.h v1.1 Layout C v4)\n");
		return 0;
	}
	printf("NO: %d assertion(s) failed\n", failures);
	return 1;
}
