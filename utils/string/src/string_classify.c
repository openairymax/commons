// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_classify.c
 * @brief 字符分类域：数字/字母/字母数字内容判断
 */

#include "airy_string.h"
#include "string_internal.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "error.h"

#ifdef _WIN32
#include <locale.h>
#include <windows.h>
#else
#include <locale.h>
#include <strings.h>
#endif

bool string_is_digit(const char *str)
{
    if (str == NULL || *str == '\0') {
        return false;
    }

    while (*str != '\0') {
        if (!isdigit((unsigned char)*str)) {
            return false;
        }
        str++;
    }

    return true;
}

bool string_is_alpha(const char *str)
{
    if (str == NULL || *str == '\0') {
        return false;
    }

    while (*str != '\0') {
        if (!isalpha((unsigned char)*str)) {
            return false;
        }
        str++;
    }

    return true;
}

bool string_is_alnum(const char *str)
{
    if (str == NULL || *str == '\0') {
        return false;
    }

    while (*str != '\0') {
        if (!isalnum((unsigned char)*str)) {
            return false;
        }
        str++;
    }

    return true;
}
