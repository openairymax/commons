// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file handler.c
 * @brief Unified error handling module implementation.
 *
 * Provides unified error handling:
 * - Error code description and severity management
 * - Error chain tracking and context management
 * - Multi-language error description support
 * - Error statistics and reporting
 */

#include "atomic_compat.h"
#include "error.h"
#include "logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <time.h>

#ifdef _WIN32
#else
#include <sys/time.h>
#endif
#include "platform.h"

static airy_language_t g_current_language = AIRY_LANG_EN_US;

static airy_err_i18n_entry_t *g_i18n_entries = NULL;
static size_t g_i18n_entry_count = 0;

static struct {
    uint64_t total_errors;
    uint64_t errors_by_severity[4];
    uint64_t last_error_time;
    airy_err_t last_error;
} g_error_stats;

#ifdef _WIN32
static airy_mtx_t g_error_stats_mutex;
static atomic_int g_error_stats_initialized = 0;

static void ensure_stats_init(void)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_error_stats_initialized, &expected, 1,
                                                memory_order_acq_rel, memory_order_acquire) == 0) {
        airy_mtx_init(&g_error_stats_mutex);
    }
}

#define STATS_LOCK()                         \
    do {                                     \
        ensure_stats_init();                 \
        airy_mtx_lock(&g_error_stats_mutex); \
    } while (0)

#define STATS_UNLOCK()                         \
    do {                                       \
        airy_mtx_unlock(&g_error_stats_mutex); \
    } while (0)

#else
static airy_mtx_t g_error_stats_mutex = {0};

#define STATS_LOCK()                         \
    do {                                     \
        airy_mtx_lock(&g_error_stats_mutex); \
    } while (0)

#define STATS_UNLOCK()                         \
    do {                                       \
        airy_mtx_unlock(&g_error_stats_mutex); \
    } while (0)
#endif

typedef struct {
    airy_err_chain_t chain;
    int initialized;
} thread_error_state_t;

#ifdef _WIN32
static DWORD g_tls_index = TLS_OUT_OF_INDEXES;

static thread_error_state_t *get_thread_error_state(void)
{
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
        g_tls_index = TlsAlloc();
        if (g_tls_index == TLS_OUT_OF_INDEXES) {
            AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
        }
    }

    thread_error_state_t *state = (thread_error_state_t *)TlsGetValue(g_tls_index);
    if (state == NULL) {
        state = (thread_error_state_t *)AIRY_CALLOC(1, sizeof(thread_error_state_t));
        if (state != NULL) {
            state->initialized = 1;
            TlsSetValue(g_tls_index, state);
        }
    }
    return state;
}
#else
static AIRY_THREAD_LOCAL thread_error_state_t *g_tls_error_state = NULL;

static thread_error_state_t *get_thread_error_state(void)
{
    if (g_tls_error_state == NULL) {
        g_tls_error_state = (thread_error_state_t *)AIRY_CALLOC(1, sizeof(thread_error_state_t));
        if (g_tls_error_state != NULL) {
            g_tls_error_state->initialized = 1;
        }
    }
    return g_tls_error_state;
}
#endif

typedef struct {
    airy_err_t code;
    const char *name;
    const char *description_en;
    const char *description_zh_cn;
    airy_err_severity_t severity;
} error_info_t;

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

const char *airy_err_str(airy_err_t code)
{
    /* S-1 收敛（2026-08-14）：输入码经 AIRY_ERR_NEG 归一（正幅值宏取负、
     * 负值原样），使 airy_err_str(AIRY_ENOMEM)（=9）与 airy_err_str(-9)
     * 均解析为同一描述（A-UEF 宏/返回值双视角一致）。 */
    airy_err_t norm = AIRY_ERR_NEG(code);
    for (size_t i = 0; i < g_error_info_count; i++) {
        if (g_error_info[i].code == norm) {
            return g_error_info[i].description_en;
        }
    }
    return "Unknown error";
}

airy_err_severity_t airy_err_get_severity(airy_err_t code)
{
    airy_err_t norm = AIRY_ERR_NEG(code);
    for (size_t i = 0; i < g_error_info_count; i++) {
        if (g_error_info[i].code == norm) {
            return g_error_info[i].severity;
        }
    }
    return AIRY_ERR_SEVERITY_ERROR;
}

airy_err_chain_t *airy_err_get_chain(void)
{
    thread_error_state_t *state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }
    return &state->chain;
}

void airy_err_clear(void)
{
    thread_error_state_t *state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        return;
    }

    for (int i = 0; i < state->chain.depth; i++) {
        AIRY_FREE((void *)state->chain.contexts[i].message);
        state->chain.contexts[i].message = NULL;
    }
    state->chain.code = AIRY_OK;
    state->chain.depth = 0;
}

void airy_err_thread_cleanup(void)
{
#ifdef _WIN32
    if (g_tls_index != TLS_OUT_OF_INDEXES) {
        thread_error_state_t *state = (thread_error_state_t *)TlsGetValue(g_tls_index);
        if (state != NULL) {

            for (int i = 0; i < state->chain.depth; i++) {
                AIRY_FREE((void *)state->chain.contexts[i].message);
                state->chain.contexts[i].message = NULL;
            }
            AIRY_FREE(state);
            TlsSetValue(g_tls_index, NULL);
        }
    }
#else
    if (g_tls_error_state != NULL) {

        for (int i = 0; i < g_tls_error_state->chain.depth; i++) {
            AIRY_FREE((void *)g_tls_error_state->chain.contexts[i].message);
            g_tls_error_state->chain.contexts[i].message = NULL;
        }
        AIRY_FREE(g_tls_error_state);
        g_tls_error_state = NULL;
    }
#endif
}

static uint64_t get_current_time_ns(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart * 100;
#else
    return airy_time_ns();
#endif
}

void airy_err_push_ex(airy_err_t code, const char *file, int line, const char *func,
                      const char *fmt, ...)
{
    thread_error_state_t *state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        return;
    }

    airy_err_chain_t *chain = &state->chain;

    STATS_LOCK();
    g_error_stats.total_errors++;
    airy_err_severity_t severity = airy_err_get_severity(code);
    if (severity >= 0 && severity < 4) {
        g_error_stats.errors_by_severity[severity]++;
    }
    g_error_stats.last_error_time = get_current_time_ns();
    g_error_stats.last_error = code;
    STATS_UNLOCK();

    char message_buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message_buffer, sizeof(message_buffer), fmt,
              args); /* flawfinder: ignore - variadic error handler with bounded buffer */
    va_end(args);

    if (chain->depth < AIRY_ERROR_CONTEXT_MAX_DEPTH) {
        airy_err_context_entry_t *entry = &chain->contexts[chain->depth];
        entry->file = file;
        entry->line = line;
        entry->function = func;
        entry->message = AIRY_STRDUP(message_buffer);
        entry->error_code = code;
        entry->timestamp_ns = get_current_time_ns();
        chain->depth++;
    } else {

        AIRY_FREE((void *)chain->contexts[0].message);
        for (int i = 0; i < AIRY_ERROR_CONTEXT_MAX_DEPTH - 1; i++) {
            chain->contexts[i] = chain->contexts[i + 1];
        }
        airy_err_context_entry_t *entry = &chain->contexts[AIRY_ERROR_CONTEXT_MAX_DEPTH - 1];
        entry->file = file;
        entry->line = line;
        entry->function = func;
        entry->message = AIRY_STRDUP(message_buffer);
        entry->error_code = code;
        entry->timestamp_ns = get_current_time_ns();
    }

    chain->code = code;
}

void airy_err_print_chain(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        AIRY_LOG_DEBUG("Error chain is NULL");
        return;
    }

    AIRY_LOG_DEBUG("Error chain (depth: %d, latest error: %d)", chain->depth, chain->code);
    for (int i = 0; i < chain->depth; i++) {
        const airy_err_context_entry_t *ctx = &chain->contexts[i];
        (void)ctx;
        AIRY_LOG_DEBUG("  [%d] %s:%d in %s() - %d: %s", i + 1, ctx->file ? ctx->file : "(unknown)",
                  ctx->line, ctx->function ? ctx->function : "(unknown)", ctx->error_code,
                  ctx->message ? ctx->message : "");
    }
}

char *airy_err_chain_to_json(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        return AIRY_STRDUP("{\"error\": \"null chain\"}");
    }

    return airy_err_chain_to_json_i18n(chain, -1);
}

void airy_err_get_stats(airy_err_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }

    STATS_LOCK();
    stats->total_errors = g_error_stats.total_errors;
    for (int i = 0; i < 4; i++) {
        stats->errors_by_code[i] = g_error_stats.errors_by_severity[i];
    }
    stats->last_error_time = g_error_stats.last_error_time;
    stats->last_error = g_error_stats.last_error;
    STATS_UNLOCK();
}

void airy_err_reset_stats(void)
{
    STATS_LOCK();
    g_error_stats.total_errors = 0;
    for (int i = 0; i < 4; i++) {
        g_error_stats.errors_by_severity[i] = 0;
    }
    g_error_stats.last_error_time = 0;
    g_error_stats.last_error = AIRY_OK;
    STATS_UNLOCK();
}

airy_err_t airy_err_set_language(airy_language_t lang)
{
    if ((int)lang < 0 || (int)lang > 7) {
        return AIRY_ERR_INVALID_PARAM;
    }

    g_current_language = lang;
    return AIRY_OK;
}

airy_language_t airy_err_get_language(void)
{
    return g_current_language;
}

const char *airy_err_str_i18n(airy_err_t code, airy_language_t lang)
{
    airy_language_t use_lang = lang;
    if ((int)lang < 0) {
        use_lang = g_current_language;
    }

    if ((int)use_lang < 0 || (int)use_lang > 7) {
        use_lang = AIRY_LANG_EN_US;
    }

    /* S-1 收敛（2026-08-14）：与 airy_err_str 一致，输入码经 AIRY_ERR_NEG
     * 归一（正幅值宏取负），保证宏/返回值双视角均命中同一描述。 */
    airy_err_t norm = AIRY_ERR_NEG(code);

    for (size_t i = 0; i < g_i18n_entry_count; i++) {
        if (g_i18n_entries[i].error_code == norm) {
            const char *desc = g_i18n_entries[i].descriptions[use_lang];
            if (desc != NULL) {
                return desc;
            }
        }
    }

    for (size_t i = 0; i < g_error_info_count; i++) {
        if (g_error_info[i].code == norm) {
            switch (use_lang) {
            case AIRY_LANG_ZH_CN:
                return g_error_info[i].description_zh_cn ? g_error_info[i].description_zh_cn :
                                                           g_error_info[i].description_en;
            default:
                return g_error_info[i].description_en;
            }
        }
    }

    return airy_err_str(code);
}

airy_err_t airy_err_register_i18n(const airy_err_i18n_entry_t *entries, size_t count)
{

    if (entries == NULL || count == 0) {
        return AIRY_ERR_INVALID_PARAM;
    }

    if (g_i18n_entries != NULL) {
        for (size_t i = 0; i < g_i18n_entry_count; i++) {
        }
        AIRY_FREE(g_i18n_entries);
        g_i18n_entries = NULL;
        g_i18n_entry_count = 0;
    }

    g_i18n_entries = airy_malloc_array(count, sizeof(airy_err_i18n_entry_t));
    if (g_i18n_entries == NULL) {
        return AIRY_ERR_OUT_OF_MEMORY;
    }

    __builtin_memcpy(g_i18n_entries, entries, count * sizeof(airy_err_i18n_entry_t));
    g_i18n_entry_count = count;

    return AIRY_OK;
}

char *airy_err_chain_to_json_i18n(const airy_err_chain_t *chain, airy_language_t lang)
{

    if (!chain) {
        return AIRY_STRDUP("{\"error\": \"null\"}");
    }

    airy_language_t use_lang = lang;
    if ((int)lang < 0) {
        use_lang = g_current_language;
    }

    size_t buf_size = 4096;
    char *buf = (char *)AIRY_MALLOC(buf_size);
    if (!buf) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    size_t offset = 0;
    int n = snprintf(buf, buf_size,
                     "{\"code\": %d, \"message\": \"%s\", \"depth\": %d, \"contexts\": [",
                     chain->code, airy_err_str_i18n(chain->code, use_lang), chain->depth);
    /* snprintf returns the number of chars "that would have been written",
     * possibly >= remaining space; offset must always stay <= buf_size,
     * otherwise buf_size - offset underflows and writes out of bounds
     * (same protection pattern as AIRY_REPORT_APPEND in resource_guard.c). */
    if (n < 0) {
        offset = buf_size;
    } else if ((size_t)n >= buf_size) {
        offset = buf_size;
    } else {
        offset = (size_t)n;
    }

    for (int i = 0; i < chain->depth && offset < buf_size; i++) {
        const airy_err_context_entry_t *ctx = &chain->contexts[i];

        char escaped_msg[2048] = {0};
        const char *msg = ctx->message ? ctx->message : "";
        for (size_t j = 0, k = 0; j < strlen(msg) && k < sizeof(escaped_msg) - 1; j++) {
            if (msg[j] == '"' || msg[j] == '\\') {
                escaped_msg[k++] = '\\';
            }
            escaped_msg[k++] = msg[j];
        }
        escaped_msg[sizeof(escaped_msg) - 1] = '\0';

        n = snprintf(buf + offset, buf_size - offset,
                     "%s{\"file\": \"%s\", \"line\": %d, \"function\": \"%s\", \"code\": %d, "
                     "\"message\": \"%s\"}",
                     i > 0 ? ", " : "", ctx->file ? ctx->file : "", ctx->line,
                     ctx->function ? ctx->function : "", ctx->error_code, escaped_msg);
        if (n < 0) {
            offset = buf_size;
        } else if ((size_t)n >= buf_size - offset) {
            offset = buf_size;
        } else {
            offset += (size_t)n;
        }
    }

    if (offset < buf_size) {
        n = snprintf(buf + offset, buf_size - offset, "]}");
        if (n < 0 || (size_t)n >= buf_size - offset) {
            offset = buf_size;
        } else {
            offset += (size_t)n;
        }
    }

    buf[buf_size - 1] = '\0';

    return buf;
}

void airy_err_chain_iter_init(const airy_err_chain_t *chain, airy_err_chain_iterator_t *iter)
{

    if (!iter)
        return;

    iter->chain = chain;
    iter->current_index = 0;
}

const airy_err_context_entry_t *airy_err_chain_iter_next(airy_err_chain_iterator_t *iter)
{

    if (!iter || !iter->chain) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if ((size_t)iter->current_index >= (size_t)iter->chain->depth) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    const airy_err_context_entry_t *ctx = &iter->chain->contexts[iter->current_index];
    iter->current_index++;

    return ctx;
}

void airy_err_chain_iter_reset(airy_err_chain_iterator_t *iter)
{
    if (!iter)
        return;

    iter->current_index = 0;
}

int airy_err_chain_get_depth(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        return 0;
    }
    return chain->depth;
}

airy_err_t airy_err_chain_get_root_error(const airy_err_chain_t *chain)
{
    if (chain == NULL || chain->depth <= 0) {
        return AIRY_OK;
    }
    return chain->contexts[0].error_code;
}

airy_err_t airy_err_ech_get_latest_error(const airy_err_chain_t *chain)
{
    if (chain == NULL) {
        return AIRY_OK;
    }
    return chain->code;
}

char *airy_err_chain_format(const airy_err_chain_t *chain, airy_language_t lang)
{

    if (chain == NULL) {
        return AIRY_STRDUP("(null chain)");
    }

    size_t buf_size = 4096;
    char *buf = (char *)AIRY_MALLOC(buf_size);
    if (buf == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "Error chain (depth=%d, code=%d):\n",
                       chain->depth, chain->code);

    for (int i = 0; i < chain->depth && offset < buf_size - 1; i++) {
        const airy_err_context_entry_t *ctx = &chain->contexts[i];
        offset += snprintf(buf + offset, buf_size - offset, "  [%d] %s:%d in %s(): %s\n", i + 1,
                           ctx->file ? ctx->file : "?", ctx->line,
                           ctx->function ? ctx->function : "?", ctx->message ? ctx->message : "");
    }

    (void)lang;
    return buf;
}

void airy_err_set_handler(airy_err_handler_t handler)
{
    (void)handler;
}

void airy_err_stats_shutdown(void)
{
#ifdef _WIN32
    if (g_error_stats_initialized) {
        airy_mtx_destroy(&g_error_stats_mutex);
        g_error_stats_initialized = 0;
        AIRY_LOG_INFO("Error stats: mutex destroyed");
    }
#endif
}
