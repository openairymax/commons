/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file platform.h
 * @brief Cross-platform compatibility layer - unifies OS API differences
 *
 * Supported platforms:
 * - Linux (POSIX)
 * - macOS (Darwin)
 * - Windows (Win32/Win64)
 *
 * Design principles:
 * - Single responsibility: handle platform differences only
 * - Zero overhead: inline functions + macro definitions
 * - Type safety: strongly-typed wrappers
 *
 * @note Thread safety: the platform abstraction layer itself does not
 *       involve thread safety
 * @see ARCHITECTURAL_PRINCIPLES.md E-4 cross-platform consistency principle
 */

#ifndef AIRY_RT_PLATFORM_H
#define AIRY_RT_PLATFORM_H

#include <compat.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#if defined(_WIN32) || defined(_WIN64)
#define AIRY_PLATFORM_WINDOWS 1
#define AIRY_PLATFORM_NAME "Windows"
#if defined(_WIN64)
#define AIRY_PLATFORM_BITS 64
#else
#define AIRY_PLATFORM_BITS 32
#endif
#define AIRY_PLATFORM_POSIX 0
#elif defined(__APPLE__) && defined(__MACH__)
#define AIRY_PLATFORM_MACOS 1
#define AIRY_PLATFORM_NAME "macOS"
#define AIRY_PLATFORM_BITS 64
#define AIRY_PLATFORM_POSIX 1
#elif defined(__linux__)
#define AIRY_PLATFORM_LINUX 1
#define AIRY_PLATFORM_NAME "Linux"
/* 位宽判定用标准宏而非架构枚举：UINTPTR_MAX==UINT64_MAX 覆盖
 * x86_64/aarch64/riscv64/ppc64 等全部 64 位架构（此前 riscv64 未列入
 * x86_64/aarch64 枚举，被判为 32 位，导致指针/原子/地址类型错配）。 */
#if UINTPTR_MAX == UINT64_MAX
#define AIRY_PLATFORM_BITS 64
#else
#define AIRY_PLATFORM_BITS 32
#endif
#define AIRY_PLATFORM_POSIX 1
#else
#error "Unsupported platform"
#endif


#include "export.h"


#if defined(_WIN32) || defined(_WIN64)
#define AIRY_THREAD_LOCAL __declspec(thread)
#else
#define AIRY_THREAD_LOCAL __thread
#endif


#if defined(_WIN32) || defined(_WIN64)
#ifndef AIRY_INLINE
#define AIRY_INLINE __forceinline
#endif
#else
#define AIRY_INLINE static inline __attribute__((always_inline))
#endif


#ifndef AIRY_UNUSED
#define AIRY_UNUSED(x) ((void)(x))
#endif


#if AIRY_PLATFORM_WINDOWS
#define AIRY_PATH_SEP '\\'
#define AIRY_PATH_SEP_STR "\\"
#define AIRY_PATH_MAX 260
#else
#define AIRY_PATH_SEP '/'
#define AIRY_PATH_SEP_STR "/"
#define AIRY_PATH_MAX 4096
#endif


#if AIRY_PLATFORM_WINDOWS

#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "C:\\ProgramData\\agentrt\\run"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "C:\\ProgramData\\agentrt\\logs"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "C:\\ProgramData\\agentrt\\config"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "C:\\ProgramData\\agentrt\\data"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "C:\\ProgramData\\agentrt\\tmp"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "C:\\ProgramData\\agentrt\\cache"
#endif
#elif AIRY_PLATFORM_LINUX

#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "/tmp/agentrt"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "/var/log/agentrt"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "/etc/agentrt"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "/var/lib/agentrt"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "/var/tmp/agentrt"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "/var/cache/agentrt"
#endif
#else

#ifndef AIRY_RUNTIME_DIR
#define AIRY_RUNTIME_DIR "./agentrt/run"
#endif
#ifndef AIRY_LOG_DIR
#define AIRY_LOG_DIR "./agentrt/logs"
#endif
#ifndef AIRY_CONFIG_DIR
#define AIRY_CONFIG_DIR "./agentrt/config"
#endif
#ifndef AIRY_DATA_DIR
#define AIRY_DATA_DIR "./agentrt/data"
#endif
#ifndef AIRY_TMP_DIR
#define AIRY_TMP_DIR "./agentrt/tmp"
#endif
#ifndef AIRY_CACHE_DIR
#define AIRY_CACHE_DIR "./agentrt/cache"
#endif
#endif


#if AIRY_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h> /* time_t / struct tm（airy_localtime_r） */
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#endif


#if AIRY_PLATFORM_WINDOWS
typedef HANDLE airy_thread_t;
typedef DWORD airy_thread_id_t;
typedef CRITICAL_SECTION airy_mtx_t;
typedef CONDITION_VARIABLE airy_cond_t;
typedef DWORD airy_pid_t;
typedef SOCKET airy_sock_t;
typedef HANDLE airy_process_t;
#else
typedef pthread_t airy_thread_t;
typedef pthread_t airy_thread_id_t;
typedef pthread_mutex_t airy_mtx_t;
typedef pthread_cond_t airy_cond_t;
typedef pid_t airy_pid_t;
typedef int airy_sock_t;
typedef pid_t airy_process_t;
#endif

#define AIRY_INVALID_THREAD ((airy_thread_t)0)
#define AIRY_INVALID_MUTEX ((airy_mtx_t){0})
#define AIRY_INVALID_SOCKET (-1)
#define AIRY_INVALID_PROCESS ((airy_process_t)0)


/**
 * @brief Initialize a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_mtx_init(airy_mtx_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex mutex pointer
 */
void airy_mtx_destroy(airy_mtx_t *mutex);

/**
 * @brief Lock a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_mtx_lock(airy_mtx_t *mutex);

/**
 * @brief Try to lock a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure or already locked
 */
int airy_mtx_trylock(airy_mtx_t *mutex);

/**
 * @brief Unlock a mutex
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_mtx_unlock(airy_mtx_t *mutex);

/**
 * @brief Dynamically create a mutex (allocate and initialize)
 * @return mutex pointer, NULL on failure
 */
airy_mtx_t *airy_mtx_create(void);

/**
 * @brief Dynamically destroy a mutex (destroy and free memory)
 * @param mutex mutex pointer
 */
void airy_mtx_free(airy_mtx_t *mutex);


/**
 * @brief Initialize a condition variable
 * @param cond condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_init(airy_cond_t *cond);

/**
 * @brief Destroy a condition variable
 * @param cond condition variable pointer
 */
void airy_cond_destroy(airy_cond_t *cond);

/**
 * @brief Wait on a condition variable
 * @param cond condition variable pointer
 * @param mutex mutex pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_wait(airy_cond_t *cond, airy_mtx_t *mutex);

/**
 * @brief Timed wait on a condition variable
 * @param cond condition variable pointer
 * @param mutex mutex pointer
 * @param timeout_ms timeout in milliseconds
 * @return 0 on success, non-zero on failure or timeout
 */
int airy_cond_timedwait(airy_cond_t *cond, airy_mtx_t *mutex, uint32_t timeout_ms);

/**
 * @brief Wake up one waiting thread
 * @param cond condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_signal(airy_cond_t *cond);

/**
 * @brief Wake up all waiting threads
 * @param cond condition variable pointer
 * @return 0 on success, non-zero on failure
 */
int airy_cond_broadcast(airy_cond_t *cond);

/**
 * @brief Dynamically create a condition variable (allocate and initialize)
 * @return condition variable pointer, NULL on failure
 */
airy_cond_t *airy_cond_create(void);

/**
 * @brief Dynamically destroy a condition variable (destroy and free memory)
 * @param cond condition variable pointer
 */
void airy_cond_free(airy_cond_t *cond);


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
 * @brief Get the current thread ID
 * @return thread ID
 */
uint64_t airy_thread_id(void);


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


/* Forward declaration: cancellation token (improvement 1 "cancellation
 * push-down"). Full definition in
 * commons/utils/sync/include/cancel_token.h (that header includes
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
 * injection risk, BAN-211/235 compliant).
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


/**
 * @brief Get high-precision timestamp (nanoseconds)
 * @return timestamp
 */
uint64_t airy_time_ns(void);

/**
 * @brief Get current timestamp (milliseconds)
 * @return timestamp
 */
uint64_t airy_time_ms(void);

/**
 * @brief Sleep for the given number of milliseconds
 * @param ms milliseconds
 */
void airy_sleep_ms(uint32_t ms);

/**
 * @brief Thread-safe local time conversion (localtime_r/localtime_s unified)
 * @param timep pointer to the time to convert
 * @param result buffer for the broken-down time
 * @return 0 on success, -1 on failure
 */
int airy_localtime_r(const time_t *timep, struct tm *result);


/**
 * @brief Initialize the random number generator (thread-safe)
 */
void airy_random_init(void);

/**
 * @brief Generate a random number (thread-safe)
 * @param min minimum value
 * @param max maximum value
 * @return random number
 */
uint32_t airy_random_uint32(uint32_t min, uint32_t max);

/**
 * @brief Generate a random float (thread-safe)
 * @return random number between 0.0 and 1.0
 */
float airy_random_float(void);

/**
 * @brief Generate random bytes (thread-safe)
 * @param buf buffer
 * @param len length
 * @return 0 on success, non-zero on failure
 */
int airy_random_bytes(void *buf, size_t len);


/**
 * @brief Check whether a file exists
 * @param path file path
 * @return 1 exists, 0 not exists
 */
int airy_file_exists(const char *path);

/**
 * @brief Create a directory (recursive)
 * @param path directory path
 * @return 0 on success, non-zero on failure
 */
int airy_mkdir_p(const char *path);

/**
 * @brief Get file size
 * @param path file path
 * @return file size, -1 on failure
 */
int64_t airy_file_size(const char *path);


/**
 * @brief Initialize the network library (required on Windows)
 * @return 0 on success
 */
int airy_network_init(void);

/**
 * @brief Clean up the network library (required on Windows)
 */
void airy_network_cleanup(void);


/**
 * @brief Ignore SIGPIPE signal
 */
void airy_ignore_sigpipe(void);


/**
 * @brief Safe string copy
 * @param dest destination buffer
 * @param src source string
 * @param dest_size destination buffer size
 * @return 0 on success, non-zero on failure
 */
int airy_strlcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief Safe string concatenation
 * @param dest destination buffer
 * @param src source string
 * @param dest_size destination buffer size
 * @return 0 on success, non-zero on failure
 */
int airy_strlcat(char *dest, const char *src, size_t dest_size);


/**
 * @brief Get the last error code
 * @return error code
 */
int airy_get_last_error(void);

/**
 * @brief Get error description string
 * @param error error code
 * @return error description string
 */
const char *airy_strerror(int error);


#ifndef AIRY_SYSINFO_T_DEFINED
#define AIRY_SYSINFO_T_DEFINED
typedef struct {
    char os_name[64];
    char os_version[64];
    char hostname[64];
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} airy_sysinfo_t;
#endif

int airy_get_sysinfo(airy_sysinfo_t *info);


#ifndef AIRY_ATOMIC_INT_T_DEFINED
#define AIRY_ATOMIC_INT_T_DEFINED
#include "atomic_compat.h"
typedef atomic_int airy_atomic_int_t;
#endif

int airy_atomic_load(airy_atomic_int_t *atomic);
void airy_atomic_store(airy_atomic_int_t *atomic, int value);
int airy_atomic_fetch_add(airy_atomic_int_t *atomic, int value);
int airy_atomic_fetch_sub(airy_atomic_int_t *atomic, int value);


/* d8 cleanup: migrated from sync_compat.h to platform.h (the RAII guard
 * depends on airy_mtx_lock/unlock, logically a helper of the platform.h
 * API). Removes sync_compat.h's compatibility-layer positioning. */

/**
 * @defgroup mutex_guard RAII mutex guard (P0.18.3)
 * @{
 *
 * AIRY_MUTEX_LOCK_GUARD combines mutex lock + automatic unlock on scope
 * exit, eliminating manual lock/unlock pairing boilerplate.
 *
 * Based on GCC/Clang __attribute__((cleanup)) for RAII semantics.
 * The guard tracks lock state and only unlocks in cleanup when the lock
 * was acquired, avoiding unlock on an unheld mutex. MSVC falls back to
 * lock-only (manual unlock required).
 *
 * Usage:
 *   AIRY_MUTEX_LOCK_GUARD(m);
 *
 *
 * Note: the macro does not check whether locking succeeded. If locking can
 * fail (e.g. deadlock detection), use airy_mtx_lock + manual if check +
 * airy_mtx_unlock.
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief Mutex guard type - tracks lock state
 *
 * Stores the mutex pointer and lock state, used to decide at cleanup
 * whether to unlock.
 */
typedef struct {
    airy_mtx_t *mutex;
    bool acquired;
} airy_mtx_guard_t;

/**
 * @brief Mutex guard cleanup function (auto-invoked by the cleanup attribute)
 *
 * Automatically called when a variable marked with AIRY_MUTEX_LOCK_GUARD
 * leaves scope. Unlocks only when acquired is true and mutex is non-NULL,
 * avoiding unlock on an unheld mutex.
 *
 * @param g pointer to the guard variable
 */
static inline void airy_mtx_guard_cleanup(airy_mtx_guard_t *g)
{
    if (g->acquired && g->mutex) {
        airy_mtx_unlock(g->mutex);
        g->acquired = false;
        g->mutex = NULL;
    }
}

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII mutex guard: lock + auto-unlock on scope exit
 *
 * @param m mutex variable (airy_mtx_t type; the macro takes its address &m)
 *
 * Usage example:
 *   static airy_mtx_t my_lock;
 *   airy_mtx_init(&my_lock);
 *   {
 *       AIRY_MUTEX_LOCK_GUARD(my_lock);
 *
 *   }
 *
 * @note Uses __COUNTER__ to generate a unique variable name, so the macro
 *       can be used multiple times in the same scope
 * @note On lock failure acquired=false, cleanup will not unlock; subsequent
 *       code runs in the unlocked state
 */
#define AIRY_MUTEX_LOCK_GUARD_(m, counter)                                                  \
    airy_mtx_guard_t __attribute__((cleanup(airy_mtx_guard_cleanup))) __guard_##counter = { \
        .mutex = &(m), .acquired = (airy_mtx_lock(&(m)) == 0)}

/* Two levels of indirection: force __COUNTER__ to expand to a number before
 * ## concatenation, avoiding variable name collisions. A direct
 * AIRY_MUTEX_LOCK_GUARD_(m, __COUNTER__) would paste into
 * __guard___COUNTER__ (a literal, not expanded), causing name collisions
 * when used multiple times in the same scope. */
#define AIRY_MUTEX_LOCK_GUARD_EXPAND(m, counter) AIRY_MUTEX_LOCK_GUARD_(m, counter)
#define AIRY_MUTEX_LOCK_GUARD(m) AIRY_MUTEX_LOCK_GUARD_EXPAND(m, __COUNTER__)

#elif defined(_MSC_VER)

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII mutex guard (MSVC -- falls back to lock-only, manual unlock
 *        required)
 *
 * MSVC does not support __attribute__((cleanup)); this macro only locks.
 * With MSVC, airy_mtx_unlock must be called manually before returning.
 */
#define AIRY_MUTEX_LOCK_GUARD(m) ((void)airy_mtx_lock(&(m)))

#else

/**
 * @def AIRY_MUTEX_LOCK_GUARD(m)
 * @brief RAII mutex guard (unknown compiler - falls back to lock-only,
 *        manual unlock required)
 */
#define AIRY_MUTEX_LOCK_GUARD(m) ((void)airy_mtx_lock(&(m)))

#endif

/** @} */ /* end of mutex_guard */

/* d8 cleanup: sync_compat.h has been migrated, but some code still uses the
 * AIRY_MUTEX_* macro form. These provide compatibility mappings to the
 * airy_mtx_* functions so callers don't need to be rewritten one by one.
 * Calling convention: callers pass a pointer (e.g.
 * AIRY_MUTEX_LOCK(&ctx->mutex)); the macros forward the pointer directly
 * to the airy_mtx_* functions.
 * New code should use the airy_mtx_init/lock/unlock/destroy function form
 * directly. */

/**
 * @def AIRY_MUTEX_INIT(m, attr)
 * @brief Initialize a mutex (compat macro - forwards to airy_mtx_init)
 * @param m airy_mtx_t* pointer
 * @param attr unused (kept for pthread_mutex_init signature compatibility)
 * @return 0 on success, non-zero on failure
 */
#define AIRY_MUTEX_INIT(m, attr) airy_mtx_init(m)

/**
 * @def AIRY_MUTEX_DESTROY(m)
 * @brief Destroy a mutex (compat macro - forwards to airy_mtx_destroy)
 * @param m airy_mtx_t* pointer
 */
#define AIRY_MUTEX_DESTROY(m) airy_mtx_destroy(m)

/**
 * @def AIRY_MUTEX_LOCK(m)
 * @brief Lock a mutex (compat macro - forwards to airy_mtx_lock)
 * @param m airy_mtx_t* pointer
 * @return 0 on success, non-zero on failure
 */
#define AIRY_MUTEX_LOCK(m) airy_mtx_lock(m)

/**
 * @def AIRY_MUTEX_UNLOCK(m)
 * @brief Unlock a mutex (compat macro - forwards to airy_mtx_unlock)
 * @param m airy_mtx_t* pointer
 * @return 0 on success, non-zero on failure
 */
#define AIRY_MUTEX_UNLOCK(m) airy_mtx_unlock(m)

/**
 * @def AIRY_MUTEX_TRYLOCK(m)
 * @brief Try to lock a mutex (compat macro - forwards to airy_mtx_trylock)
 * @param m airy_mtx_t* pointer
 * @return 0 on success, non-zero on failure (EBUSY means already locked)
 */
#define AIRY_MUTEX_TRYLOCK(m) airy_mtx_trylock(m)

/* ==================== AIRY_HOME path system (production-ready) ====================
 *
 * Unified install root: defaults to $HOME/.airymaxrt, overridable via the
 * AIRY_HOME environment variable. All runtime artifacts (socket/pid/log/
 * config/data) are consolidated under it, keeping non-root deployments,
 * containerization and uninstall clean, replacing scattered FHS paths
 * (/tmp, /var/log, /etc...).
 *
 * Directory layout:
 *   $AIRY_HOME/bin      - executables (agentrt, agent_d, llm_d, mem_d, airy_cli)
 *   $AIRY_HOME/lib      - runtime dependencies (airymax_agents, openlab, sdk-python)
 *   $AIRY_HOME/run      - Unix sockets, PID files
 *   $AIRY_HOME/logs     - daemon logs, audit logs, agent subprocess logs
 *   $AIRY_HOME/config   - agentrt.yaml, model.yaml, secrets.env
 *   $AIRY_HOME/data     - persistent data (memory, etc.)
 *   $AIRY_HOME/tmp      - temporary files
 *   $AIRY_HOME/cache    - caches
 */

#define AIRY_DEFAULT_HOME_DIR ".airymaxrt"
#define AIRY_HOME_SUBDIR_BIN "bin"
#define AIRY_HOME_SUBDIR_LIB "lib"
#define AIRY_HOME_SUBDIR_RUN "run"
#define AIRY_HOME_SUBDIR_LOG "logs"
#define AIRY_HOME_SUBDIR_CONFIG "config"
#define AIRY_HOME_SUBDIR_DATA "data"
#define AIRY_HOME_SUBDIR_TMP "tmp"
#define AIRY_HOME_SUBDIR_CACHE "cache"
/* 2.1.2.5：持久化工作区（GRAD 决策链 trace / 任务工作区）。历史实现把
 * workspace 挂在 run/ 下（airy_runtime_dir_socket("workspace")），与
 * socket 同目录——run 是运行时易失目录，决策链属持久化数据，语义错位。 */
#define AIRY_HOME_SUBDIR_WORKSPACE "workspace"


const char *airy_home_dir(void);

const char *airy_bin_dir(void);

const char *airy_lib_dir(void);

const char *airy_runtime_dir(void);

/**
 * @brief Resolve runtime socket path: $AIRY_HOME/run/<name>
 *        (AIRY_HOME path system).
 *
 * Consolidates daemon socket locations: the legacy macro
 * AIRY_RUNTIME_DIR "/<name>.sock" is a compile-time constant (default
 * /tmp/agentrt) and does not follow AIRY_HOME deployments. This helper
 * appends airy_runtime_dir() at runtime so the socket always lands in
 * $AIRY_HOME/run.
 *
 * @param name socket file name (e.g. "agent.sock")
 * @return static buffer (read once at startup; safe during the daemon's
 *         single-threaded init phase)
 */
const char *airy_runtime_dir_socket(const char *name);

const char *airy_log_dir(void);

const char *airy_config_dir(void);

const char *airy_data_dir(void);

const char *airy_tmp_dir(void);

const char *airy_cache_dir(void);

/**
 * @brief Resolve the persistent workspace root directory: $AIRY_HOME/workspace
 *        (2.1.2.5 path-system normalization).
 *
 * Holds durable task artifacts: GRAD decision-chain workspaces
 * (<plan_id>/t2|c_verify|b_arbiter|trace), task execution scratch and
 * future work-hall artifacts. Kept separate from airy_runtime_dir()
 * ($AIRY_HOME/run), which is reserved for volatile runtime sockets/pids.
 *
 * Named *root* to avoid clashing with the coreloopthree workspace object
 * accessor airy_workspace_dir(const airy_workspace_t *).
 */
const char *airy_workspace_root_dir(void);

/**
 * @brief Initialize the AIRY_HOME path system
 *
 * Resolves each directory path and creates them (mkdir -p), while also
 * setenv-ing AIRY_RUNTIME_DIR/AIRY_LOG_DIR/AIRY_CONFIG_DIR/AIRY_DATA_DIR/
 * AIRY_TMP_DIR/AIRY_CACHE_DIR so existing getenv-style consumers take
 * effect immediately. Each daemon calls this once early in main (before
 * log initialization).
 *
 * @return AIRY_SUCCESS on success; AIRY_ERR_SYS_FILE on directory creation failure
 */
int airy_paths_init(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_H */
