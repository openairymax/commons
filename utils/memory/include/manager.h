/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file manager.h
 * @brief 向后兼容层 — 旧版 manager.h API 兼容
 *
 * 原 agentrt/manager/ 目录已迁移至 ecosystem/manager/。
 * 本文件为 C 测试代码提供向后兼容的类型和函数声明。
 *
 * @owner team-A
 */

#ifndef AIRY_RT_COMMONS_UTILS_MANAGER_H
#define AIRY_RT_COMMONS_UTILS_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 兼容的配置句柄类型（不透明指针）
 */
typedef struct airy_config_s airy_config_t;

/**
 * @brief 向后兼容：加载配置文件
 *
 * @param path 配置文件路径
 * @return 配置句柄，失败返回 NULL
 */
airy_config_t *airy_config_load(const char *path);

/**
 * @brief 向后兼容：释放配置资源
 *
 * @param config 配置句柄
 */
void airy_config_free(airy_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMONS_UTILS_MANAGER_H */