/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * security_types.h — [SC] Shared Contract Layer: security model types
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.4
 * IRON-9 v2 [SC] layer — byte-identical shared between agentrt and agentrt-linux.
 *
 * DO NOT redefine these types in agentrt or agentrt-linux source.
 * Reference via: #include <airymax/security_types.h>
 */

#ifndef _AIRY_SECURITY_TYPES_H
#define _AIRY_SECURITY_TYPES_H

#include <airymax/uapi_compat.h>

/*
 * POSIX capability IDs.
 * Linux 6.6 defines 41 standard capabilities (0-40).
 * Airymax-specific capabilities start at 41 to avoid conflicts.
 */
#define AIRY_CAP_AGENT_SPAWN		41	/* Agent spawn (Airymax-specific) */
#define AIRY_CAP_GPU_SCHED		42	/* GPU scheduling */
#define AIRY_CAP_NPU_ACCESS		43	/* NPU access */

/* LSM hook IDs (252 total, subset shown) */
#define AIRY_LSM_HOOK_TASK_CREATE	0
#define AIRY_LSM_HOOK_IPC_SEND	1

/* Cupolas policy verdict (4 values) */
enum airy_verdict {
	AIRY_VERDICT_ALLOW	= 0,
	AIRY_VERDICT_DENY	= 1,
	AIRY_VERDICT_AUDIT	= 2,
	AIRY_VERDICT_ASK	= 3,
};

/* Capability invocation operations (seL4 CNode model) */
enum airy_cap_op {
	AIRY_CAP_OP_COPY	= 0,
	AIRY_CAP_OP_MINT	= 1,
	AIRY_CAP_OP_MOVE	= 2,
	AIRY_CAP_OP_MUTATE	= 3,
	AIRY_CAP_OP_REVOKE	= 4,
	AIRY_CAP_OP_DELETE	= 5,
	AIRY_CAP_OP_ROTATE	= 6,
};

#endif /* _AIRY_SECURITY_TYPES_H */
