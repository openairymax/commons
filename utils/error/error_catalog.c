// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file error_catalog.c
 * @brief 错误码目录表：code → 名称/中英文描述/严重度的只读静态映射。
 *
 * 纯数据单元，不含任何运行态或锁：handler.c 的三处查询（名称、描述、
 * 严重度）经 airy_err_find() 单点命中，避免同一线性扫描在多个函数里
 * 重复展开。码值以 airy_types.h 权威定义为唯一来源。
 */

#include "error_catalog_internal.h"

static const error_info_t g_error_info[] = {

    {AIRY_OK, "OK", "Success", "成功", AIRY_ERR_SEVERITY_INFO},

    /* S-1 收敛定稿（2026-08-14）：用户态 POSIX errno 负值码（AIRY_EINVAL=-22
     * 等，airy_types.h 权威定义）的描述/严重度映射。错误链压栈与返回值
     * 均为负值，此处直接引用宏（负值）建立映射；[SC] 正幅值宏经
     * AIRY_ERR_NEG 取负后亦可命中（airymax/error.h 子空间码）。 */
    {AIRY_EACCES, "ERR_EACCES", "Operation not permitted", "操作不允许",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_EEXIST, "ERR_EEXIST", "File exists", "文件已存在", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_EFAULT, "ERR_EFAULT", "Bad address", "地址错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_EINTR, "ERR_EINTR", "Interrupted system call", "系统调用被中断",
     AIRY_ERR_SEVERITY_WARNING},
    {AIRY_EINVAL, "ERR_EINVAL", "Invalid argument", "无效参数", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_EIO, "ERR_EIO", "I/O error", "I/O 错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_EISDIR, "ERR_EISDIR", "Is a directory", "是目录", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ENOENT, "ERR_ENOENT", "No such file or directory", "文件或目录不存在",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ENOMEM, "ERR_ENOMEM", "Out of memory", "内存不足", AIRY_ERR_SEVERITY_CRITICAL},
    {AIRY_ENOSPC, "ERR_ENOSPC", "No space left on device", "设备空间不足",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ENOTSUP, "ERR_ENOTSUP", "Operation not supported", "操作不支持",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_EPERM, "ERR_EPERM", "Operation not permitted", "操作不允许",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERANGE, "ERR_ERANGE", "Result too large", "结果过大", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_EBUSY, "ERR_EBUSY", "Device or resource busy", "设备或资源忙",
     AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ECANCELED, "ERR_ECANCELED", "Operation canceled", "操作已取消",
     AIRY_ERR_SEVERITY_INFO},
    {AIRY_EAGAIN, "ERR_EAGAIN", "Try again", "请重试", AIRY_ERR_SEVERITY_WARNING},
    {-AIRY_EIPC_MAGIC, "ERR_EIPC_MAGIC", "Invalid IPC magic", "IPC magic 无效",
     AIRY_ERR_SEVERITY_ERROR},
    {-AIRY_EIPC_OPCODE, "ERR_EIPC_OPCODE", "Unknown IPC opcode", "未知 IPC opcode",
     AIRY_ERR_SEVERITY_ERROR},
    {-AIRY_EIPC_PAYLOAD, "ERR_EIPC_PAYLOAD", "IPC payload out of bounds", "IPC payload 越界",
     AIRY_ERR_SEVERITY_ERROR},
    {-AIRY_EIPC_TIMEOUT, "ERR_EIPC_TIMEOUT", "IPC timeout", "IPC 超时",
     AIRY_ERR_SEVERITY_WARNING},

    {AIRY_ERR_UNKNOWN, "ERR_UNKNOWN", "Unknown error", "未知错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_INVALID_PARAM, "ERR_INVALID_PARAM", "Invalid parameter", "无效参数",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_NULL_POINTER, "ERR_NULL_POINTER", "Null pointer", "空指针", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_OUT_OF_MEMORY, "ERR_OUT_OF_MEMORY", "Out of memory", "内存不足",
     AIRY_ERR_SEVERITY_CRITICAL},
    {AIRY_ERR_BUFFER_TOO_SMALL, "ERR_BUFFER_TOO_SMALL", "Buffer too small", "缓冲区太小",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_NOT_FOUND, "ERR_NOT_FOUND", "Not found", "未找到", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_ALREADY_EXISTS, "ERR_ALREADY_EXISTS", "Already exists", "已存在",
     AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_TIMEOUT, "ERR_TIMEOUT", "Timeout", "超时", AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_NOT_SUPPORTED, "ERR_NOT_SUPPORTED", "Not supported", "不支持",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_PERMISSION_DENIED, "ERR_PERMISSION_DENIED", "Permission denied", "权限不足",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_IO, "ERR_IO", "I/O error", "I/O 错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_PARSE_ERROR, "ERR_PARSE_ERROR", "Parse error", "解析错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_STATE_ERROR, "ERR_STATE_ERROR", "State error", "状态错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_OVERFLOW, "ERR_OVERFLOW", "Overflow", "溢出", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_UNDERFLOW, "ERR_UNDERFLOW", "Underflow", "下溢", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_CANCELED, "ERR_CANCELED", "Canceled", "已取消", AIRY_ERR_SEVERITY_INFO},
    {AIRY_ERR_BUSY, "ERR_BUSY", "Busy", "忙碌", AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_WOULD_BLOCK, "ERR_WOULD_BLOCK", "Would block", "将阻塞", AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_INTERRUPTED, "ERR_INTERRUPTED", "Interrupted", "被中断", AIRY_ERR_SEVERITY_WARNING},

    {AIRY_ERR_SYS_NOT_INIT, "ERR_SYS_NOT_INIT", "System not initialized", "系统未初始化",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_RESOURCE, "ERR_SYS_RESOURCE", "System resource error", "系统资源错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_DEADLOCK, "ERR_SYS_DEADLOCK", "Deadlock", "死锁", AIRY_ERR_SEVERITY_CRITICAL},
    {AIRY_ERR_SYS_THREAD, "ERR_SYS_THREAD", "Thread error", "线程错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_MUTEX, "ERR_SYS_MUTEX", "Mutex error", "互斥锁错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_SEMAPHORE, "ERR_SYS_SEMAPHORE", "Semaphore error", "信号量错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_CONDITION, "ERR_SYS_CONDITION", "Condition variable error", "条件变量错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_ATOMIC, "ERR_SYS_ATOMIC", "Atomic operation error", "原子操作错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_SOCKET, "ERR_SYS_SOCKET", "Socket error", "套接字错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_PIPE, "ERR_SYS_PIPE", "Pipe error", "管道错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_PROCESS, "ERR_SYS_PROCESS", "Process error", "进程错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_FILE, "ERR_SYS_FILE", "File error", "文件错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SYS_TIME, "ERR_SYS_TIME", "Time error", "时间错误", AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_KERN_IPC, "ERR_KERN_IPC", "IPC error", "IPC 错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_KERN_TASK, "ERR_KERN_TASK", "Task error", "任务错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_KERN_SYNC, "ERR_KERN_SYNC", "Synchronization error", "同步错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_KERN_LOCK, "ERR_KERN_LOCK", "Lock error", "锁错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_KERN_MEM, "ERR_KERN_MEM", "Memory error", "内存错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_KERN_SCHED, "ERR_KERN_SCHED", "Scheduler error", "调度器错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_KERN_TIMER, "ERR_KERN_TIMER", "Timer error", "定时器错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_KERN_INTERRUPT, "ERR_KERN_INTERRUPT", "Interrupt error", "中断错误",
     AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_SVC_NOT_READY, "ERR_SVC_NOT_READY", "Service not ready", "服务未就绪",
     AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_SVC_BUSY, "ERR_SVC_BUSY", "Service busy", "服务忙碌", AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_SVC_STOPPED, "ERR_SVC_STOPPED", "Service stopped", "服务已停止",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SVC_CONFIG, "ERR_SVC_CONFIG", "Service configuration error", "服务配置错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SVC_DEPENDENCY, "ERR_SVC_DEPENDENCY", "Service dependency error", "服务依赖错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SVC_HEALTH, "ERR_SVC_HEALTH", "Service health error", "服务健康错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SVC_LOADBALANCE, "ERR_SVC_LOADBALANCE", "Load balance error", "负载均衡错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SVC_CYCLE, "ERR_SVC_CYCLE", "Service dependency cycle detected", "检测到服务依赖环",
     AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_LLM_NO_PROVIDER, "ERR_LLM_NO_PROVIDER", "No LLM provider", "无 LLM 提供商",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_PROVIDER_FAIL, "ERR_LLM_PROVIDER_FAIL", "LLM provider failure", "LLM 提供商失败",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_RATE_LIMIT, "ERR_LLM_RATE_LIMIT", "Rate limit exceeded", "超出速率限制",
     AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_LLM_CONTEXT_LEN, "ERR_LLM_CONTEXT_LEN", "Context length exceeded", "超出上下文长度",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_INVALID_MODEL, "ERR_LLM_INVALID_MODEL", "Invalid model", "无效模型",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_AUTH_FAIL, "ERR_LLM_AUTH_FAIL", "Authentication failed", "认证失败",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_TOKEN_LIMIT, "ERR_LLM_TOKEN_LIMIT", "Token limit exceeded", "超出 Token 限制",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_PARSE_RESP, "ERR_LLM_PARSE_RESP", "Failed to parse response", "解析响应失败",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_EMPTY_RESP, "ERR_LLM_EMPTY_RESP", "Empty response", "空响应",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_COST_EXCEED, "ERR_LLM_COST_EXCEED", "Cost exceeded", "超出成本限制",
     AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_LLM_BAD_REQUEST, "ERR_LLM_BAD_REQUEST", "Provider rejected request body", "模型服务拒绝请求体",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_STREAM_UNDONE, "ERR_LLM_STREAM_UNDONE", "Stream ended without [DONE]", "流式响应缺少终结标记",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_TRUNCATED, "ERR_LLM_TRUNCATED", "Output truncated by token limit", "输出被 Token 上限截断",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_NO_FINISH, "ERR_LLM_NO_FINISH", "Missing finish_reason", "响应缺少终结原因字段",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_LLM_CONTENT_FILTERED, "ERR_LLM_CONTENT_FILTERED", "Blocked by content filter", "输出被内容安全拦截",
     AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_EXEC_NOT_FOUND, "ERR_EXEC_NOT_FOUND", "Executor not found", "执行器未找到",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_EXEC_FAIL, "ERR_EXEC_FAIL", "Execution failed", "执行失败", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_EXEC_TIMEOUT, "ERR_EXEC_TIMEOUT", "Execution timeout", "执行超时",
     AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_EXEC_VALIDATION, "ERR_EXEC_VALIDATION", "Execution validation failed", "执行验证失败",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_EXEC_SANDBOX, "ERR_EXEC_SANDBOX", "Sandbox error", "沙箱错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_EXEC_PERMISSION, "ERR_EXEC_PERMISSION", "Execution permission denied", "执行权限不足",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_EXEC_ARGS, "ERR_EXEC_ARGS", "Invalid execution arguments", "无效执行参数",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_EXEC_ENV, "ERR_EXEC_ENV", "Execution environment error", "执行环境错误",
     AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_MEM_WRITE, "ERR_MEM_WRITE", "Memory write error", "内存写入错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_MEM_READ, "ERR_MEM_READ", "Memory read error", "内存读取错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_MEM_QUERY, "ERR_MEM_QUERY", "Memory query error", "内存查询错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_MEM_EVOLVE, "ERR_MEM_EVOLVE", "Memory evolution error", "内存演进错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_MEM_FULL, "ERR_MEM_FULL", "Memory full", "内存已满", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_MEM_CORRUPT, "ERR_MEM_CORRUPT", "Memory corruption", "内存损坏",
     AIRY_ERR_SEVERITY_CRITICAL},
    {AIRY_ERR_MEM_NOT_INIT, "ERR_MEM_NOT_INIT", "Memory not initialized", "内存未初始化",
     AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_SEC_VIOLATION, "ERR_SEC_VIOLATION", "Security violation", "安全违规",
     AIRY_ERR_SEVERITY_CRITICAL},
    {AIRY_ERR_SEC_SANITIZE, "ERR_SEC_SANITIZE", "Sanitization error", "清理错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SEC_AUDIT, "ERR_SEC_AUDIT", "Audit error", "审计错误", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SEC_PERMISSION, "ERR_SEC_PERMISSION", "Security permission error", "安全权限错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SEC_VALIDATION, "ERR_SEC_VALIDATION", "Security validation error", "安全验证错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SEC_QUOTA, "ERR_SEC_QUOTA", "Quota exceeded", "超出配额", AIRY_ERR_SEVERITY_WARNING},
    {AIRY_ERR_SEC_TEMP_DIR, "ERR_SEC_TEMP_DIR", "Temporary directory error", "临时目录错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SEC_SYMLINK, "ERR_SEC_SYMLINK", "Symbolic link error", "符号链接错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_SEC_PATH_TRAV, "ERR_SEC_PATH_TRAV", "Path traversal detected", "检测到路径遍历",
     AIRY_ERR_SEVERITY_CRITICAL},
    {AIRY_ERR_ESECURITY, "ERR_ESECURITY", "Security error", "安全错误", AIRY_ERR_SEVERITY_CRITICAL},
    {AIRY_ERR_ESANITIZE, "ERR_ESANITIZE", "Sanitization error", "清理错误",
     AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_COORD_PLAN_FAIL, "ERR_COORD_PLAN_FAIL", "Planning failed", "规划失败",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_COORD_SYNC_FAIL, "ERR_COORD_SYNC_FAIL", "Synchronization failed", "同步失败",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_COORD_DISPATCH, "ERR_COORD_DISPATCH", "Dispatch error", "调度错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_COORD_INTENT, "ERR_COORD_INTENT", "Intent error", "意图错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_COORD_COMPENSATE, "ERR_COORD_COMPENSATE", "Compensation error", "补偿错误",
     AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_COORD_RETRY_EXCEED, "ERR_COORD_RETRY_EXCEED", "Retry limit exceeded", "超出重试限制",
     AIRY_ERR_SEVERITY_ERROR},

    {AIRY_ERR_PROTOCOL, "ERR_PROTOCOL", "Protocol violation (magic/version/reserved)",
     "协议违规（magic/version/reserved 字段不匹配）", AIRY_ERR_SEVERITY_ERROR},
    {AIRY_ERR_CHECKSUM, "ERR_CHECKSUM", "Checksum mismatch (CRC32)", "校验和不匹配（CRC32）",
     AIRY_ERR_SEVERITY_ERROR},
};

static const size_t g_error_info_count = sizeof(g_error_info) / sizeof(g_error_info[0]);

const error_info_t *airy_err_table(size_t *count)
{
    if (count != NULL)
        *count = g_error_info_count;
    return g_error_info;
}

const error_info_t *airy_err_find(airy_err_t norm)
{
    for (size_t i = 0; i < g_error_info_count; i++) {
        if (g_error_info[i].code == norm)
            return &g_error_info[i];
    }
    return NULL;
}
