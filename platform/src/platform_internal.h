// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file platform_internal.h
 * @brief Platform 模块内部共享前置声明（家族 prelude）。
 *
 * 聚合家族内各实现文件共同依赖的标准头、OS 系统头与项目公共头；平台家族
 * 的实现文件只需包含本头，即可获得完整的前置声明环境。OS 系统头按
 * Windows / macOS / POSIX 三支取家族并集，保证跨平台一致性。
 *
 * @note platform_compat.c 因须先包含 atomic_compat.h 且依赖 cjson，不纳入
 *       本 prelude，为家族内的合理例外。
 */

#ifndef AIRY_PLATFORM_INTERNAL_H
#define AIRY_PLATFORM_INTERNAL_H

/* ==================== 标准 C 头 ==================== */
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== OS 系统头（三平台并集） ==================== */
#if defined(_WIN32) || defined(_WIN64)
/* WIN32_LEAN_AND_MEAN 先行定义：避免 windows.h 默认拉入 winsock.h 与
 * platform.h 引入的 winsock2.h 冲突（MSVC C2011 结构体重定义）。 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#ifndef EEXIST
#define EEXIST 17
#endif
#ifndef strdup
#define strdup _strdup
#endif
#ifndef access
#define access _access /* flawfinder: ignore */
#endif
/* MSVC 未提供 S_ISDIR；以 _S_IFMT 宏补齐（对齐 utils/io file_utils.c）。 */
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__) && defined(__MACH__)
#include <errno.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#endif

/* ==================== 项目公共头 ==================== */
#include "error.h"
#include "platform.h"
#include "cancel_token.h"

#include "airy_memory.h"

#endif /* AIRY_PLATFORM_INTERNAL_H */
