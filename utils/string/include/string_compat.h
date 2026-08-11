/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef STRING_COMPAT_H
#define STRING_COMPAT_H


#ifdef _WIN32
#include <stdint.h>
#include <stdio.h>
#include <string.h>


typedef intptr_t ssize_t;


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