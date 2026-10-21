// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_process.c
 * @brief 进程管理域：跨平台进程启动/等待/终止/收割、子进程生成与命令输出捕获。
 */

#include "platform_internal.h"

/* Pipe-pair teardown helper for the process implementation below; the
 * public airy_pipe_* primitives live in platform_pipe.c. */
static void pipe_pair_close(int fds[2])
{
    airy_pipe_close(&fds[0]);
    airy_pipe_close(&fds[1]);
}

/* Shared spawn prologue: validate arguments, reset the process handle and
 * stdio descriptors, and seed the three pipe pairs to "closed". The win and
 * posix spawn bodies diverge only after this point, so the prologue is
 * defined once here instead of being duplicated in both platform branches. */
static int spawn_prep(const char *exe, char *const argv[], airy_process_info_t *proc,
                      int in_pipe[2], int out_pipe[2], int err_pipe[2])
{
    if (!exe || !argv || !proc)
        return AIRY_EINVAL;
    AIRY_MEMSET(proc, 0, sizeof(airy_process_info_t));
    proc->stdin_fd = -1;
    proc->stdout_fd = -1;
    proc->stderr_fd = -1;
    in_pipe[0] = in_pipe[1] = -1;
    out_pipe[0] = out_pipe[1] = -1;
    err_pipe[0] = err_pipe[1] = -1;
    return 0;
}

/* 捕获缓冲的唯一写入点：尾部始终留一个 '\0'，超界按剩余空间截断。
 * 两个平台分支原先各自内联了同一段越界计算，缩容时极易一处改、另一处漏。 */
static size_t cap_copy(char *dst, size_t cap, size_t off, const char *src, size_t n)
{
    if (!dst || cap == 0 || off + 1 >= cap)
        return off;
    if (n > cap - 1 - off)
        n = cap - 1 - off;
    __builtin_memcpy(dst + off, src, n);
    return off + n;
}

/* 捕获缓冲的收尾写点：off=0 即清空，其余为正文尾部定界。判空集中在这里，
 * 两平台分支的缓冲生命周期形状因此完全一致。 */
static void cap_seal(char *output, size_t output_size, size_t off)
{
    if (output && output_size > 0)
        output[off] = '\0';
}

#if AIRY_PLATFORM_WINDOWS

/* Build a quoted Windows command line from argv (argv[0] is skipped; the
 * executable is quoted separately), shared by start/run_capture/spawn. */
static void win_build_cmdline(const char *executable, char *const argv[], char *cmdline, size_t cap)
{
    snprintf(cmdline, cap, "\"%s\"", executable);
    for (int i = 1; argv && argv[i]; i++) {
        size_t remaining = cap - strlen(cmdline);
        if (remaining > 0)
            snprintf(cmdline + strlen(cmdline), remaining, " \"%s\"", argv[i]);
    }
}

/* CreateProcessA needs one contiguous block of NUL-separated "NAME=VALUE"
 * strings terminated by an empty string; build it from the POSIX-style
 * environment array. Caller frees the result. */
static char *win_env_block(char *const env[])
{
    size_t total = 1;
    for (int i = 0; env && env[i]; i++)
        total += strlen(env[i]) + 1;
    char *block = (char *)malloc(total);
    if (!block)
        return NULL;
    char *p = block;
    for (int i = 0; env && env[i]; i++) {
        size_t len = strlen(env[i]) + 1;
        AIRY_MEMCPY(p, env[i], len);
        p += len;
    }
    *p = '\0';
    return block;
}

/* Wire stdio handles for CreateProcessA: inherit parent stdio by default,
 * create/wire pipes for the requested redirections. Parent-side pipe ends
 * get the inherit flag stripped (a leaked parent end would keep the pipe
 * open in the child and suppress EOF). */
static int win_spawn_stdio(const airy_process_opt_t *opt, STARTUPINFOA *si, int in_pipe[2],
                           int out_pipe[2], int err_pipe[2])
{
    si->dwFlags = STARTF_USESTDHANDLES;
    si->hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si->hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si->hStdError = GetStdHandle(STD_ERROR_HANDLE);

    if (opt && opt->redirect_stdin && airy_pipe_create(in_pipe) != 0)
        return -1;
    if (opt && opt->redirect_stdout && airy_pipe_create(out_pipe) != 0)
        return -1;
    if (opt && opt->redirect_stderr && airy_pipe_create(err_pipe) != 0)
        return -1;

    if (in_pipe[0] >= 0) {
        si->hStdInput = (HANDLE)_get_osfhandle(in_pipe[0]);
        SetHandleInformation((HANDLE)_get_osfhandle(in_pipe[1]), HANDLE_FLAG_INHERIT, 0);
    }
    if (out_pipe[1] >= 0) {
        si->hStdOutput = (HANDLE)_get_osfhandle(out_pipe[1]);
        SetHandleInformation((HANDLE)_get_osfhandle(out_pipe[0]), HANDLE_FLAG_INHERIT, 0);
    }
    if (err_pipe[1] >= 0) {
        si->hStdError = (HANDLE)_get_osfhandle(err_pipe[1]);
        SetHandleInformation((HANDLE)_get_osfhandle(err_pipe[0]), HANDLE_FLAG_INHERIT, 0);
    }
    return 0;
}

int airy_process_start(const char *executable, char *const argv[], char *const envp[],
                       airy_process_info_t *proc)
{
    (void)envp;

    if (!proc)
        return AIRY_EINVAL;
    AIRY_MEMSET(proc, 0, sizeof(airy_process_info_t));

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    AIRY_MEMSET(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    AIRY_MEMSET(&pi, 0, sizeof(pi));

    char cmdline[4096];
    win_build_cmdline(executable, argv, cmdline, sizeof(cmdline));

    BOOL success =
        CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                       NULL, NULL, &si, &pi);

    if (!success) {
        return AIRY_EINVAL;
    }

    proc->process_handle = (void *)pi.hProcess;
    proc->thread_handle = (void *)pi.hThread;
    proc->pid = pi.dwProcessId;

    return 0;
}

int airy_process_wait(airy_process_info_t *proc, uint32_t timeout_ms, int *exit_code)
{
    if (!proc || !proc->process_handle)
        return AIRY_EINVAL;

    HANDLE h_process = (HANDLE)proc->process_handle;
    DWORD result = WaitForSingleObject(h_process, timeout_ms == 0 ? INFINITE : timeout_ms);
    if (result == WAIT_TIMEOUT) {
        return AIRY_ERR_TIMEOUT;
    }
    if (result != WAIT_OBJECT_0) {
        return AIRY_EINVAL;
    }

    DWORD code;
    if (!GetExitCodeProcess(h_process, &code)) {
        return AIRY_EINVAL;
    }

    if (exit_code) {
        *exit_code = (int)code;
    }

    CloseHandle(h_process);
    if (proc->thread_handle) {
        CloseHandle((HANDLE)proc->thread_handle);
        proc->thread_handle = NULL;
    }
    proc->process_handle = NULL;

    return 0;
}

int airy_process_kill(airy_process_info_t *proc)
{
    if (!proc || !proc->process_handle)
        return AIRY_EINVAL;
    return TerminateProcess((HANDLE)proc->process_handle, 1) ? 0 : -1;
}

void airy_process_close_pipes(airy_process_info_t *proc)
{
    if (!proc)
        return;
    if (proc->thread_handle) {
        CloseHandle((HANDLE)proc->thread_handle);
        proc->thread_handle = NULL;
    }
    if (proc->process_handle) {
        CloseHandle((HANDLE)proc->process_handle);
        proc->process_handle = NULL;
    }
}

/* 摘取输出：drain=0 只取当前就绪的一段（轮询路径），drain=1 阻塞读到 EOF
 * 或缓冲填满（子进程退出后的排空路径）。 */
static size_t win_pull(HANDLE pipe, char *output, size_t output_size, size_t off, int drain)
{
    char buf[4096];
    DWORD avail = 0;
    DWORD got;

    if (!output || output_size == 0)
        return off;
    for (;;) {
        if (!drain && (!PeekNamedPipe(pipe, NULL, 0, NULL, &avail, NULL) || avail == 0))
            return off;
        if (!ReadFile(pipe, buf, sizeof(buf) - 1, &got, NULL) || got == 0)
            return off;
        off = cap_copy(output, output_size, off, buf, (size_t)got);
        if (!drain || off + 1 >= output_size)
            return off;
    }
}

/* 一轮采样：先判取消与超时（两者都直接终止进程并结束循环，残留数据不再
 * 回收），再摘取就绪输出，最后以 50ms 粒度探测退出。 */
static int win_cycle(HANDLE proc, HANDLE pipe, airy_cancel_token_t *tok,
                     uint64_t deadline_ms, int *timed_out, int *canceled,
                     char *output, size_t output_size, size_t *off)
{
    if (tok && airy_cancel_token_is_canceled(tok)) {
        *canceled = 1;
        TerminateProcess(proc, 1);
        return 1;
    }
    if (deadline_ms > 0 && airy_time_ms() >= deadline_ms) {
        *timed_out = 1;
        TerminateProcess(proc, 1);
        return 1;
    }
    *off = win_pull(pipe, output, output_size, *off, 0);
    return (WaitForSingleObject(proc, 50) == WAIT_OBJECT_0) ? 1 : 0;
}

int airy_process_run_capture_ex(const char *executable, char *const argv[], char *const envp[],
                                uint32_t timeout_ms, char *output, size_t output_size,
                                airy_cancel_token_t *cancel_token)
{
    (void)envp;
    char cmdline[4096];
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE pipe_read = NULL;
    HANDLE pipe_write = NULL;
    size_t offset = 0;
    uint64_t deadline_ms;
    int timed_out = 0;
    int canceled = 0;
    DWORD exit_code = 1;

    /* 用 CreateProcess + 匿名管道替代 _popen：命令行不经 cmd.exe 解释，
     * 与 POSIX fork/execvp 的行为对齐，也不存在 shell 注入面。 */
    win_build_cmdline(executable, argv, cmdline, sizeof(cmdline));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0))
        return AIRY_ERR_IO;
    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = pipe_write;
    si.hStdError = pipe_write;
    si.hStdInput = NULL;
    ZeroMemory(&pi, sizeof(pi));

    /* CREATE_NO_WINDOW 与 airy_process_start 对齐，避免拉起控制台程序时闪窗 */
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pipe_write);
        CloseHandle(pipe_read);
        return AIRY_ERR_EXEC_FAIL;
    }
    CloseHandle(pipe_write);

    cap_seal(output, output_size, 0);
    deadline_ms = (timeout_ms > 0) ? airy_time_ms() + (uint64_t)timeout_ms : 0;
    while (!win_cycle(pi.hProcess, pipe_read, cancel_token, deadline_ms,
                      &timed_out, &canceled, output, output_size, &offset))
        ;
    if (!canceled && !timed_out)
        offset = win_pull(pipe_read, output, output_size, offset, 1);
    cap_seal(output, output_size, offset);

    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pipe_read);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (canceled)
        return AIRY_PROCESS_RC_CANCELED;
    return timed_out ? -2 : (int)exit_code;
}

int airy_process_spawn(const char *executable, char *const argv[],
                       const airy_process_opt_t *opt, airy_process_info_t *proc)
{
    int in_pipe[2], out_pipe[2], err_pipe[2];
    int rc = spawn_prep(executable, argv, proc, in_pipe, out_pipe, err_pipe);
    if (rc != 0)
        return rc;

    STARTUPINFOA si;
    AIRY_MEMSET(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (win_spawn_stdio(opt, &si, in_pipe, out_pipe, err_pipe) != 0) {
        pipe_pair_close(in_pipe);
        pipe_pair_close(out_pipe);
        pipe_pair_close(err_pipe);
        return AIRY_ERR_IO;
    }

    char cmdline[4096];
    win_build_cmdline(executable, argv, cmdline, sizeof(cmdline));

    char *env_block = NULL;
    if (opt && opt->env) {
        env_block = win_env_block(opt->env);
        if (!env_block) {
            pipe_pair_close(in_pipe);
            pipe_pair_close(out_pipe);
            pipe_pair_close(err_pipe);
            return AIRY_ERR_IO;
        }
    }

    PROCESS_INFORMATION pi;
    AIRY_MEMSET(&pi, 0, sizeof(pi));
    const char *workdir = (opt && opt->working_dir) ? opt->working_dir : NULL;
    /* CREATE_NO_WINDOW keeps console-flash parity with start/run_capture;
     * sandbox (opt->sandbox) is a no-op on Windows by design. */
    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, env_block, workdir,
                             &si, &pi);
    if (env_block)
        free(env_block);

    /* Child ends are owned by the child now; parent keeps only its ends */
    airy_pipe_close(&in_pipe[0]);
    airy_pipe_close(&out_pipe[1]);
    airy_pipe_close(&err_pipe[1]);

    if (!ok) {
        pipe_pair_close(in_pipe);
        pipe_pair_close(out_pipe);
        pipe_pair_close(err_pipe);
        return AIRY_ERR_EXEC_FAIL;
    }

    proc->process_handle = (void *)pi.hProcess;
    proc->thread_handle = (void *)pi.hThread;
    proc->pid = pi.dwProcessId;
    proc->stdin_fd = in_pipe[1];
    proc->stdout_fd = out_pipe[0];
    proc->stderr_fd = err_pipe[0];
    return 0;
}

int airy_process_reap(airy_process_info_t *proc, airy_exit_status_t *status)
{
    if (!proc || !proc->process_handle)
        return AIRY_EINVAL;

    HANDLE h_process = (HANDLE)proc->process_handle;
    if (WaitForSingleObject(h_process, INFINITE) != WAIT_OBJECT_0)
        return AIRY_EINVAL;

    DWORD code = 1;
    if (!GetExitCodeProcess(h_process, &code))
        return AIRY_EINVAL;

    if (status) {
        status->code = (int)code;
        status->signaled = false;
        status->signal = 0;
    }

    CloseHandle(h_process);
    proc->process_handle = NULL;
    if (proc->thread_handle) {
        CloseHandle((HANDLE)proc->thread_handle);
        proc->thread_handle = NULL;
    }
    return 0;
}

#else

/* post-fork exec failure diagnostics: snprintf/strerror are unsafe in the
 * forked child of a multithreaded parent (malloc lock), so errno is
 * hand-formatted with plain write() calls; the message is captured by the
 * parent through the stderr pipe. */
static void exec_fail_diag(void)
{
    static const char hdr[] = "[airy] execvp errno=";
    char digits[12];
    int d = 0;
    int v = errno;
    /* write() results are intentionally ignored (best-effort,
     * async-signal-safe); (void) does not silence warn_unused_result,
     * so route through an ignored variable. */
    ssize_t ignored;
    ignored = write(STDERR_FILENO, hdr, sizeof(hdr) - 1);
    (void)ignored;
    if (v == 0)
        digits[d++] = '0';
    while (v > 0 && d < (int)sizeof(digits) - 1) {
        digits[d++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (d > 0) {
        ignored = write(STDERR_FILENO, &digits[--d], 1);
        (void)ignored;
    }
    ignored = write(STDERR_FILENO, "\n", 1);
    (void)ignored;
}

/* Child-side wiring between fork and exec; never returns (_exit).
 * Exit codes: 126 = chdir/sandbox failure ("found but cannot run"),
 * 127 = exec failure (errno diagnostic on stderr). */
static void spawn_child(const char *executable, char *const argv[], const airy_process_opt_t *opt,
                        int in_pipe[2], int out_pipe[2], int err_pipe[2])
{
    if (opt && opt->working_dir && chdir(opt->working_dir) != 0)
        _exit(126);

    if (in_pipe[0] >= 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
    }
    if (out_pipe[1] >= 0) {
        close(out_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(out_pipe[1]);
    }
    if (err_pipe[1] >= 0) {
        close(err_pipe[0]);
        dup2(err_pipe[1], STDERR_FILENO);
        close(err_pipe[1]);
    }

    /* R-3: spawn/start launch external system tools (git, dig,
     * docker/podman, ...). The airymaxrt launcher exports $AIRY_HOME/lib
     * through LD_LIBRARY_PATH for the runtime's own binaries; leaking that
     * into an external tool makes it load ABI-mismatched runtime .so files
     * and fail (e.g. curl: "libcurl.so.4: no version information
     * available"), which surfaces as "various tools unusable / cannot
     * reach the network". Clear it before applying env so an explicit
     * caller-provided value still wins. The daemon process keeps its own
     * LD_LIBRARY_PATH; only the child and its descendants are cleaned. */
    unsetenv("LD_LIBRARY_PATH");
    if (opt && opt->env) {
        for (int i = 0; opt->env[i]; i++)
            putenv(opt->env[i]);
    }

    /* Native sandbox (Landlock + seccomp), applied just before exec;
     * failure aborts the exec (exit 126). Non-Linux is a no-op. */
    if (opt && opt->sandbox && opt->sandbox->enabled && airy_native_sandbox_apply(opt->sandbox) != 0)
        _exit(126);

    /* flawfinder: ignore - executable and argv are caller-controlled, not arbitrary user input
     */
    execvp(executable, argv);
    exec_fail_diag();
    _exit(127);
}

int airy_process_start(const char *executable, char *const argv[], char *const envp[],
                       airy_process_info_t *proc)
{
    /* Thin wrapper over spawn: stdout+stderr piped, stdin inherited,
     * no sandbox/working_dir (historical start contract). */
    airy_process_opt_t opt;
    AIRY_MEMSET(&opt, 0, sizeof(opt));
    opt.env = envp;
    opt.redirect_stdout = 1;
    opt.redirect_stderr = 1;
    return airy_process_spawn(executable, argv, &opt, proc);
}

int airy_process_wait(airy_process_info_t *proc, uint32_t timeout_ms, int *exit_code)
{
    int status;
    pid_t result;

    if (timeout_ms == 0) {
        result = waitpid(proc->pid, &status, 0);
    } else {
        /* 单调时钟 deadline 轮询：修复 <1s 超时误报与 ts 不递减导致
         * 超时预算不准（子进程在睡眠期间退出时误报 AIRY_ERR_TIMEOUT，
         * 且子进程未被收割成僵尸）。 */
        uint64_t deadline_ms = airy_time_ms() + (uint64_t)timeout_ms;

        sigset_t mask, old_mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &old_mask);

        for (;;) {
            result = waitpid(proc->pid, &status, WNOHANG);
            if (result != 0)
                break;
            uint64_t now_ms = airy_time_ms();
            if (now_ms >= deadline_ms) {
                result = 0;
                break;
            }
            uint64_t remain_ms = deadline_ms - now_ms;
            struct timespec ts;
            ts.tv_sec = (time_t)(remain_ms / 1000);
            ts.tv_nsec = (long)((remain_ms % 1000) * 1000000);
            nanosleep(&ts, NULL);
        }

        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        if (result == 0) {
            return AIRY_ERR_TIMEOUT;
        }
    }

    if (result < 0) {
        return AIRY_EINVAL;
    }

    if (WIFEXITED(status)) {
        if (exit_code) {
            *exit_code = WEXITSTATUS(status);
        }
    } else if (WIFSIGNALED(status)) {
        if (exit_code) {
            *exit_code = -WTERMSIG(status);
        }
    }

    return 0;
}

int airy_process_kill(airy_process_info_t *proc)
{
    return kill(proc->pid, SIGKILL);
}

void airy_process_close_pipes(airy_process_info_t *proc)
{
    if (!proc)
        return;
    if (proc->stdout_fd >= 0) {
        close(proc->stdout_fd);
        proc->stdout_fd = -1;
    }
    if (proc->stderr_fd >= 0) {
        close(proc->stderr_fd);
        proc->stderr_fd = -1;
    }
    if (proc->stdin_fd >= 0) {
        close(proc->stdin_fd);
        proc->stdin_fd = -1;
    }
}

/* 把仍打开的 stdout/stderr 填入读集；两个都 EOF 时返回 -1。 */
static int posix_rfds(airy_process_info_t *proc, fd_set *rfds)
{
    int max_fd = -1;

    FD_ZERO(rfds);
    if (proc->stdout_fd >= 0) {
        FD_SET(proc->stdout_fd, rfds);
        max_fd = proc->stdout_fd;
    }
    if (proc->stderr_fd >= 0) {
        FD_SET(proc->stderr_fd, rfds);
        if (proc->stderr_fd > max_fd)
            max_fd = proc->stderr_fd;
    }
    return max_fd;
}

/* 分片等待上限 100ms；deadline 更近时收缩到剩余时间。EINTR 交由调用方
 * 重新走一轮，以便重新检查取消与超时。 */
static int posix_wait(int max_fd, fd_set *rfds, uint64_t deadline_ms)
{
    uint32_t slice_ms = 100;
    struct timeval tv;

    if (deadline_ms > 0) {
        uint64_t now = airy_time_ms();
        uint64_t remain = (deadline_ms > now) ? deadline_ms - now : 0;

        if (remain < slice_ms)
            slice_ms = (uint32_t)remain;
    }
    tv.tv_sec = slice_ms / 1000U;
    tv.tv_usec = (slice_ms % 1000U) * 1000U;
    return select(max_fd + 1, rfds, NULL, NULL, &tv);
}

/* 读一段并追加；对端关闭即摘掉该描述符，避免下一轮对 EOF fd 空转。 */
static size_t posix_take(int *fd, char *output, size_t output_size, size_t off)
{
    char buf[4096];
    ssize_t n = read(*fd, buf, sizeof(buf));

    if (n <= 0) {
        close(*fd);
        *fd = -1;
        return off;
    }
    return cap_copy(output, output_size, off, buf, (size_t)n);
}

/* 两个管道都 EOF 后唯一正常的回收点：先排空、后 waitpid。子进程已退出时
 * 管道里仍可能有未读数据，提前回收会丢输出（tool_d Test 1 间歇失败的根因）；
 * 阻塞 waitpid 同时关掉了 EOF 与回收之间的竞态窗口。 */
static int posix_reap(airy_process_info_t *proc)
{
    int status = 0;

    if (waitpid(proc->pid, &status, 0) != proc->pid)
        return -1;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return -WTERMSIG(status);
    return -1;
}

/* 一轮事件循环：判取消 → 判超时 → 等可读 → 摘取 → 判 EOF。返回非 0 表示
 * 继续下一轮；返回 0 表示已进终态，*status 带出结果。取消靠短分片 select
 * 轮询实现：cancel_token.h 目前没有注销接口，注册回调会留下悬垂栈上下文。 */
static int posix_cycle(airy_process_info_t *proc, airy_cancel_token_t *tok,
                       uint64_t deadline_ms, int *timed_out, size_t *off,
                       char *output, size_t output_size, int *status)
{
    fd_set rfds;
    int max_fd;
    int ready;

    if (tok && airy_cancel_token_is_canceled(tok)) {
        airy_process_kill(proc);
        airy_process_wait(proc, 0, status);
        *status = AIRY_PROCESS_RC_CANCELED;
        return 0;
    }
    if (!*timed_out && deadline_ms > 0 && airy_time_ms() >= deadline_ms) {
        *timed_out = 1;
        airy_process_kill(proc);
    }
    max_fd = posix_rfds(proc, &rfds);
    if (max_fd < 0) {
        *status = posix_reap(proc);
        return 0;
    }
    ready = posix_wait(max_fd, &rfds, deadline_ms);
    if (ready < 0)
        return (errno == EINTR) ? 1 : 0;
    if (ready == 0)
        return 1;
    if (proc->stdout_fd >= 0 && FD_ISSET(proc->stdout_fd, &rfds))
        *off = posix_take(&proc->stdout_fd, output, output_size, *off);
    if (proc->stderr_fd >= 0 && FD_ISSET(proc->stderr_fd, &rfds))
        *off = posix_take(&proc->stderr_fd, output, output_size, *off);
    return 1;
}

int airy_process_run_capture_ex(const char *executable, char *const argv[], char *const envp[],
                                uint32_t timeout_ms, char *output, size_t output_size,
                                airy_cancel_token_t *cancel_token)
{
    airy_process_info_t proc;
    size_t offset = 0;
    uint64_t deadline_ms;
    int timed_out = 0;
    int status = -1;

    if (airy_process_start(executable, argv, envp, &proc) != 0)
        return AIRY_ERR_EXEC_FAIL;
    cap_seal(output, output_size, 0);
    deadline_ms = (timeout_ms > 0) ? airy_time_ms() + (uint64_t)timeout_ms : 0;

    while (posix_cycle(&proc, cancel_token, deadline_ms, &timed_out,
                       &offset, output, output_size, &status))
        ;
    airy_process_close_pipes(&proc);
    cap_seal(output, output_size, offset);
    return timed_out ? -2 : status;
}

int airy_process_spawn(const char *executable, char *const argv[],
                       const airy_process_opt_t *opt, airy_process_info_t *proc)
{
    int in_pipe[2], out_pipe[2], err_pipe[2];
    int rc = spawn_prep(executable, argv, proc, in_pipe, out_pipe, err_pipe);
    if (rc != 0)
        return rc;

    if (opt && opt->redirect_stdin && airy_pipe_create(in_pipe) != 0)
        return AIRY_ERR_IO;
    if (opt && opt->redirect_stdout && airy_pipe_create(out_pipe) != 0)
        goto fail;
    if (opt && opt->redirect_stderr && airy_pipe_create(err_pipe) != 0)
        goto fail;

    pid_t pid = fork();
    if (pid < 0)
        goto fail;
    if (pid == 0)
        spawn_child(executable, argv, opt, in_pipe, out_pipe, err_pipe);

    /* parent: drop child ends, keep parent ends */
    airy_pipe_close(&in_pipe[0]);
    airy_pipe_close(&out_pipe[1]);
    airy_pipe_close(&err_pipe[1]);

    proc->pid = pid;
    proc->stdin_fd = in_pipe[1];
    proc->stdout_fd = out_pipe[0];
    proc->stderr_fd = err_pipe[0];
    return 0;

fail:
    pipe_pair_close(in_pipe);
    pipe_pair_close(out_pipe);
    pipe_pair_close(err_pipe);
    return AIRY_ERR_IO;
}

int airy_process_reap(airy_process_info_t *proc, airy_exit_status_t *status)
{
    if (!proc || proc->pid < 0)
        return AIRY_EINVAL;
    int s = 0;
    pid_t ret;
    do {
        ret = waitpid(proc->pid, &s, 0);
    } while (ret < 0 && errno == EINTR);
    if (ret != proc->pid)
        return AIRY_EINVAL;
    if (status) {
        status->signaled = WIFSIGNALED(s) ? true : false;
        status->signal = WIFSIGNALED(s) ? WTERMSIG(s) : 0;
        status->code = WIFEXITED(s) ? WEXITSTATUS(s) : 0;
    }
    proc->pid = (airy_pid_t)-1;
    return 0;
}

#endif

/* Platform-independent: both back ends capture via *_ex; hoisted out of the
 * conditional so the delegate is defined once instead of per platform. */
int airy_process_run_capture(const char *executable, char *const argv[], char *const envp[],
                             uint32_t timeout_ms, char *output, size_t output_size)
{
    return airy_process_run_capture_ex(executable, argv, envp, timeout_ms, output, output_size,
                                       NULL);
}
