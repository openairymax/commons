// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_source_env.c
 * @brief Unified config module - environment variable source.
 *
 * Implements the environment variable config source: prefix filtering,
 * key mapping (double separators become hierarchy dots), scalar type
 * inference and change detection (hash comparison), single responsibility.
 */

#include "config_source.h"

#include "config_source_internal.h"
#include "logging_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"

static uint64_t compute_env_hash(env_source_priv_t *priv);

static config_error_t env_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    env_source_priv_t *priv = (env_source_priv_t *)source->priv_data;
    if (!priv)
        return CONFIG_ERROR_INVALID_ARG;
    char **env = environ;
    if (!env)
        return CONFIG_SUCCESS;
    size_t prefix_len = priv->prefix ? strlen(priv->prefix) : 0;
    for (size_t idx = 0; env[idx]; idx++) {
        const char *entry = env[idx];
        if (prefix_len > 0 && strncmp(entry, priv->prefix, prefix_len) != 0)
            continue;
        const char *eq = strchr(entry, '=');
        if (!eq)
            continue;
        size_t key_len = (size_t)(eq - entry);
        char key[512];
        const char *val = eq + 1;
        if (prefix_len > 0) {
            size_t offset = prefix_len;
            key_len -= offset;
            if (key_len >= sizeof(key))
                key_len = sizeof(key) - 1;
            __builtin_memcpy(key, entry + offset, key_len);
        } else {
            if (key_len >= sizeof(key))
                key_len = sizeof(key) - 1;
            __builtin_memcpy(key, entry, key_len);
        }
        key[key_len] = '\0';

        /* Key mapping: doubled separators (e.g. __) become hierarchy dots,
         * single separators stay as word boundaries, aligning with YAML
         * dotted paths. */

        if (!priv->case_sensitive) {
            for (char *p = key; *p; p++)
                *p = (char)tolower((unsigned char)*p);
        }

        const char *sep = priv->separator ? priv->separator : "_";
        size_t sep_len = strlen(sep);
        char double_sep[64];
        snprintf(double_sep, sizeof(double_sep), "%s%s", sep, sep);
        size_t dsep_len = sep_len * 2;

        char mapped_key[512];
        size_t mi = 0;
        const char *src = key;
        while (*src && mi < sizeof(mapped_key) - 1) {
            if (dsep_len > 0 && strncmp(src, double_sep, dsep_len) == 0) {
                mapped_key[mi++] = '.';
                src += dsep_len;
            } else {
                mapped_key[mi++] = *src++;
            }
        }
        mapped_key[mi] = '\0';

        config_value_t *cv = NULL;
        char *endptr;
        long int_val = strtol(val, &endptr, 10);
        if (*endptr == '\0' && endptr != val) {
            cv = config_value_create_int((int32_t)int_val);
        } else {

            char lower_val[64];
            size_t vlen = strlen(val);
            if (vlen < sizeof(lower_val)) {
                for (size_t vi = 0; vi < vlen; vi++)
                    lower_val[vi] = (char)tolower((unsigned char)val[vi]);
                lower_val[vlen] = '\0';
                if (strcmp(lower_val, "true") == 0)
                    cv = config_value_create_bool(true);
                else if (strcmp(lower_val, "false") == 0)
                    cv = config_value_create_bool(false);
            }
            if (!cv)
                cv = config_value_create_string(val);
        }
        if (cv)
            config_context_set(ctx, mapped_key, cv);
    }
    priv->env_hash = compute_env_hash(priv);
    return CONFIG_SUCCESS;
}

static config_error_t env_source_save(config_source_t *source, const config_context_t *ctx)
{
    (void)source;
    (void)ctx;
    AIRY_LOG_WARN("环境变量配置源为只读，不支持保存操作");
    return CONFIG_ERROR_UNSUPPORTED;
}

static uint64_t compute_env_hash(env_source_priv_t *priv)
{
    char **env = environ;
    if (!env)
        return 0;

    uint64_t hash = 14695981039346656037ULL;
    size_t prefix_len = priv->prefix ? strlen(priv->prefix) : 0;

    for (char **p = env; *p; p++) {
        if (prefix_len > 0 && strncmp(*p, priv->prefix, prefix_len) != 0) {
            continue;
        }
        for (const char *s = *p; *s; s++) {
            hash ^= (uint64_t)(unsigned char)*s;
            hash *= 1099511628211ULL;
        }
        hash ^= 0xFFULL;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool env_source_has_changed(config_source_t *source)
{
    if (!source)
        return false;
    env_source_priv_t *priv = (env_source_priv_t *)source->priv_data;
    if (!priv)
        return false;

    uint64_t current_hash = compute_env_hash(priv);
    if (current_hash != priv->env_hash) {
        priv->env_hash = current_hash;
        return true;
    }
    return false;
}

static const config_source_attr_t *env_source_get_attributes(config_source_t *source)
{
    if (!source)
        return NULL;
    return &source->attributes;
}

static void env_source_destroy(config_source_t *source)
{
    if (!source)
        return;

    env_source_priv_t *priv = (env_source_priv_t *)source->priv_data;
    if (priv) {
        if (priv->prefix)
            AIRY_FREE(priv->prefix);
        if (priv->separator)
            AIRY_FREE(priv->separator);
        if (priv->env_keys) {
            for (size_t i = 0; i < priv->env_count; i++) {
                if (priv->env_keys[i])
                    AIRY_FREE(priv->env_keys[i]);
            }
            AIRY_FREE(priv->env_keys);
        }
        AIRY_FREE(priv);
    }

    config_source_free_base(source);
}

static const config_source_adapter_t env_source_adapter = {.load = env_source_load,
                                                           .save = env_source_save,
                                                           .has_changed = env_source_has_changed,
                                                           .get_attributes =
                                                               env_source_get_attributes,
                                                           .destroy = env_source_destroy};

config_source_t *config_source_create_env(const config_env_source_options_t *options)
{
    if (!options)
        return NULL;

    const char *name = options->prefix ? options->prefix : "env";
    config_source_t *source =
        config_source_create_base(CONFIG_SOURCE_ENV, name, &env_source_adapter);
    if (!source)
        return NULL;

    env_source_priv_t *priv = (env_source_priv_t *)AIRY_CALLOC(1, sizeof(env_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    priv->prefix = options->prefix ? duplicate_string(options->prefix) : NULL;
    priv->case_sensitive = options->case_sensitive;
    priv->separator =
        options->separator ? duplicate_string(options->separator) : duplicate_string("_");
    priv->expand_vars = options->expand_vars;
    priv->env_keys = NULL;
    priv->env_count = 0;

    if (options->separator && !priv->separator) {
        if (priv->prefix)
            AIRY_FREE(priv->prefix);
        AIRY_FREE(priv);
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    source->priv_data = priv;
    source->attributes.read_only = true;
    source->attributes.watchable = false;

    return source;
}
