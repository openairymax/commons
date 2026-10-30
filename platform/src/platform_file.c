// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_file.c
 * @brief 文件级原语：删除与原子替换改名。
 *
 * 从 platform.c 拆出——这两个函数是全树「落盘替换」的唯一入口（持久化、
 * 检查点、日志轮转、清单改写均转调此处），与网络/原子/随机等杂项同处一
 * 文件时难以辨认为独立能力格，故单独成件。
 */

#include "platform_internal.h"

int airy_file_remove(const char *path)
{
    if (!path)
        return AIRY_EINVAL;
#if AIRY_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return -1;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return RemoveDirectoryA(path) ? 0 : -1;
    return DeleteFileA(path) ? 0 : -1;
#else
    return unlink(path) == 0 ? 0 : -1;
#endif
}

/* 原子替换原语（SSoT）：POSIX rename(2) 在目标存在时原子替换，读方只会
 * 看到旧文件或完整新文件。Windows CRT rename 在目标存在时直接失败，故用
 * MoveFileExA REPLACE_EXISTING|WRITE_THROUGH——同卷单次替换，无「新旧两侧
 * 同时缺位」的崩溃窗口，且改名落盘后才返回。Win32 错误码映射回 errno，
 * 调用方得以沿用 POSIX 判据。 */
int airy_file_rename(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path)
        return AIRY_EINVAL;
#if AIRY_PLATFORM_WINDOWS
    if (MoveFileExA(old_path, new_path,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return 0;
    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
        errno = ENOENT;
    else if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION)
        errno = EACCES;
    else
        errno = EIO;
    return -1;
#else
    return rename(old_path, new_path) == 0 ? 0 : -1;
#endif
}
