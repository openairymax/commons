/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * C99 stdbool.h compatibility header
 *
 * Provides <stdbool.h> for compilers that lack it.
 *
 * Original location: agentrt/include/agentrt/compat/stdbool.h
 * Moved to: agentrt/commons/platform/compat/ (2026-04-19 include refactor)
 */

#ifndef AIRY_RT_COMPAT_STDBOOL_H
#define AIRY_RT_COMPAT_STDBOOL_H

#ifndef __cplusplus

/* C99 compatible bool type */
#ifndef __STDC_VERSION__
typedef unsigned char _Bool;
#endif

#ifndef bool
#define bool _Bool
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#else /* __cplusplus */
/* In C++, bool is a built-in type */
#ifndef bool
#define bool bool
#endif

#endif /* __cplusplus */
#endif /* AIRY_RT_COMPAT_STDBOOL_H */
