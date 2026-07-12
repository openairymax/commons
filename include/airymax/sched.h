/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * sched.h — [SC] Shared Contract Layer: scheduling types
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.6
 * IRON-9 v2 [SC] layer — byte-identical shared between agentrt and agentrt-linux.
 *
 * DO NOT redefine these types in agentrt or agentrt-linux source.
 * Reference via: #include <airymax/sched.h>
 */

#ifndef _AIRY_SCHED_H
#define _AIRY_SCHED_H

#include <airymax/uapi_compat.h>

/*
 * SCHED_EXT=7 (Linux 6.6 kernel baseline, include/uapi/linux/sched.h:121).
 * Do NOT define SCHED_AGENT as a scheduling class number;
 * reuse SCHED_EXT=7 scheduling class instead.
 */

/* Task descriptor magic: 0x41475453 = 'AGTS' (Agent Task) */
#define AIRY_TASK_MAGIC	0x41475453u

/* vtime type: Q16.16 fixed-point (no FPU) */
typedef __s32 airy_vtime_t;

/* Priority range: 0 (highest) - 139 (lowest), compatible with Linux */
#define AIRY_PRIO_MIN	0
#define AIRY_PRIO_MAX	139

/* Default scheduling slice: 20ms */
#define AIRY_SLICE_DFL_MS	20

/* Weight range: compatible with Linux sched_ext weight model */
#define AIRY_WEIGHT_MIN	1
#define AIRY_WEIGHT_MAX	10000

/*
 * MAC_MAX_AGENTS: hard limit for concurrent agent scheduling.
 * SSoT value: 1024 (validated by benchmark to 1000 concurrent).
 * All agentrt modules MUST reference this constant instead of
 * defining local MAX_AGENTS values.
 */
#define MAC_MAX_AGENTS	1024

/**
 * airy_vtime_decay - Compute vtime after slice consumption
 * @vtime: Current virtual time (Q16.16 fixed-point).
 * @consumed_slice: Slice consumed in nanoseconds.
 * @weight: Task weight in [AIRY_WEIGHT_MIN, AIRY_WEIGHT_MAX].
 *
 * Return: New vtime after decay.
 */
static inline airy_vtime_t
airy_vtime_decay(airy_vtime_t vtime, __u64 consumed_slice, __u32 weight)
{
	return vtime + (airy_vtime_t)(consumed_slice * 100 / weight);
}

/**
 * struct airy_task_desc - Agent task descriptor ([SC] shared)
 * @magic: Must be AIRY_TASK_MAGIC (0x41475453 'AGTS').
 * @prio: Priority in [AIRY_PRIO_MIN, AIRY_PRIO_MAX].
 * @vtime: Virtual time for fair scheduling (Q16.16).
 *
 * Shared between agentrt (user-space) and agentrt-linux (kernel).
 * The magic field validates descriptor integrity across the boundary.
 */
struct airy_task_desc {
	__u32		magic;		/* 0x41475453 'AGTS' */
	__u16		prio;		/* 0-139 */
	__u16		_pad;
	airy_vtime_t	vtime;		/* Q16.16 fixed-point */
};

#endif /* _AIRY_SCHED_H */
