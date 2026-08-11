/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file resource_quota.h
 * @brief 资源配额管理接口
 *
 * @details
 * 基于E-3资源确定性原则实现的资源配额管理系统。
 * 提供内存、CPU、I/O、网络等资源的配额限制和统计功能。
 */

#ifndef RESOURCE_QUOTA_H
#define RESOURCE_QUOTA_H

#include "error.h"
#include "airy_memory.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct airy_resource_quota {
    size_t max_memory_bytes;
    uint64_t max_cpu_time_ms;
    size_t max_io_ops;
    size_t max_network_bytes;
    uint64_t timeout_ms;
} airy_resource_quota_t;

typedef struct airy_resource_usage {
    size_t current_memory_bytes;
    size_t peak_usage;
    uint64_t total_cpu_time_ms;
    size_t total_io_ops;
    size_t total_network_bytes;
    time_t start_time;
    time_t last_update;
    uint64_t operation_count;
} airy_resource_usage_t;

typedef struct airy_resource_manager {
    airy_resource_quota_t quota;
    airy_resource_usage_t usage;
    char *resource_id;
    void *lock;
    int enabled;
    uint8_t exceeded_flags;
} airy_resource_manager_t;

airy_err_t airy_resource_manager_create(const airy_resource_quota_t *quota, const char *resource_id,
                                        airy_resource_manager_t **out_manager);

void airy_resource_manager_destroy(airy_resource_manager_t *manager);

airy_err_t airy_resource_check_memory(airy_resource_manager_t *manager, size_t requested_bytes);

airy_err_t airy_resource_record_allocation(airy_resource_manager_t *manager, size_t bytes);

airy_err_t airy_resource_record_free(airy_resource_manager_t *manager, size_t bytes);

airy_err_t airy_resource_record_io(airy_resource_manager_t *manager);

int airy_resource_is_exceeded(airy_resource_manager_t *manager);

void airy_resource_get_usage(airy_resource_manager_t *manager, airy_resource_usage_t *out_usage);

const char *airy_resource_get_exceeded_info(airy_resource_manager_t *manager);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_QUOTA_H */