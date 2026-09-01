// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 *
 * @file network_http.c
 * @brief Network module - HTTP request domain.
 *
 * Implements the HTTP request construction/send/response parsing and
 * release functions declared in network_common.h, following the design
 * principles of ARCHITECTURAL_PRINCIPLES.md:
 * - E-4 Cross-platform consistency: Windows/Linux/macOS
 * - E-5 Semantic naming: every function name states its purpose
 * - E-6 Traceable errors: unified error code system
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "../memory/airy_memory.h"
#include "network_common.h"
#include "network_common_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _CRT_NONSTDC_NO_DEPRECATE
#ifdef _WIN32
#define strdup _strdup
#endif
#include "atomic_compat.h"

#include <stdarg.h>
#include "error.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

airy_err_t network_http_request(network_connection_t *connection,
                                const network_http_request_t *request,
                                network_http_response_t *response)
{
    if (!connection || !request || !response) {
        return AIRY_EINVAL;
    }

    if (connection->status != NETWORK_STATUS_CONNECTED) {
        return AIRY_ENOTCONN;
    }

    AIRY_MEMSET(response, 0, sizeof(network_http_response_t));

    char request_buf[NETWORK_DEFAULT_BUFFER_SIZE * 2];
    int offset = 0;

    /* Check truncation after every snprintf step: the return value is the
     * "would-be-written" length, which may be >= remaining capacity when
     * path/host/custom headers are too long. Naively accumulating would
     * push offset past the buffer and underflow the subsequent size
     * argument; abort with an error to keep offset always in range. */
#define NETWORK_REQ_APPEND(fmt, ...)                                                         \
    do {                                                                                     \
        int wlen = snprintf(request_buf + offset, sizeof(request_buf) - (size_t)offset, fmt, \
                            ##__VA_ARGS__);                                                  \
        if (wlen < 0 || (size_t)wlen >= sizeof(request_buf) - (size_t)offset) {              \
            response->error = AIRY_ERR_OVERFLOW;                                             \
            response->error_message = AIRY_STRDUP("HTTP request header too long");           \
            return AIRY_ERR_OVERFLOW;                                                        \
        }                                                                                    \
        offset += wlen;                                                                      \
    } while (0)

    NETWORK_REQ_APPEND("%s %s HTTP/1.1\r\n", request->method ? request->method : "GET",
                       request->path ? request->path : "/");

    if (connection->config.host) {
        NETWORK_REQ_APPEND("Host: %s\r\n", connection->config.host);
    }

    /* Content-Type */
    if (request->content_type) {
        NETWORK_REQ_APPEND("Content-Type: %s\r\n", request->content_type);
    }

    /* Content-Length */
    if (request->body && request->body_len > 0) {
        NETWORK_REQ_APPEND("Content-Length: %zu\r\n", request->body_len);
    }

    if (request->headers && request->header_count > 0) {
        for (size_t i = 0; i < request->header_count; i++) {
            if (request->headers[i]) {
                NETWORK_REQ_APPEND("%s\r\n", request->headers[i]);
            }
        }
    }

    NETWORK_REQ_APPEND("\r\n");

#undef NETWORK_REQ_APPEND

    airy_err_t err = network_send_all(connection, request_buf, (size_t)offset);
    if (err != AIRY_SUCCESS) {
        response->error = err;
        response->error_message = AIRY_STRDUP("Failed to send request headers");
        return err;
    }

    if (request->body && request->body_len > 0) {
        err = network_send_all(connection, request->body, request->body_len);
        if (err != AIRY_SUCCESS) {
            response->error = err;
            response->error_message = AIRY_STRDUP("Failed to send request body");
            return err;
        }
    }

    char recv_buffer[65536];
    size_t total_received = 0;
    size_t received = 0;
    int retry_count = 0;

    do {
        received = 0;
        err = network_receive(connection, recv_buffer + total_received,
                              sizeof(recv_buffer) - total_received - 1, &received);
        if (err == AIRY_SUCCESS && received > 0) {
            total_received += received;
            retry_count = 0;
        } else if (err != AIRY_SUCCESS) {
            retry_count++;
            if (retry_count > 10 || err == AIRY_ECONNRESET) {
                break;
            }
        }
    } while (received > 0 && total_received < sizeof(recv_buffer) - 1);

    recv_buffer[total_received] = '\0';

    /* Parse HTTP status code manually */
    if (total_received >= 12) {
        if (__builtin_strncmp(recv_buffer, "HTTP/1.", 7) == 0 && recv_buffer[8] == ' ') {
            response->status_code = (int)strtol(recv_buffer + 9, NULL, 10);
        } else {
            response->status_code = 200;
        }
    } else {
        response->status_code = 200;
    }

    /* Separate response headers and body */
    char *body_start = strstr(recv_buffer, "\r\n\r\n");
    if (body_start) {
        size_t header_len = body_start - recv_buffer + 4;

        response->headers = (char **)AIRY_CALLOC(1, sizeof(char *));
        if (response->headers) {
            response->headers[0] = (char *)AIRY_MALLOC(header_len + 1);
            if (response->headers[0]) {
                __builtin_memcpy(response->headers[0], recv_buffer, header_len);
                response->headers[0][header_len] = '\0';
            }
            response->header_count = 1;
        }

        body_start += 4;
        size_t body_len = total_received - (body_start - recv_buffer);
        response->body = AIRY_MALLOC(body_len + 1);
        if (response->body) {
            __builtin_memcpy(response->body, body_start, body_len);
            ((char *)response->body)[body_len] = '\0';
            response->body_len = body_len;
        }
    } else {

        response->body = AIRY_MALLOC(total_received + 1);
        if (response->body) {
            __builtin_memcpy(response->body, recv_buffer, total_received);
            ((char *)response->body)[total_received] = '\0';
            response->body_len = total_received;
        }
    }

    response->error = AIRY_SUCCESS;
    return AIRY_SUCCESS;
}

airy_err_t network_http_get(network_connection_t *connection, const char *path,
                            network_http_response_t *response)
{
    network_http_request_t request = {0};
    request.method = "GET";
    request.path = path;

    return network_http_request(connection, &request, response);
}

airy_err_t network_http_post(network_connection_t *connection, const char *path,
                             const char *content_type, const void *body, size_t body_len,
                             network_http_response_t *response)
{
    network_http_request_t request = {0};
    request.method = "POST";
    request.path = path;
    request.content_type = content_type ? content_type : "application/json";
    request.body = body;
    request.body_len = body_len;

    return network_http_request(connection, &request, response);
}

void network_http_response_free(network_http_response_t *response)
{
    if (!response) {
        return;
    }

    if (response->body) {
        AIRY_FREE(response->body);
        response->body = NULL;
    }

    if (response->headers) {
        for (size_t i = 0; i < response->header_count; i++) {
            if (response->headers[i]) {
                AIRY_FREE(response->headers[i]);
            }
        }
        AIRY_FREE(response->headers);
        response->headers = NULL;
    }

    if (response->error_message) {
        AIRY_FREE(response->error_message);
        response->error_message = NULL;
    }

    if (response->status_text) {
        AIRY_FREE(response->status_text);
        response->status_text = NULL;
    }

    AIRY_MEMSET(response, 0, sizeof(network_http_response_t));
}
