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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(__MACH__)
/* macOS 动态链接下不导出全局 environ（仅 libc 起始代码可见），
 * 须经 _NSGetEnviron() 取环境向量指针。 */
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"

static uint64_t compute_env_hash(env_source_priv_t *priv);

/* 截取 prefix 之后的键名，返回 '=' 右侧的值指针；无 '=' 返回 NULL。
 * 键名超长按容量截断，与基线行为一致。 */
static const char *env_key_of(const char *entry, size_t prefix_len, char *key, size_t cap)
{
    const char *eq = strchr(entry, '=');
    if (!eq)
        return NULL;
    size_t len = (size_t)(eq - entry) - prefix_len;
    if (len >= cap)
        len = cap - 1;
    __builtin_memcpy(key, entry + prefix_len, len);
    key[len] = '\0';
    return eq + 1;
}

static void env_lower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

/* 双层分隔符（如 __）映射为层级点，单层分隔符保留为词边界。 */
static void env_key_map(const char *key, const char *sep, char *out, size_t cap)
{
    size_t sep_len = strlen(sep);
    size_t mi = 0;
    const char *src = key;
    while (*src && mi < cap - 1) {
        if (sep_len > 0 && strncmp(src, sep, sep_len) == 0 &&
            strncmp(src + sep_len, sep, sep_len) == 0) {
            out[mi++] = '.';
            src += sep_len * 2;
        } else {
            out[mi++] = *src++;
        }
    }
    out[mi] = '\0';
}

static config_value_t *env_value_parse(const char *val)
{
    char *end = NULL;
    long iv = strtol(val, &end, 10);
    if (end != val && *end == '\0')
        return config_value_create_int((int32_t)iv);

    char low[64];
    size_t len = strlen(val);
    if (len >= sizeof(low))
        return config_value_create_string(val);
    __builtin_memcpy(low, val, len + 1);
    env_lower(low);
    if (strcmp(low, "true") == 0)
        return config_value_create_bool(true);
    if (strcmp(low, "false") == 0)
        return config_value_create_bool(false);
    return config_value_create_string(val);
}

static config_error_t env_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    env_source_priv_t *priv = (env_source_priv_t *)source->priv_data;
    if (!priv)
        return CONFIG_ERROR_INVALID_ARG;
    if (!environ)
        return CONFIG_SUCCESS;

    size_t prefix_len = priv->prefix ? strlen(priv->prefix) : 0;
    const char *sep = priv->separator ? priv->separator : "_";
    for (size_t idx = 0; environ[idx]; idx++) {
        const char *entry = environ[idx];
        if (prefix_len > 0 && strncmp(entry, priv->prefix, prefix_len) != 0)
            continue;

        char key[512];
        const char *val = env_key_of(entry, prefix_len, key, sizeof(key));
        if (!val)
            continue;
        if (!priv->case_sensitive)
            env_lower(key);

        char mapped[512];
        env_key_map(key, sep, mapped, sizeof(mapped));

        config_value_t *cv = env_value_parse(val);
        if (cv)
            config_context_set(ctx, mapped, cv);
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
