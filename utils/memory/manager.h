/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file manager.h
 * @brief Backward compatibility layer - legacy manager.h API.
 *
 * The original agentrt/manager/ directory has moved to
 * ecosystem/manager/. This file provides backward-compatible types and
 * function declarations for C test code.
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
 * @brief Compatible config handle type (opaque pointer).
 */
typedef struct airy_config_s airy_config_t;

/**
 * @brief Backward compatibility: load a config file.
 *
 * @param path Config file path
 * @return Config handle, NULL on failure
 */
airy_config_t *airy_config_load(const char *path);

/**
 * @brief Backward compatibility: release config resources.
 *
 * @param config Config handle
 */
void airy_config_free(airy_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_COMMONS_UTILS_MANAGER_H */