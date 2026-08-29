/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC shared-memory API: create/destroy/map/unmap/sync.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_SHM_API_H
#define AIRY_RT_IPC_SHM_API_H

#include "ipc_types.h"

/**
 * @brief Shared memory handle
 */
typedef struct ipc_shm ipc_shm_t;

/**
 * @brief Shared memory configuration
 */
typedef struct {
    const char *name;
    size_t size;
    bool read_only;
    bool create;
    bool exclusive;
    const char *permissions;
} ipc_shm_config_t;

/**
 * @brief Create shared memory
 * @param config Shared memory configuration
 * @return Shared memory handle, NULL on failure
 */
ipc_shm_t *ipc_shm_create(const ipc_shm_config_t *config);

/**
 * @brief Destroy shared memory
 * @param shm Shared memory handle
 */
void ipc_shm_destroy(ipc_shm_t *shm);

/**
 * @brief Map shared memory into the process address space
 * @param shm Shared memory handle
 * @return Mapped address, NULL on failure
 */
void *ipc_shm_map(ipc_shm_t *shm);

/**
 * @brief Unmap shared memory
 * @param shm Shared memory handle
 * @return Error code
 */
airy_err_t ipc_shm_unmap(ipc_shm_t *shm);

/**
 * @brief Get the shared memory size
 * @param shm Shared memory handle
 * @return Shared memory size
 */
size_t ipc_shm_get_size(const ipc_shm_t *shm);

/**
 * @brief Synchronize shared memory
 * @param shm Shared memory handle
 * @return Error code
 */
airy_err_t ipc_shm_sync(ipc_shm_t *shm);

#endif /* AIRY_RT_IPC_SHM_API_H */
