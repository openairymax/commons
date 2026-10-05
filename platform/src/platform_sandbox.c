// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_sandbox.c
 * @brief Native execution sandbox: Landlock (FS rules) + seccomp (BPF).
 *
 * In-process Linux sandboxing primitives, applied in the fork child
 * before exec (see platform_process.c). Pure C11, no external deps;
 * non-Linux platforms degrade to no-op.
 *
 * Landlock requires kernel >= 5.13 and linux/landlock.h; seccomp BPF
 * requires linux/filter.h + linux/seccomp.h + linux/audit.h. Absence of
 * either header or an unsupported architecture degrades that layer only
 * (Landlock-only or seccomp-only operation remains valid).
 */

#if defined(__linux__)
/* O_PATH / O_CLOEXEC 等 GNU 扩展需在系统头之前定义（项目 flags 可能已定义）。 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "platform_sandbox.h"
#include "platform_internal.h"

#if defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/landlock.h>)
#include <linux/landlock.h>
#define AIRY_HAVE_LANDLOCK 1
#endif
#if __has_include(<linux/filter.h>) && __has_include(<linux/seccomp.h>) && \
    __has_include(<linux/audit.h>) && __has_include(<sys/prctl.h>)
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#define AIRY_HAVE_SECCOMP 1
#endif
#endif /* __has_include */

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

/* Landlock syscall numbers (x86_64 / aarch64 identical). */
#ifndef SYS_landlock_create_ruleset
#define SYS_landlock_create_ruleset 444
#endif
#ifndef SYS_landlock_add_rule
#define SYS_landlock_add_rule 445
#endif
#ifndef SYS_landlock_restrict_self
#define SYS_landlock_restrict_self 446
#endif

#if defined(__x86_64__)
#define AIRY_SANDBOX_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define AIRY_SANDBOX_AUDIT_ARCH AUDIT_ARCH_AARCH64
#elif defined(__riscv) && (__riscv_xlen == 64)
#define AIRY_SANDBOX_AUDIT_ARCH AUDIT_ARCH_RISCV64
#else
#define AIRY_SANDBOX_AUDIT_ARCH 0
#endif

void airy_native_sandbox_init(airy_native_sandbox_t *sb)
{
    if (!sb)
        return;
    AIRY_MEMSET(sb, 0, sizeof(*sb));
    sb->global_read = 1; /* 默认 "/" 只读基线（历史行为兼容） */
}

/* ============================================================================
 * Landlock: 默认 "/" 只读 + ro_paths 只读 + rw_paths 读写
 * ============================================================================ */

#if defined(AIRY_HAVE_LANDLOCK)

/* Landlock ABI: 1=5.13, 2=5.19 (REFER), 3=6.2 (TRUNCATE). */
static int landlock_abi(void)
{
    return (int)syscall(SYS_landlock_create_ruleset, NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
}

static int landlock_add_path(int ruleset_fd, uint64_t allowed_access, const char *path)
{
    int fd = open(path, O_PATH | O_CLOEXEC);
    if (fd < 0) {
        /* 不存在的路径（架构/部署差异，如 /lib32）跳过：Landlock 对未
         * 列出路径默认拒绝，缺规则不影响隔离正确性。 */
        return errno == ENOENT ? 0 : -1;
    }

    /* 目录专属位（READ_DIR、REMOVE_DIR/FILE、MAKE_* 系列）挂在非目录
     * 文件上会被内核以 EINVAL 拒绝（如 /dev/null 写规则），整个 ruleset
     * 随之失效。按 fstat 裁剪：文件只保留文件级访问位。 */
    uint64_t access = allowed_access;
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        access &= (uint64_t)LANDLOCK_ACCESS_FS_EXECUTE |
                  (uint64_t)LANDLOCK_ACCESS_FS_WRITE_FILE |
                  (uint64_t)LANDLOCK_ACCESS_FS_READ_FILE |
                  (uint64_t)LANDLOCK_ACCESS_FS_TRUNCATE;
    }

    struct landlock_path_beneath_attr attr;
    AIRY_MEMSET(&attr, 0, sizeof(attr));
    attr.allowed_access = access;
    attr.parent_fd = fd;

    int rc = (int)syscall(SYS_landlock_add_rule, ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &attr, 0);
    close(fd);
    return rc;
}

static int landlock_apply(const airy_native_sandbox_t *sb)
{
    int abi = landlock_abi();
    if (abi < 1) {
        /* 内核不支持 Landlock：调用方显式启用时视为失败（exec 前置错误）。 */
        return -1;
    }

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
        return -1;

    uint64_t handled = (uint64_t)LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE |
                       LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
                       LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |
                       LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR |
                       LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK |
                       LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
                       LANDLOCK_ACCESS_FS_MAKE_SYM | LANDLOCK_ACCESS_FS_REFER |
                       LANDLOCK_ACCESS_FS_TRUNCATE;
    if (abi == 1)
        handled &= ~(uint64_t)LANDLOCK_ACCESS_FS_REFER;
    if (abi < 3)
        handled &= ~(uint64_t)LANDLOCK_ACCESS_FS_TRUNCATE;

    struct landlock_ruleset_attr ruleset_attr;
    AIRY_MEMSET(&ruleset_attr, 0, sizeof(ruleset_attr));
    ruleset_attr.handled_access_fs = handled;

    int ruleset_fd =
        (int)syscall(SYS_landlock_create_ruleset, &ruleset_attr, sizeof(ruleset_attr), 0);
    if (ruleset_fd < 0)
        return -1;

    /* global_read=1：全盘只读基线（读文件/读目录/执行），rw_paths 叠加
     * 读写后对应目录可写，系统其余保持只读。global_read=0：无全局基线，
     * 仅 ro_paths/rw_paths 白名单路径可达（STRICT 白名单模式）。 */
    const uint64_t read_access = (uint64_t)LANDLOCK_ACCESS_FS_READ_FILE |
                                 LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_EXECUTE;

    if (sb->global_read && landlock_add_path(ruleset_fd, read_access, "/") != 0) {
        close(ruleset_fd);
        return -1;
    }

    if (sb->ro_paths) {
        for (size_t i = 0; sb->ro_paths[i] != NULL; i++) {
            if (landlock_add_path(ruleset_fd, read_access, sb->ro_paths[i]) != 0) {
                close(ruleset_fd);
                return -1;
            }
        }
    }

    if (sb->rw_paths) {
        for (size_t i = 0; sb->rw_paths[i] != NULL; i++) {
            if (landlock_add_path(ruleset_fd, handled, sb->rw_paths[i]) != 0) {
                close(ruleset_fd);
                return -1;
            }
        }
    }

    int rc = (int)syscall(SYS_landlock_restrict_self, ruleset_fd, 0);
    close(ruleset_fd);
    return rc;
}

#else /* !AIRY_HAVE_LANDLOCK */

static int landlock_apply(const airy_native_sandbox_t *sb)
{
    /* 无 linux/landlock.h：Landlock 层不可用。enabled 由调用方保证为真，
     * 这里仅返回失败以便调用方感知（不静默降级）。 */
    (void)sb;
    return -1;
}

#endif /* AIRY_HAVE_LANDLOCK */

/* ============================================================================
 * seccomp BPF: deny-list 特权/危险 syscall + 可选网络族
 * ============================================================================ */

#if defined(AIRY_HAVE_SECCOMP)

#define AIRY_SANDBOX_FILTER_MAX 192

/* Linux >= 6.4 的 <linux/filter.h> 将 BPF_STMT/BPF_JUMP 改为 C++ 兼容的
 * 双重括号复合字面量，在纯 C 中不合法。此处自定义等价宏，使用位置
 * 初始化（struct sock_filter 顺序：code/jt/jf/k，避免 designator 兼容性
 * 差异）。 */
#define AIRY_SANDBOX_BPF_STMT(code, k) \
    ((struct sock_filter){(code), 0, 0, (k)})
#define AIRY_SANDBOX_BPF_JUMP(code, k, jt, jf) \
    ((struct sock_filter){(code), (jt), (jf), (k)})

/* 预计算 seccomp_data 偏移（offsetof 的逗号会破坏宏参数，故提前取出）。 */
#define AIRY_SANDBOX_DENY(filter, n, off_nr, nr)     \
    do {                                             \
        (filter)[(n)++] =                            \
            AIRY_SANDBOX_BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (off_nr)); \
        (filter)[(n)++] =                            \
            AIRY_SANDBOX_BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (nr), 0, 1); \
        (filter)[(n)++] =                            \
            AIRY_SANDBOX_BPF_STMT(BPF_RET | BPF_K,   \
                                  SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)); \
    } while (0)

static int seccomp_apply(const airy_native_sandbox_t *sb)
{
#if AIRY_SANDBOX_AUDIT_ARCH
    struct sock_filter filter[AIRY_SANDBOX_FILTER_MAX];
    const size_t off_arch = offsetof(struct seccomp_data, arch);
    const size_t off_nr = offsetof(struct seccomp_data, nr);
    size_t n = 0;

    /* 架构校验：非本架构一律终止（防 32 位 compat 绕过）。 */
    filter[n++] = AIRY_SANDBOX_BPF_STMT(BPF_LD | BPF_W | BPF_ABS, off_arch);
    filter[n++] = AIRY_SANDBOX_BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
                                        AIRY_SANDBOX_AUDIT_ARCH, 1, 0);
    filter[n++] = AIRY_SANDBOX_BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS);

    /* 特权/危险 syscall deny-list（默认拒绝，返回 EPERM）。清单为
     * 全项目统一口径（tool_d shell 沙箱旧清单并集，机制归位 SSoT）。 */
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_ptrace);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_bpf);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_perf_event_open);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_init_module);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_finit_module);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_delete_module);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_kexec_load);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_kexec_file_load);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_process_vm_readv);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_process_vm_writev);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_reboot);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_swapon);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_swapoff);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_setns);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_unshare);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_mount);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_umount2);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_pivot_root);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_acct);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_sethostname);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_setdomainname);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_open_by_handle_at);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_name_to_handle_at);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_keyctl);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_add_key);
    AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_request_key);

    if (sb->deny_network) {
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_socket);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_socketpair);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_connect);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_accept);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_accept4);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_bind);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_listen);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_sendto);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_recvfrom);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_sendmsg);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_recvmsg);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_sendmmsg);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_recvmmsg);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_setsockopt);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_getsockopt);
        AIRY_SANDBOX_DENY(filter, n, off_nr, __NR_shutdown);
    }

    filter[n++] = AIRY_SANDBOX_BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);

    if (n > AIRY_SANDBOX_FILTER_MAX)
        return -1; /* 防御：指令数超上限（编译期常量路径，理论不可达） */

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
        return -1;

    struct sock_fprog prog;
    prog.len = (unsigned short)n;
    prog.filter = filter;
    return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
#else
    (void)sb;
    return 0; /* 未知架构：seccomp 不启用（Landlock 仍可提供隔离） */
#endif
}

#else /* !AIRY_HAVE_SECCOMP */

static int seccomp_apply(const airy_native_sandbox_t *sb)
{
    (void)sb;
    return 0; /* 无 linux/filter.h：seccomp 层不可用，Landlock 独立生效 */
}

#endif /* AIRY_HAVE_SECCOMP */

int airy_native_sandbox_landlock_available(void)
{
#if defined(AIRY_HAVE_LANDLOCK)
    return landlock_abi() >= 1;
#else
    return 0;
#endif
}

int airy_native_sandbox_apply(const airy_native_sandbox_t *sb)
{
    if (!sb || !sb->enabled)
        return 0; /* 默认关闭 / NULL：零开销 no-op */

    /* Landlock 与 seccomp 独立分层：一层失败不阻断另一层。
     * 两者皆失败才上报（Landlock 无头文件时依赖 seccomp 仍可成立）。 */
    int rc_ll = landlock_apply(sb);
    int rc_sc = seccomp_apply(sb);

    if (rc_ll != 0 && rc_sc != 0)
        return -1;
    return 0;
}

#else /* !__linux__ */

void airy_native_sandbox_init(airy_native_sandbox_t *sb)
{
    if (!sb)
        return;
    AIRY_MEMSET(sb, 0, sizeof(*sb));
    sb->global_read = 1;
}

int airy_native_sandbox_landlock_available(void)
{
    return 0;
}

int airy_native_sandbox_apply(const airy_native_sandbox_t *sb)
{
    (void)sb;
    return 0; /* macOS/Windows：进程隔离由平台原生机制承担 */
}

#endif /* __linux__ */
