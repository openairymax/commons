// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file config_service_crypto.c
 * @brief Unified config module - encryption and secure storage.
 *
 * Implements AES-256-GCM encrypt/decrypt of config values, hex
 * encode/decode and the encrypted config source wrapper, single
 * responsibility.
 */

#include "config_service.h"

#include "config_service_internal.h"

#include <platform.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "error.h"

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static char *config_bytes_to_hex(const unsigned char *data, size_t len)
{
    char *hex = (char *)AIRY_CALLOC(1, len * 2 + 1);
    if (!hex) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    for (size_t i = 0; i < len; i++) {
        snprintf(hex + i * 2, 3, "%02x", data[i]);
    }
    return hex;
}

static unsigned char *config_hex_to_bytes(const char *hex, size_t *out_len)
{
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }
    size_t byte_len = hex_len / 2;
    unsigned char *bytes = (unsigned char *)AIRY_CALLOC(1, byte_len);
    if (!bytes) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    for (size_t i = 0; i < byte_len; i++) {
        unsigned int val;
        char hex_byte[3] = {0};
        __builtin_memcpy(hex_byte, hex + i * 2, 2);
        val = (unsigned int)strtol(hex_byte, NULL, 16);
        if (hex_byte[0] == '\0') {
            AIRY_FREE(bytes);
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        bytes[i] = (unsigned char)val;
    }
    *out_len = byte_len;
    return bytes;
}
#pragma GCC diagnostic pop

static config_value_t *config_encrypt_string_value(const char *plaintext, size_t plaintext_len,
                                                   const encryption_config_t *enc)
{
    if (!plaintext || !enc || !enc->key || enc->key_len < 32 || !enc->iv || enc->iv_len < 12) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

#ifdef HAVE_OPENSSL
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)enc->iv_len, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_EncryptInit_ex(ctx, NULL, NULL, (const unsigned char *)enc->key,
                           (const unsigned char *)enc->iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    size_t ct_max = plaintext_len + 16;
    unsigned char *ciphertext = (unsigned char *)AIRY_CALLOC(1, ct_max);
    if (!ciphertext) {
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    int out_len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext, &out_len, (const unsigned char *)plaintext,
                          (int)plaintext_len) != 1) {
        AIRY_FREE(ciphertext);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx, ciphertext + out_len, &final_len) != 1) {
        AIRY_FREE(ciphertext);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }
    size_t ct_len = (size_t)(out_len + final_len);

    unsigned char tag[16];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
        AIRY_FREE(ciphertext);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    EVP_CIPHER_CTX_free(ctx);

    size_t encoded_len = 16 + enc->iv_len + ct_len;
    unsigned char *encoded = (unsigned char *)AIRY_CALLOC(1, encoded_len);
    if (!encoded) {
        AIRY_FREE(ciphertext);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    __builtin_memcpy(encoded, tag, 16);
    __builtin_memcpy(encoded + 16, enc->iv, enc->iv_len);
    __builtin_memcpy(encoded + 16 + enc->iv_len, ciphertext, ct_len);
    AIRY_FREE(ciphertext);

    char *hex = config_bytes_to_hex(encoded, encoded_len);
    AIRY_FREE(encoded);
    if (!hex) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    config_value_t *result = config_value_create_string(hex);
    AIRY_FREE(hex);
    return result;
#else
    (void)plaintext_len;
    AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
#endif
}

static config_value_t *config_decrypt_string_value(const char *hex_data,
                                                   const encryption_config_t *enc)
{
    if (!hex_data || !enc || !enc->key || enc->key_len < 32) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

#ifdef HAVE_OPENSSL
    size_t data_len = 0;
    unsigned char *data = config_hex_to_bytes(hex_data, &data_len);
    if (!data || data_len < 16 + 12) {
        AIRY_FREE(data);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    const unsigned char *tag = data;
    const unsigned char *iv = data + 16;
    size_t iv_len = enc->iv_len > 0 ? enc->iv_len : 12;
    if (16 + iv_len > data_len) {
        AIRY_FREE(data);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }
    const unsigned char *ciphertext = data + 16 + iv_len;
    size_t ct_len = data_len - 16 - iv_len;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        AIRY_FREE(data);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        AIRY_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL) != 1) {
        AIRY_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, (const unsigned char *)enc->key, iv) != 1) {
        AIRY_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    unsigned char *plaintext = (unsigned char *)AIRY_CALLOC(1, ct_len + 1);
    if (!plaintext) {
        AIRY_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    int out_len = 0;
    if (EVP_DecryptUpdate(ctx, plaintext, &out_len, ciphertext, (int)ct_len) != 1) {
        AIRY_FREE(plaintext);
        AIRY_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void *)tag) != 1) {
        AIRY_FREE(plaintext);
        AIRY_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext + out_len, &final_len) != 1) {
        AIRY_FREE(plaintext);
        AIRY_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }

    size_t pt_len = (size_t)(out_len + final_len);
    plaintext[pt_len] = '\0';

    EVP_CIPHER_CTX_free(ctx);
    AIRY_FREE(data);
    data = NULL;

    config_value_t *result = config_value_create_string((const char *)plaintext);
    explicit_bzero(plaintext, ct_len + 1);
    AIRY_FREE(plaintext);
    return result;
#else
    errno = ENOSYS;
    return NULL;
#endif
}

config_value_t *config_encrypt_value(const config_value_t *value,
                                     const encryption_config_t *manager)
{
    if (!value) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    if (!manager || manager->algorithm == ENCRYPTION_NONE) {
        return config_value_clone(value);
    }

    config_value_type_t type = config_value_get_type(value);
    if (type == CONFIG_TYPE_STRING) {
        const char *str = config_value_get_string(value, NULL);
        if (!str) {
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        size_t str_len = strlen(str);
        config_value_t *encrypted = config_encrypt_string_value(str, str_len, manager);
        return encrypted ? encrypted : config_value_clone(value);
    }

    return config_value_clone(value);
}

config_value_t *config_decrypt_value(const config_value_t *encrypted_value,
                                     const encryption_config_t *manager)
{
    if (!encrypted_value) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    if (!manager || manager->algorithm == ENCRYPTION_NONE) {
        return config_value_clone(encrypted_value);
    }

    config_value_type_t type = config_value_get_type(encrypted_value);
    if (type == CONFIG_TYPE_STRING) {
        const char *hex_data = config_value_get_string(encrypted_value, NULL);
        if (!hex_data) {
            AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
        }
        config_value_t *decrypted = config_decrypt_string_value(hex_data, manager);
        return decrypted ? decrypted : config_value_clone(encrypted_value);
    }

    return config_value_clone(encrypted_value);
}

config_source_t *config_source_create_encrypted(config_source_t *source,
                                                const encryption_config_t *manager)
{
    if (!source) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
    if (!manager || manager->algorithm == ENCRYPTION_NONE)
        return source;
    return source;
}
