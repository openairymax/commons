/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * bpf_struct_ops.h — Supplementary shared file (NOT one of the 6 [SC] core headers)
 *
 * SSoT: docs/AirymaxOS/50-engineering-standards/120-cross-project-code-sharing.md §2.2
 *
 * This is a supplementary shared file for SDK gateway state management.
 * It is shared between agentrt (user-space BPF loader) and agentrt-linux
 * (kernel struct_ops framework) but is NOT counted among the 6 [SC] core
 * shared contract layer headers.
 */

#ifndef _AIRY_BPF_STRUCT_OPS_H
#define _AIRY_BPF_STRUCT_OPS_H

#include <airymax/uapi_compat.h>

/**
 * struct airy_struct_ops_value - shared struct_ops state
 * @state: Operation state machine value.
 * @common: Common value shared between BPF and scheduler.
 *
 * Shared between agentrt (user-space BPF loader) and
 * agentrt-linux (kernel struct_ops framework).
 */
struct airy_struct_ops_value {
	__u32	state;
	__u64	common;
};

enum airy_struct_ops_state {
	AIRY_OPS_INIT		= 0,
	AIRY_OPS_REGISTERED	= 1,
	AIRY_OPS_ACTIVE		= 2,
	AIRY_OPS_DRAINING	= 3,
};

#endif /* _AIRY_BPF_STRUCT_OPS_H */
