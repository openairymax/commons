// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file ipc_shm.c
 * @brief 进程间通信模块 - 共享内存实现
 *
 * @details
 * 本文件实现了 ipc_common.h 中声明的共享内存 API：
 * - 创建/销毁共享内存对象
 * - 映射/取消映射共享内存到进程地址空间
 * - 获取共享内存大小、同步共享内存
 *
 * 平台差异：
 * - Windows：基于 CreateFileMappingA/MapViewOfFile
 * - Linux/macOS：基于 shm_open/mmap
 *
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的设计原则：
 * - E-4 跨平台一致性：统一 API 语义，屏蔽平台差异
 * - E-6 错误可追溯：统一的错误码体系
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-08-11
 * @version 1.0
 *
 * @see ipc_common_internal.h 内部共享定义
 */

#include "ipc_common_internal.h"

#include "ipc_common.h"

ipc_shm_t *ipc_shm_create(const ipc_shm_config_t *config)
{
    if (!config) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_shm_t *shm = (ipc_shm_t *)AIRY_CALLOC(1, sizeof(ipc_shm_t));
    if (!shm) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    shm->config = *config;
    shm->mapped_addr = NULL;
    shm->actual_size = 0;
    shm->is_mapped = false;

#ifdef _WIN32
    shm->hMapFile = NULL;
#else
    shm->shm_fd = -1;
#endif

    AIRY_MEMSET(shm->error_msg, 0, sizeof(shm->error_msg));

    return shm;
}

void ipc_shm_destroy(ipc_shm_t *shm)
{
    if (!shm) {
        return;
    }

    if (shm->is_mapped) {
        ipc_shm_unmap(shm);
    }

#ifdef _WIN32
    if (shm->hMapFile != NULL) {
        CloseHandle(shm->hMapFile);
    }
#else
    if (shm->shm_fd >= 0) {
        close(shm->shm_fd);
        if (shm->config.create) {
            shm_unlink(shm->config.name);
        }
    }
#endif

    AIRY_FREE(shm);
}

void *ipc_shm_map(ipc_shm_t *shm)
{
    if (!shm) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (shm->is_mapped) {
        return shm->mapped_addr;
    }

#ifdef _WIN32
    shm->hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                       shm->config.read_only ? PAGE_READONLY : PAGE_READWRITE,
                                       (DWORD)(shm->config.size >> 32),
                                       (DWORD)(shm->config.size & 0xFFFFFFFF), shm->config.name);

    if (shm->hMapFile == NULL) {
        snprintf(shm->error_msg, sizeof(shm->error_msg), "CreateFileMapping failed");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    shm->mapped_addr =
        MapViewOfFile(shm->hMapFile, shm->config.read_only ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0,
                      0, shm->config.size);
#else
    int flags = O_CREAT | (shm->config.read_only ? O_RDONLY : O_RDWR);
    mode_t mode = 0666;

    shm->shm_fd = shm_open(shm->config.name, flags, mode);
    if (shm->shm_fd < 0) {
        snprintf(shm->error_msg, sizeof(shm->error_msg), "shm_open failed");
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    if (shm->config.create) {
        if (ftruncate(shm->shm_fd, (off_t)shm->config.size) != 0) {
            snprintf(shm->error_msg, sizeof(shm->error_msg), "ftruncate failed for size %zu",
                     shm->config.size);
            AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
        }
    }

    shm->mapped_addr =
        mmap(NULL, shm->config.size, shm->config.read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
             MAP_SHARED, shm->shm_fd, 0);
#endif

    if (shm->mapped_addr == MAP_FAILED || shm->mapped_addr == NULL) {
        snprintf(shm->error_msg, sizeof(shm->error_msg), "Memory mapping failed");
        shm->mapped_addr = NULL;
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    shm->is_mapped = true;
    shm->actual_size = shm->config.size;

    return shm->mapped_addr;
}

airy_err_t ipc_shm_unmap(ipc_shm_t *shm)
{
    if (!shm) {
        return AIRY_EINVAL;
    }

    if (!shm->is_mapped) {
        return AIRY_SUCCESS;
    }

#ifdef _WIN32
    UnmapViewOfFile(shm->mapped_addr);
#else
    munmap(shm->mapped_addr, shm->actual_size);
#endif

    shm->mapped_addr = NULL;
    shm->is_mapped = false;

    return AIRY_SUCCESS;
}

size_t ipc_shm_get_size(const ipc_shm_t *shm)
{
    if (!shm) {
        return 0;
    }
    return shm->actual_size;
}

airy_err_t ipc_shm_sync(ipc_shm_t *shm)
{
    if (!shm) {
        return AIRY_EINVAL;
    }

#ifdef _WIN32
    FlushViewOfFile(shm->mapped_addr, shm->actual_size);
#else
    msync(shm->mapped_addr, shm->actual_size, MS_SYNC);
#endif

    return AIRY_SUCCESS;
}
