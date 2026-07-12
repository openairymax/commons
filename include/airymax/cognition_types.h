/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * cognition_types.h — [SC] Shared Contract Layer: cognition model types
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.5
 * IRON-9 v2 [SC] layer — byte-identical shared between agentrt and agentrt-linux.
 *
 * DO NOT redefine these types in agentrt or agentrt-linux source.
 * Reference via: #include <airymax/cognition_types.h>
 */

#ifndef _AIRY_COGNITION_TYPES_H
#define _AIRY_COGNITION_TYPES_H

#include <airymax/uapi_compat.h>

/*
 * Q16.16 fixed-point: required because -mno-80387 (no FPU) in kernel.
 * Uses __s32 (UAPI type) for cross-platform consistency.
 */
typedef __s32 airy_q16_t;
#define AIRY_Q16_ONE		(1 << 16)

/* CoreLoopThree phases */
enum airy_cog_phase {
	AIRY_COG_PERCEPT	= 0,
	AIRY_COG_THINK	= 1,
	AIRY_COG_ACT		= 2,
};

/* Thinkdual modes */
enum airy_think_mode {
	AIRY_THINK_FAST	= 0,	/* System-1: fast, intuitive */
	AIRY_THINK_SLOW	= 1,	/* System-2: slow, deliberative */
};

#endif /* _AIRY_COGNITION_TYPES_H */
