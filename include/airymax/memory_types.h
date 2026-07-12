/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * memory_types.h — [SC] Shared Contract Layer: MemoryRovol memory types
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.3
 * IRON-9 v2 [SC] layer — byte-identical shared between agentrt and agentrt-linux.
 *
 * DO NOT redefine these types in agentrt or agentrt-linux source.
 * Reference via: #include <airymax/memory_types.h>
 */

#ifndef _AIRY_MEMORY_TYPES_H
#define _AIRY_MEMORY_TYPES_H

#include <airymax/uapi_compat.h>

/* MemoryRovol L1-L4 hierarchy (shared semantics) */
enum airy_mem_level {
	AIRY_MEM_L1_HOT	= 0,	/* Hot working set, agent-local */
	AIRY_MEM_L2_WARM	= 1,	/* Warm set, node-local */
	AIRY_MEM_L3_COLD	= 2,	/* Cold set, node-remote */
	AIRY_MEM_L4_PMEM	= 3,	/* Persistent memory */
};

/*
 * GFP mask semantics for memory tier selection.
 *
 * These are platform-independent semantic tier selectors, NOT kernel
 * __GFP_* flags. Each side (agentrt user-space / agentrt-linux kernel)
 * translates AIRY_GFP_* to its own allocation mechanism internally.
 * Using independent bitmask constants ensures the [SC] header compiles
 * byte-identically on Linux/macOS/Windows (kernel GFP flags are
 * kernel-internal and unavailable in user-space).
 */
#define AIRY_GFP_HOT		(0x01u)	/* Hot: agent-local working set */
#define AIRY_GFP_WARM		(0x02u)	/* Warm: node-local */
#define AIRY_GFP_COLD		(0x04u)	/* Cold: node-remote */
#define AIRY_GFP_PMEM		(0x08u)	/* Persistent memory */

#endif /* _AIRY_MEMORY_TYPES_H */
