/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file platform_misc.h
 * @brief Cross-platform compatibility layer - time / random / file / system
 *
 * Time & random helpers, file utilities, network init, safe string ops,
 * error translation and system info probing. Domain split of platform.h
 * (2026-08-27).
 *
 * @see platform.h aggregate entry
 */

#ifndef AIRY_RT_PLATFORM_MISC_H
#define AIRY_RT_PLATFORM_MISC_H

#include "platform_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/* ==================== Time & random ==================== */

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


/* ==================== File utilities ==================== */

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
 * @brief Take an advisory file lock (cross-process single-instance guard)
 *
 * POSIX fcntl F_SETLK/F_SETLKW record lock; Windows LockFileEx. Lock is
 * released on close or process exit (advisory: cooperating processes must
 * call this API). Daemons use it to guarantee single-instance startup.
 *
 * @param fd file descriptor (POSIX) / HANDLE cast (Windows)
 * @param exclusive 1 exclusive (write) lock, 0 shared (read) lock
 * @param block 1 block until acquired, 0 fail fast (EBUSY)
 * @return 0 acquired; AIRY_EBUSY held by another process (non-block only);
 *         other non-zero on error
 */
int airy_file_lock(int fd, int exclusive, int block);

/**
 * @brief Release an advisory file lock previously taken by airy_file_lock
 * @param fd file descriptor / handle
 * @return 0 on success, non-zero on failure
 */
int airy_file_unlock(int fd);


/* ==================== Network & signal ==================== */

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


/* ==================== Safe string & error ==================== */

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


/* ==================== System info ==================== */

#ifndef AIRY_SYSINFO_T_DEFINED
#define AIRY_SYSINFO_T_DEFINED
typedef struct {
    char os_name[64];
    char os_version[64];
    char hostname[64];
    /* 2026-08-24 强化（1.10.2 架构适配）：CPU 型号字符串
     * （Linux /proc/cpuinfo "model name"、macOS machdep.cpu.brand_string、
     * Windows CentralProcessor\ProcessorNameString）。供硬件画像/外设
     * 增强检测复用（install.sh / airymaxrt monitor 的 C 侧 SSoT 判据）。 */
    char cpu_model[128];
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} airy_sysinfo_t;
#endif

int airy_get_sysinfo(airy_sysinfo_t *info);

/**
 * @brief Probe GPU model(s) into a caller buffer (best effort).
 *
 * Used by the F2 hardware panel and the hardware auto-trim detector so
 * the runtime can report (and later act on) GPU presence. Formats a
 * short human string, e.g. "NVIDIA GeForce RTX 4090" (first adapter
 * only, for display). On platforms/tools without a probe it writes an
 * empty string and returns AIRY_SUCCESS (not an error — "no GPU
 * reported" is a valid result). Probe order (POSIX):
 *   1. nvidia-smi -L (NVIDIA driver present)
 *   2. /proc/driver/nvidia/version (driver without nvidia-smi)
 *   3. lspci (generic VGA/3D/display controller)
 *
 * @param out [out] GPU info string (NUL-terminated; empty when none found)
 * @param cap [in]  out buffer capacity
 * @return AIRY_SUCCESS / AIRY_EINVAL
 */
int airy_get_gpu_info(char *out, size_t cap);


#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_MISC_H */
