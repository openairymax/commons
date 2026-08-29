/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * IPC message-queue API: create/destroy/send/receive/count/clear.
 * Split from ipc_common.h (0.1.6 大文件拆分).
 */

#ifndef AIRY_RT_IPC_MQ_API_H
#define AIRY_RT_IPC_MQ_API_H

#include "ipc_types.h"

/**
 * @brief Message queue handle
 */
typedef struct ipc_mq ipc_mq_t;

/**
 * @brief Message queue configuration
 */
typedef struct {
    const char *name;
    size_t max_messages;
    size_t max_message_size;
    bool create;
    bool exclusive;
    const char *permissions;
} ipc_mq_config_t;

/**
 * @brief Create a message queue
 * @param config Message queue configuration
 * @return Message queue handle, NULL on failure
 */
ipc_mq_t *ipc_mq_create(const ipc_mq_config_t *config);

/**
 * @brief Destroy a message queue
 * @param mq Message queue handle
 */
void ipc_mq_destroy(ipc_mq_t *mq);

/**
 * @brief Send a message to the queue
 * @param mq Message queue handle
 * @param data Message data
 * @param len Data length
 * @param priority Priority (0 is lowest)
 * @return Error code
 */
airy_err_t ipc_mq_send(ipc_mq_t *mq, const void *data, size_t len, unsigned int priority);

/**
 * @brief Receive a message from the queue
 * @param mq Message queue handle
 * @param buffer Receive buffer
 * @param len Buffer length
 * @param received [out] Actual bytes received
 * @param priority [out] Message priority (optional)
 * @param timeout_ms Timeout
 * @return Error code
 */
airy_err_t ipc_mq_receive(ipc_mq_t *mq, void *buffer, size_t len, size_t *received,
                          unsigned int *priority, uint32_t timeout_ms);

/**
 * @brief Get the current message count of the queue
 * @param mq Message queue handle
 * @return Message count
 */
size_t ipc_mq_count(const ipc_mq_t *mq);

/**
 * @brief Clear the message queue
 * @param mq Message queue handle
 * @return Error code
 */
airy_err_t ipc_mq_clear(ipc_mq_t *mq);

#endif /* AIRY_RT_IPC_MQ_API_H */
