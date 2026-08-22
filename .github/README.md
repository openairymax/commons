# .github — 持续集成配置

`commons/.github` 存放本仓库的 GitHub Actions CI 配置。

## 内容

- `workflows/ci.yml` — 统一基础库 CI 流水线：
  - 平台矩阵：`ubuntu-latest` / `macos-latest` / `windows-latest`
    （Windows 运行器仅依赖检查，不做原生构建）。
  - 依赖安装：Linux（apt：cmake/libsqlite3/libcjson/libyaml/libcurl/libssl）、
    macOS（brew）、Windows（检查）。
  - 静态检查：License / README / NOTICE 存在性检查；源码目录结构检查
    （utils/* 各模块存在性）；SPDX-License-Identifier 合规检查。

> 说明：当前 CI 为**依赖 + 结构 + 许可合规**检查，未包含编译与测试步骤；
> 本地构建与测试由源外构建（CMake + ctest）完成。后续可扩展编译/测试 job。

## 状态

- 与 `tests/` 配合；CI 失败即阻断合并，保障统一基础库质量基线。
