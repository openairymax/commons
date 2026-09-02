# airy_ime — 轻量内置输入法（统一基础库）

`airy_ime` 是 AirymaxRT 内置的全拼输入法，面向无输入法环境（端侧/服务器/
容器）提供中文与英文输入能力，供 CLI/TUI 等宿主调用。位于 `commons/utils/ime`
——输入法是基础系统级功能，置于统一基础库内；atoms 核心系统层保持简约。

## 设计原则

- **运行期纯 C**：`airy_ime.c` + `airy_ime.h` 为唯一运行期代码，
  无任何 Python/外部进程依赖；词典一次性读入内存，查询零拷贝。
- **零外部依赖**：词典 `data/airy_ime.dat` 随安装分发（确定性构建产物，随
  git 追踪），运行期无网络、无数据库、无第三方库。
- **跨架构一致**：显式小端读取 + CRC32 校验，x86 / ARM / RISC-V（含大端）
  行为一致。

## 目录结构

```
commons/utils/ime/
├── include/airy_ime.h   # 公共 API（加载/查询/释放）
├── src/airy_ime.c       # 运行期实现（纯 C）
├── data/airy_ime.dat    # 内置词典（只读，随安装分发）
├── data/README.md       # 词典数据来源与重新生成说明
├── tools/gen_ime_dict.py# 词典生成工具（仅开发期，运行期不依赖）
└── README.md            # 本文件
```

## API 一览

| 函数 | 说明 |
|------|------|
| `airy_ime_load(path, &ime)` | 加载词典（magic/version/CRC 校验） |
| `airy_ime_query(ime, pinyin, ...)` | 全拼前缀查询，输出候选汉字 |
| `airy_ime_free(ime)` | 释放 |

## 开发期生成工具

`gen_ime_dict.py` 仅用于维护者**重新生成**词典（数据来源与用法见
`data/README.md`），不参与运行期；运行期依赖为零，符合 commons 纯 C 约束。
