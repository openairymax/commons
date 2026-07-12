/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/* SPDX-Copyright: Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. */
/*
 * uapi_compat.h — Cross-platform UAPI types compatibility layer
 *
 * This file provides __u8/__u16/__u32/__u64/__s32/__s64 types on platforms
 * where <linux/types.h> is unavailable (macOS/Windows). On Linux, it includes
 * <linux/types.h> directly.
 *
 * This is an agentrt user-space helper file, NOT one of the 6 [SC] core
 * shared contract layer headers. It exists solely to allow the [SC] headers
 * (which use kernel UAPI types) to compile on non-Linux platforms.
 */

#ifndef _AIRY_UAPI_COMPAT_H
#define _AIRY_UAPI_COMPAT_H

#ifdef __linux__
#include <linux/types.h>
#elif defined(__APPLE__) || defined(_WIN32) || defined(_WIN64)
#include <stdint.h>
#ifndef __u8
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t  __s32;
typedef int64_t  __s64;
#endif /* __u8 */
#else
/* Fallback: assume POSIX with stdint.h */
#include <stdint.h>
#ifndef __u8
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t  __s32;
typedef int64_t  __s64;
#endif /* __u8 */
#endif /* __linux__ */

#endif /* _AIRY_UAPI_COMPAT_H */
