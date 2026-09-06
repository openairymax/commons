/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file export.h
 * @brief AgentRT symbol export management (commons platform layer copy).
 *
 * @note Defines cross-platform symbol export macros for Windows and POSIX.
 * This is one of the authoritative definition sources of AIRY_API.
 */

#ifndef AIRY_RT_EXPORT_H
#define AIRY_RT_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#ifdef AIRY_BUILDING_DLL
#ifndef AIRY_API
#define AIRY_API __declspec(dllexport)
#endif
#else
/* Static linkage: AIRY_API must still be *defined* as an empty macro —
 * leaving it undefined exposes the bare identifier at every use site,
 * which MSVC reports as a C2054/C2061 cascade (build-test #110;
 * uapi_compat.h __s32 errors are recovery noise of the same TU).
 * dllimport here would break static consumers at their definition
 * points instead (C2491/C4273, build-test #109). Static builds need
 * no export decoration. */
#ifndef AIRY_API
#define AIRY_API
#endif
#endif
#elif defined(__GNUC__) || defined(__clang__)
#ifndef AIRY_API
#define AIRY_API __attribute__((visibility("default")))
#endif
#else
#ifndef AIRY_API
#define AIRY_API
#endif
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define AIRY_INTERNAL
#elif defined(__GNUC__) || defined(__clang__)
#define AIRY_INTERNAL __attribute__((visibility("hidden")))
#else
#define AIRY_INTERNAL
#endif

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_EXPORT_H */
