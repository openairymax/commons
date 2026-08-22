/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file io.h
 * @brief File operation utilities.
 */

#ifndef AIRY_RT_UTILS_IO_H
#define AIRY_RT_UTILS_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the full contents of a file.
 * @param path File path
 * @param out_len Output content length (optional)
 * @return Allocated memory, caller must free; NULL on failure
 */
char *airy_io_read_file(const char *path, size_t *out_len);

/**
 * @brief Write data to a file.
 * @param path File path
 * @param data Data
 * @param len Data length (-1 auto-computes the string length)
 * @return 0 on success, negative error code on failure
 */
int airy_io_write_file(const char *path, const void *data, size_t len);

/**
 * @brief Ensure a directory exists (create it if missing).
 * @param path Directory path
 * @return 0 on success, negative error code on failure
 */
int airy_io_ensure_dir(const char *path);

/**
 * @brief Recursively create directories (cross-platform).
 * @param path Directory path
 * @param mode Directory permissions (Unix-style, ignored on Windows)
 * @return 0 on success, negative error code on failure
 */
int airy_io_mkdir_p(const char *path, int mode);

/**
 * @brief List all files in a directory (not including subdirectories).
 * @param path Directory path
 * @param out_files Output file name array (free with airy_io_free_list)
 * @param out_count Output count
 * @return 0 on success, negative error code on failure
 */
int airy_io_list_files(const char *path, char ***out_files, size_t *out_count);

/**
 * @brief Free a file list.
 */
void airy_io_free_list(char **files, size_t count);

/**
 * @brief Recursively delete a directory tree (cross-platform).
 * @param path Directory path
 * @return 0 on success (including not-exists), negative error code on failure
 */
int airy_io_remove_dir_recursive(const char *path);

/**
 * @brief Delete a single file (cross-platform).
 * @param path File path
 * @return 0 on success (including not-exists), negative error code on failure
 */
int airy_io_remove_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UTILS_IO_H */