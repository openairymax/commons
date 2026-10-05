/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file platform_sandbox.h
 * @brief 原生执行沙箱（Landlock + seccomp）
 *
 * 去 docker 化的进程隔离：Linux 上叠加 Landlock（文件系统规则）+
 * seccomp（syscall BPF 过滤）两层原生沙箱。由 airy_process_spawn 在
 * fork 子进程 exec 前调用 airy_native_sandbox_apply 挂接；enabled=0（默认）
 * 为零开销 no-op，非 Linux 平台（macOS/Windows）也为 no-op，进程隔离
 * 由各平台原生机制承担。
 *
 * 安全模型（默认只读）：
 *   - Landlock：restrict 后进程只能访问显式允许的路径。默认允许
 *     "/" 只读（读文件/读目录/执行），rw_paths 叠加读写权限，
 *     ro_paths 叠加额外只读路径。
 *   - seccomp：deny-list 拦截特权/危险 syscall（ptrace、模块加载、
 *     内核执行、进程内存写入等）；deny_network=1 时额外拦截网络族。
 *
 * @see platform.h aggregate entry
 */

#ifndef AIRY_RT_PLATFORM_SANDBOX_H
#define AIRY_RT_PLATFORM_SANDBOX_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Native execution sandbox configuration.
 *
 * @note Thread-safe: N/A (const configuration)
 * @note 仅 Linux 生效；macOS/Windows 恒为 no-op。
 */
typedef struct airy_native_sandbox {
    int enabled;                 /* 0 = disabled（默认，零开销 no-op） */
    int deny_network;            /* 1 = seccomp 拦截 network syscall 族 */
    const char *const *ro_paths; /* Landlock 追加只读路径（NULL 结尾数组，可 NULL） */
    const char *const *rw_paths; /* Landlock 读写路径（NULL 结尾数组，可 NULL） */
} airy_native_sandbox_t;

/**
 * @brief Initialize a sandbox config to the disabled default.
 * @param sb [in/out] sandbox config (must not be NULL)
 */
void airy_native_sandbox_init(airy_native_sandbox_t *sb);

/**
 * @brief Apply the sandbox to the current process (fork child, pre-exec).
 *
 * Linux: enabled=1 时应用 Landlock 规则 + seccomp filter。成功后当前
 * 进程无法回退。enabled=0 或非 Linux 平台返回 0（no-op）。
 *
 * @param sb [in] sandbox config (may be NULL -> no-op)
 * @return 0 = applied or no-op; non-zero = application failed（调用方
 *         应终止 exec 并上报）
 * @note Thread-safe: N/A（仅 fork 子进程单线程上下文调用）
 */
int airy_native_sandbox_apply(const airy_native_sandbox_t *sb);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_SANDBOX_H */
