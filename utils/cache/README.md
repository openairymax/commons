# cache — 通用键值缓存

**模块路径**: `commons/utils/cache/` · **版本**: 0.1.15

基于哈希表与 LRU 淘汰的通用键值缓存，支持 TTL 过期、自定义键值类型回调与字符串键便捷接口，为高频数据访问提供进程内缓存基础设施。

## 概述

- **单一实现**：1024 个固定哈希桶（链地址法）+ 全局 LRU 双向链表；默认字符串哈希为 djb2。
- **深拷贝语义**：`cache_put` 立即以配置的复制回调复制键与值，调用方可自由复用或释放原始内存；`cache_get` 同样输出值的**副本**，调用方须用配置的释放回调回收（字符串缓存即 `cache_string_free`）。
- **线程安全**：每桶一把互斥锁，另有一把全局 LRU 锁；读写可在多线程下并发调用。
- **TTL 惰性过期**：条目过期仅在被 `cache_get` 命中时检测并删除，没有后台清扫；`ttl_sec <= 0` 表示永不过期。
- 内置字符串键的哈希/比较/复制/释放四个默认回调与便捷构造、读写接口。

## 目录结构

```
commons/utils/cache/
├── README.md
├── cache_common.h    # 缓存接口定义
└── cache_common.c    # 哈希表 + LRU 实现
```

## 数据结构

`cache_t` 为不透明指针句柄（`struct cache_impl *`）。

### cache_config_t — 缓存配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `capacity` | `size_t` | 容量上限（默认 1000） |
| `ttl_sec` | `int` | 过期秒数（默认 3600；`<= 0` 永不过期） |
| `hash_func` | `cache_hash_func_t` | 键哈希：`unsigned int (*)(const void *key)` |
| `compare_func` | `cache_compare_func_t` | 键比较：`int (*)(const void *a, const void *b)`，相等返回 0 |
| `key_copy_func` / `value_copy_func` | `cache_copy_func_t` | 深拷贝：`void *(*)(const void *data)` |
| `key_free_func` / `value_free_func` | `cache_free_func_t` | 释放：`void (*)(void *data)` |

`cache_create_default_config()` 返回上表默认值并挂接四个字符串回调；`cache_create(NULL)` 等效于使用默认配置。

## 接口

### 生命周期

| 函数 | 语义 |
|------|------|
| `cache_create_default_config()` | 返回默认配置（容量 1000、TTL 3600 秒、字符串回调） |
| `cache_create(config)` | 创建缓存；`config` 为 `NULL` 用默认配置；失败返回 `NULL`（错误入栈） |
| `cache_destroy(cache)` | 销毁并释放全部条目，接受 `NULL` |
| `cache_create_string_cache(capacity, ttl_sec)` | 字符串键值缓存的便捷构造 |

### 数据操作

| 函数 | 语义 |
|------|------|
| `cache_get(cache, key, out_value)` | 查找并输出**值副本**。返回 1 命中、0 未命中或已过期、`AIRY_EINVAL` 参数错误 |
| `cache_put(cache, key, value)` | 存入键值对（深拷贝）；键已存在则整体替换并刷新时间戳；`value == NULL` 等效删除；容量为 0 时忽略 |
| `cache_delete(cache, key)` | 删除条目 |
| `cache_clear(cache)` | 清空全部条目 |
| `cache_get_size` / `cache_get_capacity` | 当前条目数 / 容量上限（`cache` 为 `NULL` 时返回 0） |
| `cache_set_capacity(cache, capacity)` | 调整容量；缩容时立即按 LRU 批量逐出至满足新上限 |
| `cache_get_ttl` / `cache_set_ttl` | 读取 / 调整过期秒数（仅对后续过期判断生效） |

### 字符串便捷接口

| 函数 | 语义 |
|------|------|
| `cache_get_string` / `cache_put_string` | 以 `const char *` 键值直接读写（内部转调通用接口） |
| `cache_string_hash` / `cache_string_compare` / `cache_string_copy` / `cache_string_free` | 默认字符串回调，可复用于自定义配置 |

## 容量与并发语义

- **LRU 更新**：`cache_get` 命中后以 `trylock` 方式将条目移至链表头；锁竞争时跳过热度更新，不影响正确性。
- **逐出**：`cache_put` 使条目数超过容量时逐出 LRU 尾端 1 条；`cache_set_capacity` 缩容则循环逐出。
- **过期检查**：在桶锁内完成，过期条目从哈希链与 LRU 链同时摘除并按配置的释放回调回收，`cache_get` 对其返回未命中。
- 通用接口约定 `key` 指向键对象本体（字符串场景即 `char *` 本身，而非 `char **`），四个回调须与该键类型配套。

## 用法示例

### 字符串缓存

```c
#include "cache_common.h"

cache_t cache = cache_create_string_cache(100, 300);

if (cache) {
    cache_put_string(cache, "user:42", "{\"name\":\"Alice\"}");

    char *value = NULL;
    int rc = cache_get_string(cache, "user:42", &value);
    if (rc == 1) {
        /* 使用 value（值副本）... */
        cache_string_free(value);
    }

    cache_destroy(cache);
}
```

### 自定义键值类型

```c
#include "cache_common.h"

cache_config_t config = cache_create_default_config();
config.capacity = 1000;
config.ttl_sec = 600;
config.hash_func = my_hash;        /* unsigned int (*)(const void *) */
config.compare_func = my_compare;  /* int (*)(const void *, const void *) */
config.key_copy_func = my_copy;    /* void *(*)(const void *) */
config.key_free_func = my_free;    /* void (*)(void *) */
config.value_copy_func = my_copy;
config.value_free_func = my_free;

cache_t cache = cache_create(&config);

if (cache) {
    void *value = NULL;
    int rc = cache_get(cache, &my_key, &value);
    if (rc == 0) {
        /* 未命中：计算 value 后存入，缓存持有副本 */
        cache_put(cache, &my_key, &computed);
    } else if (rc == 1) {
        my_free(value); /* 命中输出副本，须自行释放 */
    }

    cache_destroy(cache);
}
```

## 构建与依赖

`cache_common.c` 随 commons 编入静态库 `airy_common`；`cache_common.h` 随库安装并作为 PUBLIC include 目录导出。

```cmake
target_link_libraries(<your_target> PRIVATE airy_common)
```

| 依赖 | 用途 |
|------|------|
| [memory](../memory/README.md) | `memory_safe_alloc` / `memory_safe_strdup` / `memory_safe_free` 内部分配 |
| [sync](../sync/README.md) | `sync_common.h` 轻量互斥锁（桶锁与 LRU 锁） |
| [error](../error/README.md) | `AIRY_EINVAL` 错误码与 error 栈 |
| C 标准库 | `string` / `time` |

本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
