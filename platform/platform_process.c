// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file platform_process.c
 * @brief 进程管理域：进程启动/等待/终止、管道关闭与命令输出捕获的跨平台实现
 */

#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#define strdup _strdup
#define access _access /* flawfinder: ignore */
#ifndef EEXIST
#define EEXIST 17
#endif
#pragma comment(lib, "bcrypt.lib")
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "error.h"
#include "platform.h"
#include "cancel_token.h"

#include "airy_memory.h"

#if AIRY_PLATFORM_WINDOWS

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

    char cmdline[4096] = {0};
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", executable);
    for (int i = 1; argv && argv[i]; i++) {
        size_t remaining = sizeof(cmdline) - strlen(cmdline);
        if (remaining > 0) {
            snprintf(cmdline + strlen(cmdline), remaining, " \"%s\"", argv[i]);
        }
    }

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

int airy_process_run_capture(const char *executable, char *const argv[], char *const envp[],
                             uint32_t timeout_ms, char *output, size_t output_size)
{
    (void)envp;
    (void)timeout_ms;
    /* BAN-211/235: 使用 CreateProcess + 匿名管道替代 _popen（消除 cmd.exe 注入风险）。
     * CreateProcess 直接解析命令行，不经 shell，行为对齐 POSIX 的 fork/execvp。
     * 与 market_service_impl.c 的 win_run_command 安全模式一致。 */
    char cmdline[4096] = {0};
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", executable);
    for (int i = 1; argv && argv[i]; i++) {
        size_t remaining = sizeof(cmdline) - strlen(cmdline);
        if (remaining > 0)
            snprintf(cmdline + strlen(cmdline), remaining, " \"%s\"", argv[i]);
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    HANDLE pipe_read = NULL, pipe_write = NULL;
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0)) {
        return AIRY_ERR_IO;
    }

    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = pipe_write;
    si.hStdError = pipe_write;
    si.hStdInput = NULL;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(pipe_write);
    if (!ok) {
        CloseHandle(pipe_read);
        return AIRY_ERR_EXEC_FAIL;
    }

    size_t offset = 0;
    if (output && output_size > 0) {
        char buf[4096];
        DWORD bytes_read;
        while (ReadFile(pipe_read, buf, sizeof(buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
            size_t len = (size_t)bytes_read;
            if (offset + len >= output_size)
                len = output_size - 1 - offset;
            __builtin_memcpy(output + offset, buf, len);
            offset += len;
            if (offset + 1 >= output_size)
                break;
        }
        output[offset] = '\0';
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pipe_read);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)exit_code;
}

int airy_process_run_capture_ex(const char *executable, char *const argv[], char *const envp[],
                                uint32_t timeout_ms, char *output, size_t output_size,
                                airy_cancel_token_t *cancel_token)
{
    (void)cancel_token;
    return airy_process_run_capture(executable, argv, envp, timeout_ms, output, output_size);
}

#else

int airy_process_start(const char *executable, char *const argv[], char *const envp[],
                       airy_process_info_t *proc)
{
    if (!proc)
        return AIRY_EINVAL;
    AIRY_MEMSET(proc, 0, sizeof(airy_process_info_t));
    proc->stdin_fd = -1;
    proc->stdout_fd = -1;
    proc->stderr_fd = -1;

    int stdout_pipe[2];
    int stderr_pipe[2];
    if (pipe(stdout_pipe) < 0)
        return AIRY_EINVAL;
    if (pipe(stderr_pipe) < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return AIRY_EINVAL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return AIRY_EINVAL;
    }

    if (pid == 0) {

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (envp) {
            for (int i = 0; envp[i]; i++) {
                putenv(envp[i]);
            }
        }
        /* flawfinder: ignore - executable and argv are caller-controlled, not arbitrary user input
         */
        execvp(executable, argv);
        _exit(127);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    proc->pid = pid;
    proc->stdout_fd = stdout_pipe[0];
    proc->stderr_fd = stderr_pipe[0];
    return 0;
}

int airy_process_wait(airy_process_info_t *proc, uint32_t timeout_ms, int *exit_code)
{
    int status;
    pid_t result;

    if (timeout_ms == 0) {
        result = waitpid(proc->pid, &status, 0);
    } else {
        struct timespec ts;
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;

        sigset_t mask, old_mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &old_mask);

        do {
            result = waitpid(proc->pid, &status, WNOHANG);
            if (result == 0) {
                nanosleep(&ts, NULL);
            }
        } while (result == 0 && ts.tv_sec > 0);

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

int airy_process_run_capture(const char *executable, char *const argv[], char *const envp[],
                             uint32_t timeout_ms, char *output, size_t output_size)
{
    return airy_process_run_capture_ex(executable, argv, envp, timeout_ms, output, output_size,
                                       NULL);
}

// 取消经 select 短片轮询实现（cancel_token.h 暂无 unregister 接口，避免栈 ctx 悬垂）
int airy_process_run_capture_ex(const char *executable, char *const argv[], char *const envp[],
                                uint32_t timeout_ms, char *output, size_t output_size,
                                airy_cancel_token_t *cancel_token)
{
    airy_process_info_t proc;
    if (airy_process_start(executable, argv, envp, &proc) != 0)
        return AIRY_ERR_EXEC_FAIL;

    size_t offset = 0;
    if (output && output_size > 0)
        output[0] = '\0';

    uint64_t deadline_ms = (timeout_ms > 0) ? airy_time_ms() + (uint64_t)timeout_ms : 0;
    int timed_out = 0;
    int canceled = 0;
    int exit_code = -1;

    for (;;) {

        if (cancel_token && airy_cancel_token_is_canceled(cancel_token)) {
            canceled = 1;
            airy_process_kill(&proc);
        }

        if (canceled) {
            airy_process_wait(&proc, 0, &exit_code);
            airy_process_close_pipes(&proc);
            if (output && output_size > 0)
                output[offset] = '\0';
            return AIRY_PROCESS_RC_CANCELED;
        }
        int st = 0;
        pid_t wr = waitpid(proc.pid, &st, WNOHANG);
        if (wr == proc.pid) {

            if (WIFEXITED(st))
                exit_code = WEXITSTATUS(st);
            else if (WIFSIGNALED(st))
                exit_code = -WTERMSIG(st);

            break;
        }

        if (deadline_ms > 0 && airy_time_ms() >= deadline_ms) {
            timed_out = 1;
            airy_process_kill(&proc);
            continue;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        int max_fd = -1;
        if (proc.stdout_fd >= 0) {
            FD_SET(proc.stdout_fd, &rfds);
            if (proc.stdout_fd > max_fd)
                max_fd = proc.stdout_fd;
        }
        if (proc.stderr_fd >= 0) {
            FD_SET(proc.stderr_fd, &rfds);
            if (proc.stderr_fd > max_fd)
                max_fd = proc.stderr_fd;
        }
        if (max_fd < 0) {
            /* 所有管道已 EOF：子进程已退出（管道写端随子进程关闭）。
             * 必须阻塞回收，消除 EOF 与 waitpid 之间的竞态窗口——否则
             * 窗口内 break 会以 exit_code=-1 误报"启动失败"（间歇性 heisenbug，
             * 见 tool_d test_sandbox_integration Test 1 随机失败）。 */
            int eof_status = 0;
            if (waitpid(proc.pid, &eof_status, 0) == proc.pid) {
                if (WIFEXITED(eof_status))
                    exit_code = WEXITSTATUS(eof_status);
                else if (WIFSIGNALED(eof_status))
                    exit_code = -WTERMSIG(eof_status);
            }
            break;
        }

        uint32_t slice_ms = 100;
        if (deadline_ms > 0) {
            uint64_t remain = deadline_ms - airy_time_ms();
            if (remain < slice_ms)
                slice_ms = (uint32_t)remain;
        }
        struct timeval tv;
        tv.tv_sec = slice_ms / 1000U;
        tv.tv_usec = (slice_ms % 1000U) * 1000U;
        int retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);
        if (retval < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (retval > 0) {
            char buf[4096];
            if (proc.stdout_fd >= 0 && FD_ISSET(proc.stdout_fd, &rfds)) {
                ssize_t n = read(proc.stdout_fd, buf, sizeof(buf));
                if (n <= 0) {
                    close(proc.stdout_fd);
                    proc.stdout_fd = -1;
                } else if (output && offset + 1 < output_size) {
                    size_t copy =
                        (offset + (size_t)n < output_size) ? (size_t)n : output_size - 1 - offset;
                    __builtin_memcpy(output + offset, buf, copy);
                    offset += copy;
                }
            }
            if (proc.stderr_fd >= 0 && FD_ISSET(proc.stderr_fd, &rfds)) {
                ssize_t n = read(proc.stderr_fd, buf, sizeof(buf));
                if (n <= 0) {
                    close(proc.stderr_fd);
                    proc.stderr_fd = -1;
                } else if (output && offset + 1 < output_size) {
                    size_t copy =
                        (offset + (size_t)n < output_size) ? (size_t)n : output_size - 1 - offset;
                    __builtin_memcpy(output + offset, buf, copy);
                    offset += copy;
                }
            }
        }
    }

    airy_process_close_pipes(&proc);

    if (output && output_size > 0)
        output[offset] = '\0';
    if (canceled)
        return AIRY_PROCESS_RC_CANCELED;
    return timed_out ? -2 : exit_code;
}

#endif
