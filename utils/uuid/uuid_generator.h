/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file uuid_generator.h
 * @brief UUID v4 generator interface.
 */

#ifndef AIRY_RT_UUID_GENERATOR_H
#define AIRY_RT_UUID_GENERATOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UUID format.
 */
#define AIRY_UUID_STR_LEN 37
#define AIRY_UUID_PREFIXED_STR_LEN 64

/**
 * @brief UUID generation error codes.
 */
typedef enum airy_uuid_error {
    AIRY_UUID_SUCCESS = 0,
    AIRY_UUID_EINVALID = -1,
    AIRY_UUID_ENOMEM = -2,
    AIRY_UUID_EUNAVAIL = -3
} airy_uuid_error_t;

/**
 * @brief Initialize the UUID generator.
 * @return AIRY_UUID_SUCCESS on success
 */
airy_uuid_error_t airy_uuid_init(void);

/**
 * @brief Clean up the UUID generator.
 */
void airy_uuid_cleanup(void);

/**
 * @brief Generate a standard UUID v4 string.
 * @param out_buf Output buffer (at least AIRY_UUID_STR_LEN bytes)
 * @param buf_len Buffer length
 * @return AIRY_UUID_SUCCESS on success
 */
airy_uuid_error_t airy_uuid_v4(char *out_buf, size_t buf_len);

/**
 * @brief Generate a UUID with a prefix.
 * @param prefix Prefix string (e.g. "mem_", "task_")
 * @param out_buf Output buffer (at least AIRY_UUID_PREFIXED_STR_LEN bytes)
 * @param buf_len Buffer length
 * @return AIRY_UUID_SUCCESS on success
 */
airy_uuid_error_t airy_uuid_with_prefix(const char *prefix, char *out_buf, size_t buf_len);

/**
 * @brief Validate a UUID string format.
 * @param uuid UUID string
 * @return 1 if valid, 0 otherwise
 */
int airy_uuid_is_valid(const char *uuid);

/**
 * @brief Convert raw UUID binary to a string.
 * @param uuid_bin 16-byte raw UUID
 * @param out_buf Output buffer (at least AIRY_UUID_STR_LEN bytes)
 * @param buf_len Buffer length
 * @return AIRY_UUID_SUCCESS on success
 */
airy_uuid_error_t airy_uuid_bin_to_str(const uint8_t *uuid_bin, char *out_buf, size_t buf_len);

/**
 * @brief Convert a UUID string to raw binary.
 * @param uuid_str UUID string
 * @param out_bin Output buffer (at least 16 bytes)
 * @return AIRY_UUID_SUCCESS on success
 */
airy_uuid_error_t airy_uuid_str_to_bin(const char *uuid_str, uint8_t *out_bin);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_UUID_GENERATOR_H */
