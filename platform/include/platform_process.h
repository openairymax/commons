/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file platform_process.h
 * @brief Cross-platform compatibility layer - thread / socket / process API
 *
 * Thread lifecycle, socket primitives and child-process management.
 * Domain split of platform.h (2026-08-27).
 *
 * @see platform.h aggregate entry
 */

#ifndef AIRY_RT_PLATFORM_PROCESS_H
#define AIRY_RT_PLATFORM_PROCESS_H

#include "platform_base.h"
#include "platform_sandbox.h"


#ifdef __cplusplus
extern "C" {
#endif


/* ==================== Thread API ==================== */

/**
 * @brief Thread function type
 */
typedef void *(*airy_thread_func_t)(void *arg);

/**
 * @brief Create a thread
 * @param thread thread handle pointer
 * @param func thread function
 * @param arg thread argument
 * @return 0 on success, non-zero on failure
 */
int airy_platform_thread_create(airy_thread_t *thread, airy_thread_func_t func, void *arg);

/**
 * @brief Wait for a thread to finish
 * @param thread thread handle
 * @param retval return value pointer (may be NULL)
 * @return 0 on success, non-zero on failure
 */
int airy_platform_thread_join(airy_thread_t thread, void **retval);

/**
 * @brief Detach a thread (resources reclaimed automatically on exit)
 * @param thread thread handle
 * @return 0 on success, non-zero on failure
 */
int airy_platform_thread_detach(airy_thread_t thread);

#ifndef AIRY_USE_SCHEDULER_THREAD_IMPL
#define airy_thread_create airy_platform_thread_create
#define airy_thread_join airy_platform_thread_join
#define airy_thread_detach airy_platform_thread_detach
#endif

/**
 * @brief Set the current thread's name (observability / debugging)
 *
 * Linux pthread_setname_np (truncated to 15 chars by the kernel),
 * macOS pthread_setname_np, Windows SetThreadDescription.
 *
 * @param name thread name (NUL-terminated, copied internally)
 * @return 0 on success, non-zero on failure
 */
int airy_thread_set_name(const char *name);

/**
 * @brief Get the current thread ID
 * @return thread ID
 */
uint64_t airy_thread_id(void);


/* ==================== Socket API ==================== */

/**
 * @brief Create a TCP socket
 * @return socket handle, AIRY_INVALID_SOCKET on failure
 */
airy_sock_t airy_sock_tcp(void);

/**
 * @brief Create a Unix domain socket (POSIX only)
 * @return socket handle, AIRY_INVALID_SOCKET on failure
 */
airy_sock_t airy_sock_unix(void);

/**
 * @brief Close a socket
 * @param sock socket handle
 */
void airy_sock_close(airy_sock_t sock);

/**
 * @brief Set socket non-blocking mode
 * @param sock socket handle
 * @param nonblock non-blocking flag
 * @return 0 on success, non-zero on failure
 */
int airy_sock_set_nonblock(airy_sock_t sock, int nonblock);

/**
 * @brief Set socket address reuse
 * @param sock socket handle
 * @param reuse reuse flag
 * @return 0 on success, non-zero on failure
 */
int airy_sock_set_reuseaddr(airy_sock_t sock, int reuse);


/* ==================== Process API ==================== */

/* Forward declaration: cancellation token (improvement 1 "cancellation
 * push-down"). Full definition in
 * commons/utils/sync/cancel_token.h (that header includes
 * platform.h, so only a pointer parameter is declared here to avoid a
 * circular dependency). */
struct airy_cancel_token;
typedef struct airy_cancel_token airy_cancel_token_t;

/**
 * @brief Process info structure
 */
typedef struct {
    airy_pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
#if AIRY_PLATFORM_WINDOWS
    /* Child process handles are stored in the struct to support concurrent
     * multi-process and thread safety, replacing the previous global
     * variables (single-process limitation). void* avoids pulling in
     * windows.h here. */
    void *process_handle; /* HANDLE to the child process */
    void *thread_handle; /* HANDLE to the child's primary thread */
#endif
} airy_process_info_t;

/**
 * @brief Start a process
 * @param executable executable file path
 * @param argv argument array (NULL-terminated)
 * @param envp environment array (NULL-terminated, may be NULL)
 * @param proc output process info
 * @return 0 on success, non-zero on failure
 */
int airy_process_start(const char *executable, char *const argv[], char *const envp[],
                       airy_process_info_t *proc);

/**
 * @brief Wait for a process to finish
 * @param proc process info
 * @param timeout_ms timeout in milliseconds, 0 means wait indefinitely
 * @param exit_code output exit code (may be NULL)
 * @return 0 on success, non-zero on failure or timeout
 */
int airy_process_wait(airy_process_info_t *proc, uint32_t timeout_ms, int *exit_code);

/**
 * @brief Terminate a process
 * @param proc process info
 * @return 0 on success, non-zero on failure
 */
int airy_process_kill(airy_process_info_t *proc);

/**
 * @brief Close process pipes
 * @param proc process info
 */
void airy_process_close_pipes(airy_process_info_t *proc);

/**
 * @brief Run a command and capture merged output (high-level convenience API)
 *
 * Internally completes the full start -> read merged stdout+stderr -> wait
 * -> close flow. The POSIX path uses fork + execvp (no shell, no command
 * injection risk).
 *
 * @param executable executable file path (searched in PATH by execvp)
 * @param argv       argument array (NULL-terminated, argv[0] is usually the program name)
 * @param envp       environment array (NULL-terminated, NULL means inherit)
 * @param timeout_ms timeout in milliseconds, 0 means wait indefinitely
 * @param output     output buffer (NULL means don't capture, but pipes are
 *                   still drained to prevent the child from blocking)
 * @param output_size output buffer size (including '\0')
 * @return exit code (0-255); -1 = launch failure; -2 = timeout
 */
int airy_process_run_capture(const char *executable, char *const argv[], char *const envp[],
                             uint32_t timeout_ms, char *output, size_t output_size);

/**
 * @brief Event-source-driven cancellable command execution
 *        (improvement 1 "tool_d event-source-driven")
 *
 * Same semantics as airy_process_run_capture, but the wait mechanism is
 * event-source-driven instead of select blocking polling + blocking waitpid:
 *   - A self-built wake pipe serves as the event source: the cancel-token
 *     callback writes a wakeup on match, select returns immediately, no
 *     1s-granularity polling needed;
 *   - Child exit is detected via pipe EOF, and waitpid WNOHANG reaps the
 *     child non-blockingly (replacing the blocking waitpid after the loop);
 *   - Timeout is precise to the millisecond (monotonic-clock deadline).
 *
 * @param cancel_token cancellation token (may be NULL = same as airy_process_run_capture)
 * @return exit code (0-255); -1 = launch failure; -2 = timeout;
 *         -3 = canceled (AIRY_PROCESS_RC_CANCELED)
 */
int airy_process_run_capture_ex(const char *executable, char *const argv[], char *const envp[],
                                uint32_t timeout_ms, char *output, size_t output_size,
                                airy_cancel_token_t *cancel_token);


#define AIRY_PROCESS_RC_CANCELED (-3)


/* ==================== Extended spawn API (sandbox-aware) ==================== */

/**
 * @brief Child process exit status
 */
typedef struct {
    int code;      /* exit code (valid when signaled == false) */
    bool signaled; /* true = terminated by a signal */
    int signal;    /* signal number (valid when signaled == true) */
} airy_exit_status_t;

/**
 * @brief Extended spawn options
 *
 * opt == NULL: the child inherits the parent's stdio (no redirection)
 * and environment, runs in the parent's working directory, no sandbox.
 *
 * Requested pipes are created internally by airy_process_spawn; their
 * parent-side fds are stored in proc->stdin_fd / stdout_fd / stderr_fd
 * (stdin keeps the write end, stdout/stderr keep the read end, unused
 * fds stay -1). The caller owns these fds and closes them via
 * airy_pipe_close or airy_process_close_pipes.
 */
typedef struct {
    const char *working_dir;       /* NULL = inherit parent cwd */
    char *const *env;              /* NULL-terminated, NULL = inherit */
    int redirect_stdin;            /* 1 = wire a pipe to child stdin */
    int redirect_stdout;           /* 1 = wire a pipe to child stdout */
    int redirect_stderr;           /* 1 = wire a pipe to child stderr */
    const airy_native_sandbox_t *sandbox; /* NULL = none; applied pre-exec on POSIX */
} airy_process_opt_t;

/**
 * @brief Spawn a child process with stdio redirection and optional sandbox
 *
 * POSIX: fork + execvp (PATH search preserved even with a custom env).
 * On Linux the sandbox (Landlock + seccomp, see platform_sandbox.h) is
 * applied in the child before exec; sandbox failure exits 126.
 *
 * @param executable executable file path (searched in PATH)
 * @param argv argument array (NULL-terminated, argv[0] is the program name)
 * @param opt options (may be NULL)
 * @param proc output process info
 * @return 0 on success, non-zero on failure
 */
int airy_process_spawn(const char *executable, char *const argv[],
                       const airy_process_opt_t *opt, airy_process_info_t *proc);

/**
 * @brief Reap a spawned process and decode its exit status
 *
 * Blocks until the child exits, then releases the process resources and
 * resets proc so a double reap is rejected. Pipe fds are not touched.
 *
 * @param proc process info
 * @param status output exit status (may be NULL)
 * @return 0 on success, non-zero on failure
 */
int airy_process_reap(airy_process_info_t *proc, airy_exit_status_t *status);


/* ==================== Pipe API ==================== */

/**
 * @brief Create an anonymous pipe
 * @param fds receives two fds: fds[0] = read end, fds[1] = write end
 * @return 0 on success, -1 on failure
 */
int airy_pipe_create(int fds[2]);

/**
 * @brief Close one pipe fd and set it to -1 (NULL-safe)
 *
 * A pipe fd is unidirectional, so closing the single fd is exactly the
 * half-close of that direction.
 *
 * @param fd pointer to the fd
 */
void airy_pipe_close(int *fd);

/**
 * @brief Read from a pipe fd
 * @return bytes read, 0 on EOF (all write ends closed), -1 on error
 */
long airy_pipe_read(int fd, void *buf, size_t len);

/**
 * @brief Write to a pipe fd (full write: retries until all bytes are written)
 * @return 0 on success, -1 on error
 */
int airy_pipe_write(int fd, const void *buf, size_t len);


#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_PROCESS_H */
