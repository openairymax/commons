/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * sanitize_level.h - Canonical sanitize level type (single source of truth)
 *
 * 净化级别唯一权威定义。所有模块必须包含本文件，禁止本地重复定义
 * sanitize_level_t，尤其禁止以 #define 宏遮蔽本枚举。
 *
 * 权威来源：docs/AirymaxRT/07-subsystem-specs/04-cupolas.md §4.3
 * 枚举按净化强度递增排列，level 可直接用于数值比较。
 */

#ifndef SANITIZE_LEVEL_H
#define SANITIZE_LEVEL_H

/**
 * @brief Input sanitization strictness level (increasing strength)
 *
 * NONE   - No sanitization: input passed through unchanged
 * LOW    - Basic: escape special characters
 * MEDIUM - Moderate: basic escaping + pattern matching rules
 * HIGH   - Strict: whitelist mode, non-whitelisted input rejected
 * MAX    - Strictest: reject all input
 */
typedef enum sanitize_level {
    SANITIZE_LEVEL_NONE = 0,
    SANITIZE_LEVEL_LOW,
    SANITIZE_LEVEL_MEDIUM,
    SANITIZE_LEVEL_HIGH,
    SANITIZE_LEVEL_MAX
} sanitize_level_t;

#endif /* SANITIZE_LEVEL_H */
