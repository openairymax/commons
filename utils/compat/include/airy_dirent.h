/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

#ifndef AIRY_RT_COMPAT_DIRENT_H
#define AIRY_RT_COMPAT_DIRENT_H

#ifdef _WIN32

/* compat 为最底层域：使用标准库内存/字符串原语，不依赖上层 memory 域
 * （0.1.6 P1-2 依赖图去环：airy_dirent.h 曾 include airy_memory.h，
 * 形成 compat → memory → logging → compat 头文件环）。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define AIRY_MAX_PATH 260

struct dirent {
    char d_name[AIRY_MAX_PATH];
};

typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
    struct dirent ent;
    int first;
} DIR;

static inline DIR *opendir(const char *name)
{
    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir)
        return NULL;

    char pattern[AIRY_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", name);

    dir->hFind = FindFirstFileA(pattern, &dir->ffd);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static inline struct dirent *readdir(DIR *dir)
{
    if (!dir)
        return NULL;
    if (!dir->first) {
        if (!FindNextFileA(dir->hFind, &dir->ffd))
            return NULL;
    }
    dir->first = 0;
    snprintf(dir->ent.d_name, AIRY_MAX_PATH, "%s", dir->ffd.cFileName);
    dir->ent.d_name[AIRY_MAX_PATH - 1] = '\0';
    return &dir->ent;
}


static inline int closedir(DIR *dir)
{
    if (!dir)
        return -1; /* BAN-073 exempt: POSIX API contract */
    FindClose(dir->hFind);
    free(dir);
    return 0;
}

#else

#include <dirent.h>

#endif

#endif