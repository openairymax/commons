/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef STRING_COMPAT_H
#define STRING_COMPAT_H


#ifdef _WIN32
#include <stdint.h>
#include <stdio.h>
#include <string.h>


/* cmake/windows_preinclude.h 已经 SSIZE_T 定义 ssize_t 并置位
 * _SSIZE_T_DEFINED；此处重复 typedef 在 MSVC 触发 C4142（benign
 * redefinition），x86-32 leg 被 /WX 升为 C2220（probe-3 实证）。 */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif


/* flawfinder: ignore - Windows compat macro, format is always const */
#ifndef snprintf
#define snprintf _snprintf
#endif

#else
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#endif

#endif /* STRING_COMPAT_H */