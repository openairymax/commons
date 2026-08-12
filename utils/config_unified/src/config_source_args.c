// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_source_args.c
 * @brief 统一配置模块 - 命令行配置源实现
 *
 * 本文件实现命令行参数配置源：前缀过滤、赋值符解析
 * 与只读语义，单一职责。
 */

#include "config_source.h"

#include "config_source_internal.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"
#include "error.h"

static config_error_t args_source_load(config_source_t *source, config_context_t *ctx)
{
    if (!source || !ctx)
        return CONFIG_ERROR_INVALID_ARG;
    args_source_priv_t *priv = (args_source_priv_t *)source->priv_data;
    if (!priv || !priv->argv)
        return CONFIG_ERROR_INVALID_ARG;
    size_t prefix_len = priv->prefix ? strlen(priv->prefix) : 0;
    const char *assign = priv->assign_char ? priv->assign_char : "=";
    for (int idx = 1; idx < priv->argc; idx++) {
        const char *arg = priv->argv[idx];
        if (!arg)
            continue;
        if (prefix_len > 0 && strncmp(arg, priv->prefix, prefix_len) != 0)
            continue;
        const char *eq = strstr(arg, assign);
        if (!eq)
            continue;
        size_t key_len = (size_t)(eq - arg);
        char key[512];
        if (key_len >= sizeof(key))
            key_len = sizeof(key) - 1;
        __builtin_memcpy(key, arg + prefix_len, key_len - prefix_len);
        key[key_len - prefix_len] = '\0';
        const char *val = eq + strlen(assign);
        config_value_t *cv = config_value_create_string(val);
        if (cv)
            config_context_set(ctx, key, cv);
    }
    return CONFIG_SUCCESS;
}

static config_error_t args_source_save(config_source_t *source, const config_context_t *ctx)
{
    (void)source;
    (void)ctx;
    AIRY_LOG_WARN("命令行配置源为只读，不支持保存操作");
    return CONFIG_ERROR_UNSUPPORTED;
}

static bool args_source_has_changed(config_source_t *source)
{
    (void)source;
    return false;
}

static const config_source_attr_t *args_source_get_attributes(config_source_t *source)
{
    if (!source)
        return NULL;
    return &source->attributes;
}

static void args_source_destroy(config_source_t *source)
{
    if (!source)
        return;

    args_source_priv_t *priv = (args_source_priv_t *)source->priv_data;
    if (priv) {
        if (priv->prefix)
            AIRY_FREE(priv->prefix);
        if (priv->assign_char)
            AIRY_FREE(priv->assign_char);
        // 注意：不释放argv，因为通常不拥有所有权
        AIRY_FREE(priv);
    }

    config_source_free_base(source);
}

static const config_source_adapter_t args_source_adapter = {.load = args_source_load,
                                                            .save = args_source_save,
                                                            .has_changed = args_source_has_changed,
                                                            .get_attributes =
                                                                args_source_get_attributes,
                                                            .destroy = args_source_destroy};

config_source_t *config_source_create_args(const config_args_source_options_t *options)
{
    if (!options || options->argc <= 0 || !options->argv)
        return NULL;

    const char *name = options->prefix ? options->prefix : "args";
    config_source_t *source =
        config_source_create_base(CONFIG_SOURCE_ARGS, name, &args_source_adapter);
    if (!source)
        return NULL;

    args_source_priv_t *priv = (args_source_priv_t *)AIRY_CALLOC(1, sizeof(args_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    priv->argc = options->argc;
    priv->argv = options->argv;
    priv->prefix = options->prefix ? duplicate_string(options->prefix) : NULL;
    priv->assign_char =
        options->assign_char ? duplicate_string(options->assign_char) : duplicate_string("=");
    priv->allow_positional = options->allow_positional;

    if (!priv->assign_char) {
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
