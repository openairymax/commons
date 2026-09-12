# Config Unified — 统一配置管理

**模块路径**: `commons/utils/config_unified/`
**版本**: 0.1.15

## 概述

Config Unified 是 AgentRT 的统一配置管理模块，提供从配置建模、多源加载、校验到
运行时热更新的一站式能力。模块内所有文件平铺于同一目录，按功能域分为五组：

- **core**：类型化配置值与配置上下文（点分键路径、所有权语义、读写锁）；
- **source**：文件 / 环境变量 / 命令行参数 / 内存 / 远程（NETWORK）五类配置源，
  经统一适配器接口由来源管理器按属性优先级合并加载，支持变更监听与轮询；
- **service**：Schema 校验、范围/正则/枚举/自定义校验器、热重载、AES-256-GCM
  加密、版本快照与回滚、模板展开、配置服务入口；
- **parse**：JSON / YAML / INI 三种格式的内部解析器；
- **yaml_minimal**：独立的 YAML 1.1 子集文档解析/序列化库（AST 模型，公开头文件）。

聚合入口头 `config_unified.h` 包含三层公共头并提供 27 个便捷宏。

## 目录结构

```
config_unified/
├── config_unified.h                 # 聚合入口 + 便捷宏
├── core_config.h/.c                 # core 层：值/上下文/迭代器 API
├── core_config_value.c              # 值构造与访问
├── core_config_strings.c            # 类型名/错误码字符串
├── core_config_internal.h
├── config_source.h/.c               # source 层：适配器接口与通用实现
├── config_source_file.c             # 文件源（inotify/kqueue/ReadDirectoryChangesW）
├── config_source_env.c              # 环境变量源
├── config_source_args.c             # 命令行参数源
├── config_source_memory.c           # 内存源与远程源
├── config_source_manager.c          # 源管理器（优先级合并/监听/轮询）
├── config_source_internal.h
├── config_service.h/.c              # service 层：配置服务入口
├── config_service_validator.c       # 校验器与 Schema
├── config_service_hotreload.c       # 热重载管理器
├── config_service_crypto.c          # AES-256-GCM 加解密
├── config_service_version.c         # 版本管理
├── config_service_internal.h
├── config_parse.c                   # INI 解析（config_parse_ini）
├── config_parse_json.c              # JSON 解析（自研，不依赖 cJSON）
├── config_parse_yaml.c              # YAML 解析（状态机骨架 + 结构递归）
├── config_parse_yaml_scalar.c       # YAML 标量与流式集合
├── config_parse_yaml.h / config_parse_internal.h
└── yaml_minimal.h                   # YAML 子集解析器公共 API
    yaml_minimal.c                   # 入口（含块结构解析）
    yaml_minimal_lexer.c / _parser.c # 词法 / 文档解析驱动
    yaml_minimal_node.c / _anchor.c  # 节点构造 / 锚点与别名
    yaml_minimal_scalar.c / _value.c # 标量 / 流式值
    yaml_minimal_serialize.c         # 序列化输出
    yaml_minimal_internal.h
```

## 类型系统

### 配置值类型 `config_value_type_t`（core_config.h）

| 枚举 | 值 | 枚举 | 值 |
|------|----|------|----|
| `CONFIG_TYPE_NULL` | 0 | `CONFIG_TYPE_STRING` | 5 |
| `CONFIG_TYPE_BOOL` | 1 | `CONFIG_TYPE_ARRAY` | 6 |
| `CONFIG_TYPE_INT` | 2 | `CONFIG_TYPE_OBJECT` | 7 |
| `CONFIG_TYPE_INT64` | 3 | `CONFIG_TYPE_BINARY` | 8 |
| `CONFIG_TYPE_DOUBLE` | 4 | | |

### 错误码 `config_error_t`

`CONFIG_SUCCESS`(0)、`CONFIG_ERROR_INVALID_ARG`、`CONFIG_ERROR_NOT_FOUND`、
`CONFIG_ERROR_TYPE_MISMATCH`、`CONFIG_ERROR_OUT_OF_MEMORY`、`CONFIG_ERROR_IO`、
`CONFIG_ERROR_PARSE`、`CONFIG_ERROR_VALIDATION`、`CONFIG_ERROR_LOCKED`、
`CONFIG_ERROR_UNSUPPORTED`、`CONFIG_ERROR_THREAD`（1–10 依次递增）。
`config_error_to_string()` / `config_type_to_string()` 提供字符串化。

## Core 层（core_config.h）

配置键采用点分路径（如 `"database.host"`）。

### 配置值 `config_value_t`

| 接口 | 说明 |
|------|------|
| `config_value_create_null/bool/int/int64/double/string/array/object` | 按类型构造 |
| `config_value_clone` / `config_value_destroy` | 深拷贝 / 销毁 |
| `config_value_get_type` | 查询类型 |
| `config_value_get_bool/int/int64/double/string` | 取值，类型不符或 NULL 时返回 `default_value`；字符串为内部所有，不得释放 |
| `config_value_array_append(array, item)` | 向数组追加元素 |
| `config_value_print(value, indent)` | 调试打印 |

### 配置上下文 `config_context_t`

| 接口 | 说明 |
|------|------|
| `config_context_create(name)` / `config_context_destroy` | 创建 / 销毁 |
| `config_context_set(ctx, key, value)` | 写入；**value 所有权转移给上下文** |
| `config_context_get(ctx, key)` | 返回 `const config_value_t *` 内部引用，不得释放或修改 |
| `config_context_delete` / `has` / `clear` / `count` | 删除 / 判存 / 清空 / 计数 |
| `config_context_lock` / `unlock` | 锁定后 `set` 返回 `CONFIG_ERROR_LOCKED` |
| `config_context_clone` / `config_context_copy` | 深拷贝上下文 / 复制到既有上下文 |
| `config_context_get_key_at` / `get_value_at` | 按下标遍历 |
| `config_context_iterator` + `config_iterator_reset/has_next/next_key` | 迭代器 |
| `config_context_set_schema` / `set_hot_reload(enabled, interval_ms)` / `set_encryption` | 挂载 schema、热重载开关、加密开关 |

## 便捷宏（config_unified.h）

| 类别 | 宏 |
|------|-----|
| 取值 | `CONFIG_STRING(ctx, key, def)` / `CONFIG_INT` / `CONFIG_BOOL` / `CONFIG_DOUBLE` |
| 安全读写 | `CONFIG_SET_SAFE`、`CONFIG_GET_STRING_SAFE` / `GET_INT_SAFE` / `GET_BOOL_SAFE` / `GET_DOUBLE_SAFE` |
| 常用路径 | `CONFIG_PATH` / `DB_PATH` / `LOG_PATH` / `NETWORK_PATH` / `SECURITY_PATH` |
| 校验 | `CONFIG_VALID_STRING` / `VALID_INT` / `VALID_BOOL` / `VALID_DOUBLE` |
| 错误处理 | `CONFIG_SUCCESS(err)`、`CONFIG_FAILED(err)`、`RETURN_IF_FAILED`、`GOTO_IF_FAILED` |
| 初始化/转换 | `CONFIG_INIT_WITH_DEFAULTS`、`CONFIG_AS_STRING/INT/BOOL/DOUBLE` |

注意宏 `CONFIG_SUCCESS(err)`（判定宏）与枚举值 `CONFIG_SUCCESS`（0）同名不同用法。

## Source 层（config_source.h）

### 配置源类型

`CONFIG_SOURCE_FILE`(0)、`ENV`(1)、`ARGS`(2)、`MEMORY`(3)、`NETWORK`(4)、
`DATABASE`(5)、`DEFAULT`(6)。当前提供实现的创建入口为文件、环境变量、命令行
参数、内存、默认值五类，远程配置经 `config_source_create_remote()` 创建并归入
NETWORK 类型；DATABASE 为枚举占位。

每个源携带属性 `config_source_attr_t`：`type`、`name`、`priority`（整型优先级，
数值语义由各源约定）、`read_only`、`watchable`、`timestamp`、`version`。

### 创建选项

| 创建函数 | 选项要点 |
|----------|----------|
| `config_source_create_file` | `config_file_source_options_t{file_path, format, encoding, auto_reload, reload_interval_ms}` |
| `config_source_create_env` | `config_env_source_options_t{prefix, case_sensitive, separator, expand_vars}`；prefix 为可选前缀（NULL 表示不加前缀），separator 默认 `"_"`；env 源只读、不可 save |
| `config_source_create_args` | `config_args_source_options_t{argc, argv, prefix, assign_char, allow_positional}` |
| `config_source_create_memory` | `config_memory_source_options_t{data, data_len, format}` |
| `config_source_create_defaults` | `key=value` 字符串数组默认值表 |
| `config_source_create_remote(url, token, ns, poll_interval_ms)` | 远程配置源：url 必需，token 可选，namespace 默认 `"default"`，轮询间隔默认 30000 ms；只读、可监听，响应按 JSON 解析并以 ETag/内容哈希判定变更 |

源对象统一接口：`config_source_load/save/has_changed/get_attributes/get_type/destroy`，
内部经适配器 vtable（`load/save/has_changed/get_attributes/destroy`）分派。

### 源管理器

| 接口 | 说明 |
|------|------|
| `config_source_manager_create/destroy` | 管理器生命周期 |
| `config_source_manager_add/remove/find` | 注册、移除、按名查找源 |
| `config_source_manager_load_all(mgr, ctx, merge_strategy)` | 按优先级依次加载全部源；策略 `0`=覆盖、`1`=合并、`2`=智能合并 |
| `config_source_manager_watch(cb, user_data)` | 注册变更回调 |
| `config_source_manager_poll_changes(mgr)` | 轮询检测变更（内置约 500 ms 防抖），返回变更源数或 -1 |
| `config_source_type_to_string` / `config_parse_file_format` / `config_source_create_name` | 工具函数 |

文件源格式按扩展名分派 json / yaml / ini（未识别默认按 json 尝试，失败回退
yaml）；变更监听平台实现为 inotify（Linux）、kqueue（BSD/macOS）、
ReadDirectoryChangesW（Windows）。

## Service 层（config_service.h）

### 校验器与 Schema

- `config_validator_create(options)`：按 `validator_options_t` 创建，类型为
  `VALIDATOR_TYPE_RANGE/REGEX/ENUM/CUSTOM`；快捷入口
  `config_validator_create_range(min, max)`、`create_regex(pattern)`、
  `create_enum(values, count)`；`config_validator_validate(validator, key, value)`
  执行校验（自定义回调签名 `bool (*)(const char *key, const config_value_t *value,
  void *user_data)`）。
- `config_schema_create(name)` / `add_item(schema, item)`：条目
  `config_schema_item_t{key, type, required, description, default_value, validator}`；
  `config_schema_validate(schema, ctx, strict)` 整表校验，
  `config_schema_get_error(index)` 读取错误明细，
  `config_schema_apply_defaults(schema, ctx)` 填充默认值。

### 热重载

`config_hot_reload_manager_create(ctx, source_manager)` → 
`config_hot_reload_register_callback(manager, key, cb, user_data)`
（key 传 NULL 监听全部；回调 `void (*)(ctx, key, old_value, new_value, user_data)`）
→ `config_hot_reload_start(manager, check_interval_ms)` 启动轮询线程 →
`stop` / `trigger`（手动触发一次重载）/ `destroy`。

### 加密

`config_encrypt_value` / `config_decrypt_value`（返回值为新 `config_value_t`，hex
编码的 `tag(16B) ‖ iv ‖ ciphertext`），以及 `config_source_create_encrypted()`
将任意源包装为加密源。算法为 **AES-256-GCM**（OpenSSL EVP），要求
`encryption_config_t.key_len ≥ 32`、`iv_len ≥ 12`；需编译期定义 `HAVE_OPENSSL`
并链接 OpenSSL，未启用时加解密返回错误。枚举中的 `ENCRYPTION_CHACHA20_POLY1305`
目前为保留项，无实现。

### 版本管理

`config_version_manager_create(ctx, max_versions)`；`config_version_create_snapshot
(manager, author, description)` 返回版本号；`config_version_rollback(manager,
version)`；`config_version_get_list` / `get_diff` 查询 `config_version_info_t`
（version、timestamp、author、description、change_count）。

### 模板

`config_expand_template(ctx, template_str, result, size)` 展开字符串中的引用；
`config_apply_template(ctx, template_ctx)` 将模板上下文批量套入。

### 服务入口

| 接口 | 说明 |
|------|------|
| `config_service_create(name, schema, hot_reload, encryption)` | 创建配置上下文并挂载服务 |
| `config_service_load(ctx, sources, count)` | 按源数组顺序加载 |
| `config_service_save(ctx, primary_source)` | 回写主源 |
| `config_service_get_status(ctx, json_buf, size)` | 输出 JSON 格式状态 |

## 格式解析

配置加载管线的三种格式解析器均为模块内自研、无外部解析库依赖，入口声明于
内部头：

| 入口 | 格式 | 说明 |
|------|------|------|
| `config_parse_json(data, len, ctx)` | JSON | 手写递归下降，对象展平为点分键 |
| `config_parse_yaml(data, len, ctx)` | YAML | 状态机骨架 + mapping/sequence 递归 + 标量侧（引号/块标量/流式集合） |
| `config_parse_ini(data, len, ctx)` | INI | `[section]` + `key=value` 展平为 `section.key` |

不支持 TOML 格式。

## yaml_minimal — YAML 1.1 子集解析器

`yaml_minimal.h` 是公开的独立 API（AST 风格的 YAML 文档模型），仅依赖
`airy_memory.h` 与 `error.h`，支持子集：锚点/别名、标签、`---`/`...`
文档分隔、折叠 `>` 与字面 `|` 块标量、chomping 指示符、合并键 `<<`、流式
`{}`/`[]`、`%YAML`/`%TAG` 指令、BOM。

| 接口 | 说明 |
|------|------|
| `yaml_create` / `yaml_destroy` | 解析器上下文 |
| `yaml_parse_string` / `yaml_parse_file` / `yaml_parse_multi` | 解析为 `yaml_document_t`（多文档成链，`yaml_destroy_chain` 释放） |
| `yaml_get_error` | 解析错误信息 |
| `yaml_document_root` / `yaml_node_get` / `get_index` / `size` / `has_key` | 节点树访问（`yaml_node_type_t`：NONE/SCALAR/MAPPING/SEQUENCE） |
| `yaml_node_as_string/as_int64/as_double/as_bool` | 标量取值 |
| `yaml_dump(node, buf, size, indent)` | 追加式文本输出到调用方缓冲区 |
| `yaml_serialize(doc)` | 序列化为 YAML 文本（堆分配，**必须用 `AIRY_FREE()` 释放**） |

## 用法示例

```c
#include "config_unified.h"
#include "logger.h" /* AIRY_LOG_*，来自 utils/observability */

static void on_change(config_context_t *ctx, const char *key,
                      const config_value_t *old_v, const config_value_t *new_v,
                      void *user_data)
{
    (void)ctx; (void)old_v; (void)new_v; (void)user_data;
    AIRY_LOG_INFO("config key %s changed", key);
}

int configure(config_source_t **sources, size_t n)
{
    /* 1. 创建配置服务上下文 */
    config_context_t *ctx = config_service_create("agent", NULL, true, false);
    if (!ctx) {
        return -1;
    }

    /* 2. 多源加载（文件 + 环境变量覆盖） */
    config_file_source_options_t fopt = { .file_path = "agent.yaml",
                                          .format = NULL };
    config_source_t *file = config_source_create_file(&fopt);
    config_env_source_options_t eopt = { .prefix = "AGENT_" };
    config_source_t *env = config_source_create_env(&eopt);

    config_source_t *srcs[] = { file, env };
    if (CONFIG_FAILED(config_service_load(ctx, srcs, 2))) {
        config_context_destroy(ctx);
        return -1;
    }

    /* 3. 类型安全读取 */
    const char *model = CONFIG_STRING(ctx, "llm.model", "default");
    int temperature = CONFIG_INT(ctx, "llm.temperature_pct", 70);
    (void)model; (void)temperature;

    /* 4. 热重载轮询 */
    config_source_manager_t *mgr = config_source_manager_create();
    config_source_manager_add(mgr, file); /* 管理器接管 file 源生命周期 */
    config_hot_reload_manager_t *hr = config_hot_reload_manager_create(ctx, mgr);
    config_hot_reload_register_callback(hr, NULL, on_change, NULL);
    config_hot_reload_start(hr, 5000);

    /* ...运行期... */

    /* 5. 收尾 */
    config_hot_reload_stop(hr);
    config_hot_reload_manager_destroy(hr);
    config_source_manager_destroy(mgr); /* 同时释放已注册的 file 源 */
    config_source_destroy(env);
    config_context_destroy(ctx);
    (void)sources; (void)n;
    return 0;
}
```

## 构建与依赖

本模块源码全部编入静态库 `airy_common`，头文件目录经 PUBLIC 导出，使用者
`#include "config_unified.h"`（或单独 `core_config.h` / `yaml_minimal.h`）。

| 依赖 | 来源 | 用途 |
|------|------|------|
| `airy_memory.h` | [`utils/memory`](../memory/README.md) | 全部内存分配与释放 |
| `error.h` | [`utils/error`](../error/README.md) | 错误码与日志宏 |
| `string_compat.h` | [`utils/string`](../string/README.md) | 安全字符串操作 |
| `logging_compat.h` / `logging.h` | [`utils/include`](../include/README.md) / [`utils/logging`](../logging/README.md) | 内部日志 |
| `atomic_compat.h` | [`utils/include`](../include/README.md) | 源/上下文原子状态 |
| `platform.h` | [`commons/platform`](../../platform/README.md) | 跨平台时间戳 |
| OpenSSL（可选） | 外部 | AES-256-GCM 加密，需定义 `HAVE_OPENSSL` |

JSON / YAML / INI 配置解析不依赖任何外部解析库。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
