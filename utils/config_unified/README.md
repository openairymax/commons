# Config Unified — 统一配置管理

**模块路径**: `commons/utils/config_unified/`
**版本**: 0.1.19

## 概述

Config Unified 是 AgentRT 的统一配置管理模块，提供从配置建模、多源加载到
Schema 默认值填充的分层能力。模块内所有文件平铺于同一目录，按功能域分组：

- **core**：类型化配置值与配置上下文（点分键路径、所有权语义）；
- **source**：配置源适配层，统一适配器接口；内置环境变量源与内存源两类，
  其余源类型为枚举保留位，可按适配器接口外部扩展；
- **service**：Schema 驱动的默认值填充与配置服务入口（create/load）；
- **parse**：JSON / YAML / INI 三种格式的内部解析器（声明于内部头）；
- **yaml_minimal**：独立的 YAML 1.1 子集文档解析/序列化库（AST 模型，公开头文件）。

聚合入口头 `config_unified.h` 包含三层公共头并提供唯一便利宏
`CONFIG_GET_INT_SAFE`。

## 目录结构

```
config_unified/
├── config_unified.h                 # 聚合入口 + 便利宏
├── core_config.h/.c                 # core 层：上下文与接口
├── core_config_value.c              # 值构造与访问
├── core_config_internal.h
├── config_source.h/.c               # source 层：适配器接口与通用基座
├── config_source_env.c              # 环境变量源
├── config_source_memory.c           # 内存源
├── config_source_internal.h
├── config_service.h/.c              # service 层：配置服务入口
├── config_service_validator.c       # Schema（create/add_item/apply_defaults）
├── config_service_internal.h
├── config_parse.c                   # INI 解析（config_parse_ini）
├── config_parse_json.c              # JSON 解析（自研，不依赖 cJSON）
├── config_parse_yaml.c              # YAML 解析（状态机 + 结构递归 + 标量）
├── config_parse_internal.h
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
`CONFIG_ERROR_PARSE`、`CONFIG_ERROR_VALIDATION`、`CONFIG_ERROR_UNSUPPORTED`、
`CONFIG_ERROR_THREAD`（1–9 依次递增）。

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

### 配置上下文 `config_context_t`

| 接口 | 说明 |
|------|------|
| `config_context_create(name)` / `config_context_destroy` | 创建 / 销毁 |
| `config_context_set(ctx, key, value)` | 写入；**value 所有权转移给上下文** |
| `config_context_get(ctx, key)` | 返回 `const config_value_t *` 内部引用，不得释放或修改；键不存在返回 NULL |
| `config_context_has` / `clear` / `count` | 判存 / 清空 / 计数 |
| `config_context_clone` / `config_context_copy` | 深拷贝上下文 / 复制到既有上下文 |
| `config_context_set_schema` | 挂载 Schema（默认值填充来源） |
| `config_context_set_hot_reload(enabled, interval_ms)` / `set_encryption(enabled)` | 运行态开关标记 |

## 便利宏（config_unified.h）

| 宏 | 说明 |
|----|------|
| `CONFIG_GET_INT_SAFE(ctx, key, default_value)` | 键不存在时直接返回默认值；存在时经 `config_value_get_int` 取值（类型不符亦回退默认值） |

## Source 层（config_source.h）

### 配置源类型

`CONFIG_SOURCE_FILE`(0)、`ENV`(1)、`ARGS`(2)、`MEMORY`(3)、`NETWORK`(4)、
`DATABASE`(5)、`DEFAULT`(6)。内置实现为环境变量与内存两类创建入口；
其余类型为枚举保留位（默认值统一经 `config_schema_apply_defaults()` 施加），
外部源按适配器接口实现即可接入。

每个源携带属性 `config_source_attr_t`：`type`、`name`、`priority`（整型优先级，
数值语义由各源约定）、`read_only`、`watchable`、`timestamp`、`version`。

### 创建选项

| 创建函数 | 选项要点 |
|----------|----------|
| `config_source_create_env` | `config_env_source_options_t{prefix, case_sensitive, separator, expand_vars}`；prefix 为可选前缀（NULL 表示不加前缀），separator 默认 `"_"`；env 源只读、不可 save |
| `config_source_create_memory` | `config_memory_source_options_t{data, data_len, format}` |

源对象统一接口：`config_source_load/save/has_changed/get_attributes/destroy`，
内部经适配器 vtable（`load/save/has_changed/get_attributes/destroy`）分派；
`config_source_type_to_string()` 提供类型字符串化。

## Service 层（config_service.h）

### Schema

- `config_schema_create(name)` / `config_schema_destroy(schema)`：生命周期。
- `config_schema_add_item(schema, item)`：条目
  `config_schema_item_t{key, type, required, description, default_value}`。
- `config_schema_apply_defaults(schema, ctx)`：按条目为上下文填充默认值
  （仅填充缺失键）。

### 服务入口

| 接口 | 说明 |
|------|------|
| `config_service_create(name, schema, hot_reload, encryption)` | 创建配置上下文并挂载 Schema；hot_reload / encryption 为运行态开关标记 |
| `config_service_load(ctx, sources, count)` | 按源数组顺序依次加载 |

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
| `yaml_create` / `yaml_destroy` / `yaml_destroy_chain` | 文档上下文（多文档成链） |
| `yaml_parse_string` / `yaml_parse_file` / `yaml_parse_multi` | 解析为 `yaml_document_t` |
| `yaml_get_error` | 解析错误信息 |
| `yaml_root` / `yaml_get` / `yaml_get_index` / `yaml_size` / `yaml_has_key` | 节点树访问（`yaml_node_type_t`：NONE/SCALAR/MAPPING/SEQUENCE） |
| `yaml_as_string/as_int64/as_double/as_bool` | 标量取值 |
| `yaml_dump(node, buf, size, indent)` | 追加式文本输出到调用方缓冲区 |
| `yaml_serialize(doc)` | 序列化为 YAML 文本（堆分配，**必须用 `AIRY_FREE()` 释放**） |

## 用法示例

```c
#include "config_unified.h"

int configure(void)
{
    /* 1. 创建配置服务上下文（挂载 Schema 可选） */
    config_context_t *ctx = config_service_create("agent", NULL, false, false);
    if (!ctx) {
        return -1;
    }

    /* 2. 内存源（YAML 文本）加载，环境变量源覆盖 */
    config_memory_source_options_t mopt = {
        .data = yaml_text, .data_len = yaml_len, .format = "yaml"};
    config_source_t *mem = config_source_create_memory(&mopt);
    config_env_source_options_t eopt = { .prefix = "AGENT_" };
    config_source_t *env = config_source_create_env(&eopt);

    config_source_t *srcs[] = { mem, env };
    config_error_t err = config_service_load(ctx, srcs, 2);
    config_source_destroy(mem);
    config_source_destroy(env);
    if (err != CONFIG_SUCCESS) {
        config_context_destroy(ctx);
        return -1;
    }

    /* 3. 类型安全读取 */
    int temperature = CONFIG_GET_INT_SAFE(ctx, "llm.temperature_pct", 70);
    (void)temperature;

    config_context_destroy(ctx);
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

JSON / YAML / INI 配置解析不依赖任何外部解析库。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
