# security — 安全工具

**模块路径**: `commons/utils/security/` · **版本**: 0.1.15

提供两个相互独立的安全组件：`input_validator`（基于白名单的输入验证与净化）和 `log_sanitizer`（日志敏感信息脱敏）。两者均编译进 commons 静态库 `airy_common`。

## 概述

- **input_validator**：对字符串、标识符、JSON、文件路径、Shell 命令、SQL、URL、数值范围做验证，并提供 `airy_safe_*` 系列带边界检查的缓冲区操作。遵循白名单优先、边界从严、失败即拒绝的原则。纯函数、无内部状态，可并发调用。
- **log_sanitizer**：维护一张全局敏感字段模式表（内置 15 个默认模式，如 `api_key`、`password`、`token`、`authorization`），对任意日志文本做大小写不敏感的模式匹配，将字段值替换为掩码（默认 `***`）。内部以互斥锁保护全局表，接口线程安全。

## 目录结构

```
security/
├── input_validator.h        # 输入验证接口（17 个函数 + 4 个宏）
├── input_validator.c        # 输入验证实现
├── log_sanitizer.h          # 日志脱敏接口（7 个函数）
├── log_sanitizer.c          # 日志脱敏实现
└── README.md
```

## input_validator — 输入验证与净化

### 验证结果结构

```c
typedef struct {
    int is_valid;             /* 1 = 通过，0 = 拒绝 */
    const char *error_message; /* 静态字符串，无需释放 */
    int error_code;           /* AIRY_EINVAL / AIRY_ESECURITY / AIRY_ESANITIZE */
    const char *error_field;  /* 出错字段名，静态字符串 */
} airy_validation_result_t;
```

所有 `airy_validate_*` 函数返回 `void`，结果写入调用方提供的 `result`；`result` 为 NULL 时直接返回、不产生任何输出。

### 验证接口

| 函数 | 说明 |
|------|------|
| `airy_validate_string_length(str, min_len, max_len, result)` | 长度区间校验 |
| `airy_validate_string_charset(str, allowed_chars, result)` | 字符白名单校验（逐字符 `strchr`） |
| `airy_validate_identifier(str, max_len, result)` | 标识符：字母/下划线开头，其后字母/数字/下划线 |
| `airy_validate_json_string(str, max_len, result)` | 仅做括号平衡与引号闭合扫描，非完整 JSON 解析 |
| `airy_validate_file_path(path, allowed_root, result)` | 拒绝含 `..` 的路径；`allowed_root` 非 NULL 时要求路径以之为前缀；长度 ≤ 4096 |
| `airy_validate_shell_command(cmd, allowed_commands, result)` | 拒绝 `;`、`|`、`&`、`$`、反引号、换行；黑名单子串检测；白名单为前缀匹配 |
| `airy_validate_sql_query(sql, result)` | 危险关键字子串检测 + 单引号奇偶校验 |
| `airy_validate_url(url, allowed_schemes, result)` | 危险协议前缀检测；SSRF 私有地址前缀为全 URL 子串检测 |
| `airy_validate_int_range(value, min, max, result)` | 整数范围 |
| `airy_validate_float_range(value, min, max, result)` | 浮点范围 |

### 净化与解析接口

| 函数 | 说明 |
|------|------|
| `airy_normalize_path(path, out_normalized, out_len)` | POSIX 走 `realpath`（要求路径存在）；Windows 走 `GetFullPathNameA`（纯词法，≤ MAX_PATH）。输出由调用方释放 |
| `airy_sanitize_shell_param(param, out_sanitized)` | 单引号包裹整体，内部 `'` 转义为 `'\''`，非可打印字符与 `` ` `` `$` 静默剔除。输出由调用方释放 |
| `airy_sanitize_sql_identifier(identifier, out_sanitized)` | 先按标识符规则校验（≤ 128），再以双引号包裹。输出由调用方释放 |
| `airy_parse_url(url, out_scheme, out_host, out_port, out_path)` | 拆分 `://` 后的协议/主机/端口/路径，各 out 参数可为 NULL。无端口时端口为 0，无路径时路径为空串 |

### 缓冲区安全操作

| 函数 | 说明 |
|------|------|
| `airy_safe_memcpy(dest, dest_size, src, src_size)` | `src_size > dest_size` 时返回 `AIRY_EOVERFLOW` |
| `airy_safe_strcpy(dest, dest_size, src)` | 要求含终止符空间，`strlen(src) >= dest_size` 返回 `AIRY_EOVERFLOW` |
| `airy_safe_strcat(dest, dest_size, src)` | 拼接后总长（含终止符）不得超出 `dest_size` |

### 便捷宏

| 宏 | 说明 |
|------|------|
| `AIRY_VALIDATE_OR_RETURN(result, error_code)` | 未通过验证则返回给定错误码 |
| `AIRY_VALIDATE_OR_GOTO(result, label, error_code)` | 未通过验证则赋值 `err` 并 `goto label`（要求作用域内已声明 `err`） |
| `AIRY_SAFE_STRCPY(dest, src)` / `AIRY_SAFE_STRCAT(dest, src)` | 自动以 `sizeof(dest)` 作为容量 |

## log_sanitizer — 日志脱敏

| 函数 | 说明 |
|------|------|
| `log_sanitizer_init(max_fields)` | 重建模式表（容量 0 → 默认 32），预置内置模式 |
| `log_sanitizer_destroy(void)` | 释放模式表并复位，之后可再次 `init` |
| `log_sanitizer_add_pattern(pattern, replacement)` | 追加模式；`replacement` 为 NULL 时用 `***`；表满返回 `false` |
| `log_sanitize(message, buffer, buffer_size)` | 脱敏写入调用方缓冲 |
| `log_sanitize_dup(message)` | 动态版本，返回调用方释放的副本 |
| `log_contains_sensitive(message)` | 是否命中任一模式 |
| `log_get_default_patterns(count)` | 取内置模式表（15 项）与数量 |

脱敏行为：模式匹配大小写不敏感，且要求字段名前后为边界字符（前：串首/空白/引号/逗号；后：`=` `:` 空格/引号/换行/`&`/串尾）。命中后从字段名结束到值结束整段消费，重写为 `字段名=替换串`——即原分隔符（`:`、`=`、空白）被归一为 `=`，例如 `password: hunter2` 输出 `password=***`。

## 语义与约束

- `airy_normalize_path` 在 POSIX 下要求目标路径真实存在（`realpath` 语义），文件不存在返回 `AIRY_EINVAL`，与参数非法不可区分。
- 路径校验只检测 `..` 子串：任何位置出现连续两个点（含合法文件名）都会被拒绝；不检查符号链接，也无法检测内嵌空字节（`const char *` 接口在首个 NUL 处即截断，头文件注释中的该项声明与实现不符，检测逻辑已作为恒假式移除）。
- Shell 白名单与 URL scheme 白名单均为不区分大小写的前缀匹配，不校验词边界；SSRF 私有地址检测是对完整 URL 做子串匹配，路径或版本号中出现 `10.`、`fc` 等前缀会产生误报。检测规则偏保守，适合纵深防御的一层而非完备判定。
- `airy_parse_url` 不支持 IPv6 字面量（方括号内 `:` 会被当作端口分隔符）。
- 日志脱敏的全局模式表由内部互斥锁保护；`pattern`/`replacement` 字符串按指针保存，调用方须保证其生命周期覆盖 sanitizer 使用期。
- `log_sanitize` 成功返回输出长度；失败返回负错误码（参数非法 `-36`、缓冲不足 `-60`），调用方按 `< 0` 判断即可。`log_sanitize_dup` 分配量不小于 4096 字节。
- 脱敏输出始终为完整拷贝，不存在零拷贝路径。

## 用法示例

```c
#include "input_validator.h"
#include "log_sanitizer.h"
#include "airy_memory.h"

#include <stddef.h>

/* 请求入口：验证 + 净化 */
int handle_request(const char *id, const char *cmd)
{
    airy_validation_result_t v;
    char *safe_arg = NULL;
    int ret;

    airy_validate_identifier(id, 64, &v);
    if (!v.is_valid) {
        return -1;
    }

    const char *allowed[] = { "ls", "cat", NULL };
    airy_validate_shell_command(cmd, allowed, &v);
    if (!v.is_valid) {
        return -1;
    }

    ret = airy_sanitize_shell_param(id, &safe_arg);
    if (ret == AIRY_SUCCESS) {
        /* 拼接命令时只使用 safe_arg（已单引号包裹） */
        AIRY_FREE(safe_arg);
    }
    return ret;
}

/* 写日志前脱敏 */
void prepare_log_line(const char *raw, char *out, size_t out_size)
{
    if (log_sanitize(raw, out, out_size) < 0) {
        out[0] = '\0'; /* 溢出或参数非法时宁可不输出 */
    }
}
```

## 构建与依赖

两个源文件均由 `commons/CMakeLists.txt` 列入 `airy_common` 源列表，`utils/security` 注册为 PUBLIC include 目录，头文件随 `install` 导出到 `include/agentrt/utils/security/`。链接 `airy_common` 即可使用，无需额外 target。

| 依赖 | 用途 |
|------|------|
| `utils/error`（`error.h`、`error_codes.h`） | `AIRY_EINVAL`、`AIRY_ESECURITY`、`AIRY_ESANITIZE`、`AIRY_EOVERFLOW` 等错误码 |
| `utils/memory`（`airy_memory.h`） | 净化输出与模式表的分配/释放（`AIRY_MALLOC`/`AIRY_FREE`） |
| `utils/logging`（`svc_logger.h`） | `log_sanitizer` 的事件日志（`SVC_LOG_*` 转发到 `AIRY_LOG_*`） |
| `utils/include`（`atomic_compat.h`） | `log_sanitizer` 的互斥锁与原子原语 |

本模块仅依赖 commons 内部其他工具模块，无 agentrt 内部上游依赖。

`input_validator` 的单元测试位于 `commons/tests/unit/test_input_validator.c`；`log_sanitizer` 在 commons 测试套件中暂无专项用例。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
