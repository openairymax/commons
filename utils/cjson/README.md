# cjson — cJSON 三步宏辅助层

`cjson` 提供 cJSON 的**声明式宏辅助层**（P0.18.2），位于 `commons/utils/cjson`。
目标：消除全项目 544 处重复的 `cJSON_Parse + NULL 检查 + cJSON_GetObjectItem +
cJSON_Delete` 样板代码，不改变 cJSON 本身语义。

## 核心宏

| 宏 | 用途 |
|----|------|
| `CJSON_PARSE_GUARD` | 解析 + NULL 检查 + 失败动作（声明式 RAII 前置） |
| `CJSON_GET_REQUIRED` | 必填字段提取 + NULL 检查 + 失败动作 |
| `CJSON_GET_OPTIONAL` | 可选字段提取（NULL 容忍） |
| `CJSON_AUTO_FREE` | 作用域自动 `cJSON_Delete`（GCC/Clang cleanup 属性） |

## 设计要点

- 失败路径由调用方通过 `on_fail` 块控制（goto / return / log）。
- 与既有 `AIRY_MALLOC` / `AUTO_FREE` 风格一致。
- MSVC 下 `CJSON_AUTO_FREE` 为空（回退手动释放）。
- 线程安全：宏仅操作局部变量，无共享状态。
- 由 `AIRY_HAS_CJSON` 门控；未链接 cJSON 的目标仅得到空回退，避免未解析符号。

## 状态

- **实现**：`include/cjson_helpers.h`（宏定义，无独立运行期代码）。
- **验收**：`grep -rn 'cJSON_Parse' agentrt/ | wc -l` 相比引入前减少 ≥50%。
- **测试**：随 commons 单元测试覆盖宏路径。
