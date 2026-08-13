// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file yaml_minimal_scalar.c
 * @brief YAML 1.1 parser - scalar type conversion.
 *
 * Implements YAML scalar node type conversion: string, integer, float
 * and boolean (including YAML 1.1 boolean synonyms), single
 * responsibility.
 */

#include "yaml_minimal.h"

#include "yaml_minimal_internal.h"

#include "airy_memory.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"

const char *yaml_as_string(struct yaml_node *node, const char *default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    return node->scalar.value;
}

long long yaml_as_int64(struct yaml_node *node, long long default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    char *endp = NULL;
    long long v = strtoll(node->scalar.value, &endp, 0);
    return (endp && *endp == '\0') ? v : default_val;
}

double yaml_as_double(struct yaml_node *node, double default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    char *endp = NULL;
    double v = strtod(node->scalar.value, &endp);
    return (endp && *endp == '\0') ? v : default_val;
}

bool yaml_as_bool(struct yaml_node *node, bool default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    const char *s = node->scalar.value;
    if (strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "on") == 0 ||
        strcmp(s, "1") == 0)
        return true;
    if (strcmp(s, "false") == 0 || strcmp(s, "no") == 0 || strcmp(s, "off") == 0 ||
        strcmp(s, "0") == 0 || strcmp(s, "null") == 0 || strcmp(s, "~") == 0)
        return false;
    return default_val;
}
