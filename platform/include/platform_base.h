/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file platform_base.h
 * @brief Cross-platform compatibility layer - base detection & core types
 *
 * Platform detection macros, compiler attribute macros, OS system header
 * inclusion and core handle typedefs. Domain split of platform.h
 * (2026-08-27); the aggregate platform.h includes this header first.
 *
 * @see platform.h aggregate entry
 */

#ifndef AIRY_RT_PLATFORM_BASE_H
#define AIRY_RT_PLATFORM_BASE_H

#include <compat.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#if defined(_WIN32) || defined(_WIN64)
#define AIRY_PLATFORM_WINDOWS 1
#define AIRY_PLATFORM_NAME "Windows"
#if defined(_WIN64)
#define AIRY_PLATFORM_BITS 64
#else
#define AIRY_PLATFORM_BITS 32
#endif
#define AIRY_PLATFORM_POSIX 0
#elif defined(__APPLE__) && defined(__MACH__)
#define AIRY_PLATFORM_MACOS 1
#define AIRY_PLATFORM_NAME "macOS"
#define AIRY_PLATFORM_BITS 64
#define AIRY_PLATFORM_POSIX 1
#elif defined(__linux__)
#define AIRY_PLATFORM_LINUX 1
#define AIRY_PLATFORM_NAME "Linux"
/* 位宽判定用标准宏而非架构枚举：UINTPTR_MAX==UINT64_MAX 覆盖
 * x86_64/aarch64/riscv64/ppc64 等全部 64 位架构（此前 riscv64 未列入
 * x86_64/aarch64 枚举，被判为 32 位，导致指针/原子/地址类型错配）。 */
#if UINTPTR_MAX == UINT64_MAX
#define AIRY_PLATFORM_BITS 64
#else
#define AIRY_PLATFORM_BITS 32
#endif
#define AIRY_PLATFORM_POSIX 1
#else
#error "Unsupported platform"
#endif


#include "export.h"


/* ==================== CPU architecture detection ==================== */

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#define AIRY_ARCH_X86_64 1
#define AIRY_ARCH_NAME "x86_64"
#define AIRY_ARCH_LE 1
#elif defined(__i386__) || defined(_M_IX86)
#define AIRY_ARCH_X86 1
#define AIRY_ARCH_NAME "x86"
#define AIRY_ARCH_LE 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define AIRY_ARCH_ARM64 1
#define AIRY_ARCH_NAME "arm64"
#if defined(__AARCH64EB__)
#define AIRY_ARCH_LE 0
#else
#define AIRY_ARCH_LE 1
#endif
#elif defined(__arm__) || defined(_M_ARM)
#define AIRY_ARCH_ARM 1
#define AIRY_ARCH_NAME "arm"
#if defined(__ARMEB__) || defined(__BIG_ENDIAN__)
#define AIRY_ARCH_LE 0
#else
#define AIRY_ARCH_LE 1
#endif
#elif defined(__riscv)
#define AIRY_ARCH_RISCV 1
#define AIRY_ARCH_NAME "riscv"
/* RISC-V 默认小端；大端（__riscv_bi_endian 或显式宏）极罕见，暂不细分 */
#define AIRY_ARCH_LE 1
#elif defined(__loongarch64)
#define AIRY_ARCH_LOONGARCH 1
#define AIRY_ARCH_NAME "loongarch64"
#define AIRY_ARCH_LE 1
#elif defined(__powerpc64__)
#define AIRY_ARCH_PPC64 1
#define AIRY_ARCH_NAME "ppc64"
#if defined(__BIG_ENDIAN__)
#define AIRY_ARCH_LE 0
#else
#define AIRY_ARCH_LE 1
#endif
#else
#define AIRY_ARCH_UNKNOWN 1
#define AIRY_ARCH_NAME "unknown"
#define AIRY_ARCH_LE 1
#endif


#if defined(_WIN32) || defined(_WIN64)
#define AIRY_THREAD_LOCAL __declspec(thread)
#else
#define AIRY_THREAD_LOCAL __thread
#endif


#if defined(_WIN32) || defined(_WIN64)
#ifndef AIRY_INLINE
#define AIRY_INLINE __forceinline
#endif
#else
#define AIRY_INLINE static inline __attribute__((always_inline))
#endif


#ifndef AIRY_UNUSED
#define AIRY_UNUSED(x) ((void)(x))
#endif


/* ==================== OS system headers ==================== */

#if AIRY_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h> /* time_t / struct tm（airy_localtime_r） */
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#endif


/* ==================== Core handle typedefs ==================== */

#if AIRY_PLATFORM_WINDOWS
typedef HANDLE airy_thread_t;
typedef DWORD airy_thread_id_t;
typedef CRITICAL_SECTION airy_mtx_t;
typedef CONDITION_VARIABLE airy_cond_t;
typedef DWORD airy_pid_t;
typedef SOCKET airy_sock_t;
typedef HANDLE airy_process_t;
#else
typedef pthread_t airy_thread_t;
typedef pthread_t airy_thread_id_t;
typedef pthread_mutex_t airy_mtx_t;
typedef pthread_cond_t airy_cond_t;
typedef pid_t airy_pid_t;
typedef int airy_sock_t;
typedef pid_t airy_process_t;
#endif

#define AIRY_INVALID_THREAD ((airy_thread_t)0)
#define AIRY_INVALID_MUTEX ((airy_mtx_t){0})
#define AIRY_INVALID_SOCKET (-1)
#define AIRY_INVALID_PROCESS ((airy_process_t)0)


#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_BASE_H */
