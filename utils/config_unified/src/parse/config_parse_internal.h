// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_parse_internal.h
 * @brief Unified config module - format parsing layer internal sharing.
 *
 * 2026-08-27 域拆分：config_parse.c（994 行）按格式拆为
 * config_parse_json.c / config_parse_ini.c / config_parse_yaml.c，
 * 本头承载三个翻译单元共享的 include 集合，保证行为与原单体一致。
 */

#ifndef CONFIG_PARSE_INTERNAL_H
#define CONFIG_PARSE_INTERNAL_H

#include "config_source.h"
#include "core_config.h"
#include "logging_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"

#endif /* CONFIG_PARSE_INTERNAL_H */
