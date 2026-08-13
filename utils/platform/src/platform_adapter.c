// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_adapter.c
 * @brief Platform adapter - implementation.
 *
 * Implements the cross-platform abstraction layer, eliminating
 * platform-specific code duplication.
 */

/* _POSIX_C_SOURCE: defined via CMakeLists.txt target_compile_definitions (BAN-182) */
/* _XOPEN_SOURCE: defined via CMakeLists.txt target_compile_definitions (BAN-182) */
/* _GNU_SOURCE: defined via CMakeLists.txt target_compile_definitions (BAN-182) */
#include <time.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#define PLATFORM_SLASH '\\'
#else
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#define PLATFORM_SLASH '/'
#endif

#include "platform_adapter.h"

#include "platform.h"

#include <string.h>
#include "airy_memory.h"
#include "error.h"

platform_type_t platform_get_type(void)
{
#if defined(_WIN32)
    return PLATFORM_WINDOWS;
#elif defined(__linux__)
    return PLATFORM_LINUX;
#elif defined(__APPLE__)
    return PLATFORM_MACOS;
#elif defined(__unix__)
    return PLATFORM_UNIX;
#else
    return PLATFORM_UNKNOWN;
#endif
}

const char *platform_get_name(void)
{
    switch (platform_get_type()) {
    case PLATFORM_WINDOWS:
        return "Windows";
    case PLATFORM_LINUX:
        return "Linux";
    case PLATFORM_MACOS:
        return "macOS";
    case PLATFORM_UNIX:
        return "Unix";
    default:
        return "Unknown";
    }
}

/* platform_exec() / platform_free_exec_result() removed (BAN-211/235
 * security compliance). They executed command strings via /bin/sh -c,
 * creating a command-injection risk, and had zero callers in the repo.
 * Use airy_process_run_capture() (fork+execvp, no shell) as the canonical
 * subprocess API instead. See the airy_process_run_capture declaration
 * in platform.h. */

platform_file_info_t platform_get_file_info(const char *path)
{
    platform_file_info_t info = {
        .path = path, .size = 0, .mtime = 0, .is_directory = false, .exists = false};

    if (!path) {
        return info;
    }

#if defined(_WIN32)
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        info.exists = true;
        info.is_directory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!info.is_directory) {
            info.size = ((uint64_t)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            info.mtime = (time_t)findData.ftLastWriteTime.dwLowDateTime;
        }
        FindClose(hFind);
    }
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        info.exists = true;
        info.is_directory = S_ISDIR(st.st_mode);
        if (!info.is_directory) {
            info.size = st.st_size;
            info.mtime = st.st_mtime;
        }
    }
#endif

    return info;
}

bool platform_mkdir(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (_mkdir(path) == 0);
#else
    return (mkdir(path, 0755) == 0);
#endif
}

bool platform_mkdir_recursive(const char *path)
{
    if (!path) {
        return false;
    }

    char *copy = (char *)AIRY_MALLOC(strlen(path) + 1);
    if (!copy) {
        return false;
    }

    __builtin_memcpy(copy, path, strlen(path) + 1);
    char *p = copy;

    while (*p) {
        if (*p == PLATFORM_SLASH) {
            *p = '\0';
            if (*copy && !platform_path_exists(copy)) {
                if (!platform_mkdir(copy)) {
                    AIRY_FREE(copy);
                    return false;
                }
            }
            *p = PLATFORM_SLASH;
        }
        p++;
    }

    if (*copy && !platform_path_exists(copy)) {
        if (!platform_mkdir(copy)) {
            AIRY_FREE(copy);
            return false;
        }
    }

    AIRY_FREE(copy);
    return true;
}

bool platform_unlink(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (DeleteFileA(path) != 0);
#else
    return (unlink(path) == 0);
#endif
}

bool platform_rmdir(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (RemoveDirectoryA(path) != 0);
#else
    return (rmdir(path) == 0);
#endif
}

bool platform_copy_file(const char *src, const char *dest)
{
    if (!src || !dest) {
        return false;
    }

#if defined(_WIN32)
    return (CopyFileA(src, dest, FALSE) != 0);
#else
    FILE *srcFile = fopen(src, "rb");
    if (!srcFile) {
        return false;
    }

    FILE *destFile = fopen(dest, "wb");
    if (!destFile) {
        fclose(srcFile);
        return false;
    }

    char buffer[4096];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
        if (fwrite(buffer, 1, bytesRead, destFile) != bytesRead) {
            fclose(srcFile);
            fclose(destFile);
            return false;
        }
    }

    fclose(srcFile);
    fclose(destFile);
    return true;
#endif
}

bool platform_move_file(const char *src, const char *dest)
{
    if (!src || !dest) {
        return false;
    }

#if defined(_WIN32)
    return (MoveFileA(src, dest) != 0);
#else
    return (rename(src, dest) == 0);
#endif
}

char *platform_get_env(const char *name, const char *default_value)
{
    if (!name) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

#if defined(_WIN32)
    char buffer[4096];
    DWORD size = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
    if (size == 0) {
        if (default_value) {
            return AIRY_STRDUP(default_value);
        }
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    char *value = (char *)AIRY_MALLOC(size + 1);
    if (value) {
        GetEnvironmentVariableA(name, value, size + 1);
    }
    return value;
#else
    const char *value = getenv(name);
    if (value) {
        return AIRY_STRDUP(value);
    }
    if (default_value) {
        return AIRY_STRDUP(default_value);
    }
    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
#endif
}

bool platform_set_env(const char *name, const char *value)
{
    if (!name) {
        return false;
    }

#if defined(_WIN32)
    return (SetEnvironmentVariableA(name, value) != 0);
#else
    return (setenv(name, value, 1) == 0);
#endif
}

char *platform_get_cwd(void)
{
#if defined(_WIN32)
    char buffer[4096];
    if (_getcwd(buffer, sizeof(buffer)) != NULL) {
        return AIRY_STRDUP(buffer);
    }
#else
    char *buffer = getcwd(NULL, 0);
    if (buffer) {
        char *copy = AIRY_STRDUP(buffer);
        AIRY_FREE(buffer);
        return copy;
    }
#endif
    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
}

bool platform_chdir(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (_chdir(path) == 0);
#else
    return (chdir(path) == 0);
#endif
}

char *platform_get_temp_dir(void)
{
#if defined(_WIN32)
    char buffer[4096];
    if (GetTempPathA(sizeof(buffer), buffer) > 0) {
        return AIRY_STRDUP(buffer);
    }
#else
    const char *temp = getenv("TMPDIR");
    if (temp) {
        return AIRY_STRDUP(temp);
    }
    return AIRY_STRDUP("/tmp");
#endif
    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
}

char *platform_get_temp_file(const char *prefix)
{
    char *temp_dir = platform_get_temp_dir();
    if (!temp_dir) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    char *path = NULL;
    const char *base = prefix ? prefix : "agentrt";

#if defined(_WIN32)
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "%s\\%s_XXXXXX", temp_dir, base);
    if (GetTempFileNameA(temp_dir, base, 0, buffer) != 0) {
        path = AIRY_STRDUP(buffer);
    }
#else
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "%s/%s_XXXXXX", temp_dir, base);
    int fd = mkstemp(buffer);
    if (fd != -1) {
        close(fd);
        path = AIRY_STRDUP(buffer);
    }
#endif

    AIRY_FREE(temp_dir);
    return path;
}

char *platform_path_join(const char *path1, const char *path2)
{
    if (!path1 || !path2) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    bool needs_slash = (len1 > 0 && path1[len1 - 1] != PLATFORM_SLASH);
    size_t total_len = len1 + len2 + (needs_slash ? 1 : 0) + 1;

    char *result = (char *)AIRY_MALLOC(total_len);
    if (!result) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (needs_slash) {
        snprintf(result, total_len, "%s%c%s", path1, PLATFORM_SLASH, path2);
    } else {
        snprintf(result, total_len, "%s%s", path1, path2);
    }
    return result;
}

char *platform_path_normalize(const char *path)
{
    if (!path) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    // Simple implementation - real implementation would handle .. and .
    return AIRY_STRDUP(path);
}

char *platform_path_basename(const char *path)
{
    if (!path) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    const char *last_slash = strrchr(path, PLATFORM_SLASH);
    if (last_slash) {
        return AIRY_STRDUP(last_slash + 1);
    }
    return AIRY_STRDUP(path);
}

char *platform_path_dirname(const char *path)
{
    if (!path) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    const char *last_slash = strrchr(path, PLATFORM_SLASH);
    if (!last_slash) {
        return AIRY_STRDUP(".");
    }

    size_t len = last_slash - path;
    char *result = (char *)AIRY_MALLOC(len + 1);
    if (result) {
        AIRY_STRNCPY_TERM(result, path, len);
        result[len] = '\0';
    }
    return result;
}

bool platform_path_exists(const char *path)
{
    if (!path) {
        return false;
    }

    platform_file_info_t info = platform_get_file_info(path);
    return info.exists;
}

bool platform_path_is_directory(const char *path)
{
    if (!path) {
        return false;
    }

    platform_file_info_t info = platform_get_file_info(path);
    return info.exists && info.is_directory;
}

bool platform_path_is_file(const char *path)
{
    if (!path) {
        return false;
    }

    platform_file_info_t info = platform_get_file_info(path);
    return info.exists && !info.is_directory;
}

uint64_t platform_get_timestamp_ms(void)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10000; /* Convert 100-nanosecond intervals to milliseconds */
#else
    return airy_time_ms();
#endif
}

uint64_t platform_get_timestamp_us(void)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10; /* Convert 100-nanosecond intervals to microseconds */
#else
    return airy_time_ns() / 1000;
#endif
}

void platform_sleep_ms(unsigned int ms)
{
#if defined(_WIN32)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

bool platform_adapter_init(void)
{
#if defined(_WIN32)
    WSADATA wsaData;
    return (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#else
    return true;
#endif
}

void platform_adapter_cleanup(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}