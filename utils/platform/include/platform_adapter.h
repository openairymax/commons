/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file platform_adapter.h
 * @brief Platform adapter: advanced cross-platform utility set.
 *
 * ## Module positioning
 *
 * This module lives in AgentRT commons utils/platform/ and provides
 * high-level cross-platform utilities for application layers.
 *
 * ## Difference from the top-level platform/ module
 *
 * | Aspect     | This module (utils/platform/) | Top-level module (platform/) |
 * |------------|-------------------------------|------------------------------|
 * | Location   | utils/platform/               | platform/                    |
 * | Abstraction| Application layer (high-level)| System layer (low-level)     |
 * | Core       | File system, env vars, paths  | Thread primitives, Socket, time |
 * | Use cases  | Business logic code           | Infrastructure code          |
 * | Perf       | General                       | Critical-path optimized      |
 * | Users      | cognition, strategy, etc.     | sync, ipc and other low-level |
 *
 * ## Design philosophy
 *
 * Follows the "engineering dimension" principle of the AgentRT five-axis
 * orthogonal system:
 * - Unified interface: all platform differences handled transparently
 * - Least surprise: intuitive APIs with clear parameter semantics
 * - Incremental migration: platform-specific code can be replaced
 *   gradually
 *
 * ## Main feature categories
 *
 * ### 1. File system operations
 * - platform_mkdir(), platform_unlink(), platform_copy_file()
 * - Recursive directory creation, file info queries
 *
 * ### 2. Environment and paths
 * - platform_get_env(), platform_set_env()
 * - platform_path_join(), platform_path_normalize()
 *
 * ### 3. System services
 * - platform_get_timestamp_ms/us()
 * - platform_sleep_ms()
 *
 * ## Subprocess execution
 *
 * Subprocess execution uniformly uses airy_process_run_capture() from the
 * top-level platform.h (fork+execvp, no shell), eliminating command
 * injection risk. platform_exec()/platform_free_exec_result() were removed
 * (BAN-211/235 security compliance).
 *
 * @see platform.h (top-level system abstraction layer)
 */

#ifndef AIRY_RT_PLATFORM_ADAPTER_H
#define AIRY_RT_PLATFORM_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 0.1.6 P1-2 依赖图去环：本头仅声明平台适配 API（bool/size_t/time_t 等
 * 标准类型），不使用 airy_memory.h / string_compat.h 的任何符号，两处
 * include 为历史遗留，移除以消除 platform -> memory / platform -> string
 * 域级环（platform 成为只被依赖的基座域）。 */
#include <string.h>

/**
 * @brief Platform types
 */
typedef enum {
    PLATFORM_UNKNOWN,
    PLATFORM_WINDOWS,
    PLATFORM_LINUX,
    PLATFORM_MACOS,
    PLATFORM_UNIX
} platform_type_t;

/**
 * @brief File information
 */
typedef struct platform_file_info {
    const char *path;
    size_t size;
    time_t mtime;
    bool is_directory;
    bool exists;
} platform_file_info_t;

/**
 * @brief Get the current platform type
 * @return Platform type
 */
platform_type_t platform_get_type(void);

/**
 * @brief Get the platform name
 * @return Platform name string
 */
const char *platform_get_name(void);

/**
 * @brief Get file information
 * @param path File path
 * @return File information
 */
platform_file_info_t platform_get_file_info(const char *path);

/**
 * @brief Create a directory
 * @param path Directory path
 * @return true on success, false on failure
 */
bool platform_mkdir(const char *path);

/**
 * @brief Create a directory (recursively)
 * @param path Directory path
 * @return true on success, false on failure
 */
bool platform_mkdir_recursive(const char *path);

/**
 * @brief Delete a file
 * @param path File path
 * @return true on success, false on failure
 */
bool platform_unlink(const char *path);

/**
 * @brief Delete a directory
 * @param path Directory path
 * @return true on success, false on failure
 */
bool platform_rmdir(const char *path);

/**
 * @brief Copy a file
 * @param src Source file path
 * @param dest Destination file path
 * @return true on success, false on failure
 */
bool platform_copy_file(const char *src, const char *dest);

/**
 * @brief Move a file
 * @param src Source file path
 * @param dest Destination file path
 * @return true on success, false on failure
 */
bool platform_move_file(const char *src, const char *dest);

/**
 * @brief Get an environment variable
 * @param name Environment variable name
 * @param default_value Default value
 * @return Environment variable value (release with AIRY_FREE)
 */
char *platform_get_env(const char *name, const char *default_value);

/**
 * @brief Set an environment variable
 * @param name Environment variable name
 * @param value Environment variable value
 * @return true on success, false on failure
 */
bool platform_set_env(const char *name, const char *value);

/**
 * @brief Get the current working directory
 * @return Current working directory (release with AIRY_FREE)
 */
char *platform_get_cwd(void);

/**
 * @brief Change the current working directory
 * @param path Target path
 * @return true on success, false on failure
 */
bool platform_chdir(const char *path);

/**
 * @brief Get the temp directory
 * @return Temp directory path (release with AIRY_FREE)
 */
char *platform_get_temp_dir(void);

/**
 * @brief Generate a temp file path
 * @param prefix Prefix
 * @return Temp file path (release with AIRY_FREE)
 */
char *platform_get_temp_file(const char *prefix);

/**
 * @brief Join paths
 * @param path1 Path 1
 * @param path2 Path 2
 * @return Joined path (release with AIRY_FREE)
 */
char *platform_path_join(const char *path1, const char *path2);

/**
 * @brief Normalize a path
 * @param path Path
 * @return Normalized path (release with AIRY_FREE)
 */
char *platform_path_normalize(const char *path);

/**
 * @brief Get the file name portion of a path
 * @param path Path
 * @return File name (release with AIRY_FREE)
 */
char *platform_path_basename(const char *path);

/**
 * @brief Get the directory portion of a path
 * @param path Path
 * @return Directory path (release with AIRY_FREE)
 */
char *platform_path_dirname(const char *path);

/**
 * @brief Check whether a path exists
 * @param path Path
 * @return true if it exists, false otherwise
 */
bool platform_path_exists(const char *path);

/**
 * @brief Check whether a path is a directory
 * @param path Path
 * @return true if a directory, false otherwise
 */
bool platform_path_is_directory(const char *path);

/**
 * @brief Check whether a path is a file
 * @param path Path
 * @return true if a file, false otherwise
 */
bool platform_path_is_file(const char *path);

/**
 * @brief Get the system timestamp (milliseconds)
 * @return Timestamp
 */
uint64_t platform_get_timestamp_ms(void);

/**
 * @brief Get the system timestamp (microseconds)
 * @return Timestamp
 */
uint64_t platform_get_timestamp_us(void);

/**
 * @brief Sleep for the given number of milliseconds
 * @param ms Milliseconds
 */
void platform_sleep_ms(unsigned int ms);

/**
 * @brief Initialize the platform adapter
 * @return true on success, false on failure
 */
bool platform_adapter_init(void);

/**
 * @brief Clean up the platform adapter
 */
void platform_adapter_cleanup(void);

#endif /* AIRY_RT_PLATFORM_ADAPTER_H */
