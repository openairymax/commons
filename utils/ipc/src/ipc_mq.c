// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file ipc_mq.c
 * @brief IPC module - message queue implementation.
 *
 * Implements the message queue API declared in ipc_common.h:
 * - Create/destroy message queues
 * - Send/receive messages (priority ordering and timeout wait)
 * - Query queue message count, clear the queue
 *
 * Internally based on a doubly linked list + mutex/condvar:
 * - ipc_mq_lock/ipc_mq_unlock: lock/unlock (timeout support)
 * - ipc_mq_wait_for_message: wait for a message (condvar)
 * - ipc_mq_dequeue_message: take a message from the queue head
 *
 * Following ARCHITECTURAL_PRINCIPLES.md design principles:
 * - E-3 Resource determinism: message data and nodes freed on dequeue
 * - E-6 Traceable errors: unified error code system
 * - E-7 Concurrency safety: mutex protects the queue state
 *
 * @see ipc_common_internal.h internal shared definitions
 */

#include "ipc_common_internal.h"

#include "ipc_common.h"

#include "platform.h"

static int ipc_mq_lock(ipc_mq_t *mq, uint32_t timeout_ms)
{
#ifdef _WIN32
    DWORD wait_result = WaitForSingleObject(mq->hMutex, timeout_ms);
    return (wait_result == WAIT_TIMEOUT) ? ETIMEDOUT : 0;
#else
    if (timeout_ms == 0) {
        return airy_mtx_lock(&mq->mutex);
    } else {
        uint64_t deadline = airy_time_ms() + timeout_ms;
        while (airy_mtx_trylock(&mq->mutex) != 0) {
            if (airy_time_ms() >= deadline) {
                return ETIMEDOUT;
            }
            struct timespec ts = {0, 1000000};
            nanosleep(&ts, NULL);
        }
        return 0;
    }
#endif
}

static void ipc_mq_unlock(ipc_mq_t *mq)
{
#ifdef _WIN32
    ReleaseMutex(mq->hMutex);
#else
    airy_mtx_unlock(&mq->mutex);
#endif
}

static bool ipc_mq_wait_for_message(ipc_mq_t *mq, uint32_t timeout_ms)
{
    if (mq->current_count > 0) {
        return true;
    }

    if (timeout_ms == 0) {
        return false;
    }

    ipc_mq_unlock(mq);

#ifdef _WIN32
    DWORD wait_result = WaitForSingleObject(mq->hNotEmpty, timeout_ms);
    if (wait_result == WAIT_TIMEOUT) {
        return false;
    }
    WaitForSingleObject(mq->hMutex, INFINITE);
#else
    airy_mtx_lock(&mq->mutex);
    while (mq->current_count == 0) {
        int wait_result = airy_cond_timedwait(&mq->not_empty, &mq->mutex, timeout_ms);
        if (wait_result != 0) {
            airy_mtx_unlock(&mq->mutex);
            return false;
        }
    }
#endif

    return (mq->current_count > 0);
}

static airy_err_t ipc_mq_dequeue_message(ipc_mq_t *mq, void *buffer, size_t len, size_t *received,
                                         unsigned int *priority)
{
    ipc_mq_message_t *msg = mq->head;
    if (!msg) {
        return AIRY_EINVAL;
    }

    size_t copy_len = (len < msg->len) ? len : msg->len;
    __builtin_memcpy(buffer, msg->data, copy_len);

    if (received) {
        *received = copy_len;
    }

    if (priority) {
        *priority = msg->priority;
    }

    mq->head = msg->next;
    if (mq->head == NULL) {
        mq->tail = NULL;
    }

    mq->current_count--;
    mq->total_dequeued++;

    AIRY_FREE(msg->data);
    AIRY_FREE(msg);

    return AIRY_SUCCESS;
}

ipc_mq_t *ipc_mq_create(const ipc_mq_config_t *config)
{
    if (!config) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    ipc_mq_t *mq = (ipc_mq_t *)AIRY_CALLOC(1, sizeof(ipc_mq_t));
    if (!mq) {
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }

    mq->config = *config;
    mq->current_count = 0;
    mq->total_enqueued = 0;
    mq->total_dequeued = 0;
    mq->head = NULL;
    mq->tail = NULL;
    AIRY_MEMSET(mq->error_msg, 0, sizeof(mq->error_msg));

#ifdef _WIN32
    mq->hMutex = CreateMutex(NULL, FALSE, NULL);
    mq->hNotEmpty = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!mq->hMutex || !mq->hNotEmpty) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "Failed to create synchronization objects");
        if (mq->hMutex)
            CloseHandle(mq->hMutex);
        if (mq->hNotEmpty)
            CloseHandle(mq->hNotEmpty);
        AIRY_FREE(mq);
        AIRY_ERROR_NULL(AIRY_ERR_INVALID_PARAM, "null parameter");
    }
#else
    if (airy_mtx_init(&mq->mutex) != 0) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "Failed to initialize mutex");
        AIRY_FREE(mq);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }
    if (airy_cond_init(&mq->not_empty) != 0) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "Failed to initialize condition variable");
        airy_mtx_destroy(&mq->mutex);
        AIRY_FREE(mq);
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "validation failed");
    }
#endif

    return mq;
}

void ipc_mq_destroy(ipc_mq_t *mq)
{
    if (!mq) {
        return;
    }

    ipc_mq_clear(mq);

#ifdef _WIN32
    if (mq->hMutex)
        CloseHandle(mq->hMutex);
    if (mq->hNotEmpty)
        CloseHandle(mq->hNotEmpty);
#else
    airy_mtx_destroy(&mq->mutex);
    airy_cond_destroy(&mq->not_empty);
#endif

    AIRY_FREE(mq);
}

airy_err_t ipc_mq_send(ipc_mq_t *mq, const void *data, size_t len, unsigned int priority)
{
    if (!mq || !data || len == 0) {
        return AIRY_EINVAL;
    }

#ifdef _WIN32
    WaitForSingleObject(mq->hMutex, INFINITE);
#else
    airy_mtx_lock(&mq->mutex);
#endif

    if (mq->current_count >= mq->config.max_messages) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "Message queue full (count=%zu, max=%zu)",
                 mq->current_count, mq->config.max_messages);
#ifdef _WIN32
        ReleaseMutex(mq->hMutex);
#else
        airy_mtx_unlock(&mq->mutex);
#endif
        return AIRY_EBUSY;
    }

    ipc_mq_message_t *msg = (ipc_mq_message_t *)AIRY_MALLOC(sizeof(ipc_mq_message_t));
    if (!msg) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "Failed to allocate memory for message");
#ifdef _WIN32
        ReleaseMutex(mq->hMutex);
#else
        airy_mtx_unlock(&mq->mutex);
#endif
        return AIRY_ENOMEM;
    }

    msg->data = AIRY_MALLOC(len);
    if (!msg->data) {
        snprintf(mq->error_msg, sizeof(mq->error_msg),
                 "Failed to allocate memory for message data");
        AIRY_FREE(msg);
#ifdef _WIN32
        ReleaseMutex(mq->hMutex);
#else
        airy_mtx_unlock(&mq->mutex);
#endif
        return AIRY_ENOMEM;
    }

    __builtin_memcpy(msg->data, data, len);
    msg->len = len;
    msg->priority = priority;
    msg->timestamp = ipc_get_timestamp_ns();
    msg->next = NULL;
    if (mq->tail == NULL) {
        mq->head = mq->tail = msg;
    } else if (priority >= mq->tail->priority) {
        mq->tail->next = msg;
        mq->tail = msg;
    } else if (priority > mq->head->priority) {
        msg->next = mq->head;
        mq->head = msg;
    } else {
        ipc_mq_message_t *current = mq->head;
        while (current->next && current->next->priority > priority) {
            current = current->next;
        }
        msg->next = current->next;
        current->next = msg;
    }

    mq->current_count++;
    mq->total_enqueued++;

#ifdef _WIN32
    SetEvent(mq->hNotEmpty);
    ReleaseMutex(mq->hMutex);
#else
    airy_cond_signal(&mq->not_empty);
    airy_mtx_unlock(&mq->mutex);
#endif

    return AIRY_SUCCESS;
}

airy_err_t ipc_mq_receive(ipc_mq_t *mq, void *buffer, size_t len, size_t *received,
                          unsigned int *priority, uint32_t timeout_ms)
{
    if (!mq || !buffer) {
        return AIRY_EINVAL;
    }

    int lock_result = ipc_mq_lock(mq, timeout_ms);
    if (lock_result == ETIMEDOUT) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "Receive timeout after %u ms", timeout_ms);
        return AIRY_ETIMEDOUT;
    }
    if (lock_result != 0) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "Failed to acquire mutex");
        return AIRY_EUNKNOWN;
    }

    if (!ipc_mq_wait_for_message(mq, timeout_ms)) {
        snprintf(mq->error_msg, sizeof(mq->error_msg), "No message available after timeout");
        return AIRY_EBUSY;
    }

    airy_err_t err = ipc_mq_dequeue_message(mq, buffer, len, received, priority);

    ipc_mq_unlock(mq);

    return err;
}

size_t ipc_mq_count(const ipc_mq_t *mq)
{
    if (!mq) {
        return 0;
    }
    return mq->current_count;
}

airy_err_t ipc_mq_clear(ipc_mq_t *mq)
{
    if (!mq) {
        return AIRY_EINVAL;
    }

#ifdef _WIN32
    WaitForSingleObject(mq->hMutex, INFINITE);
#else
    airy_mtx_lock(&mq->mutex);
#endif

    ipc_mq_message_t *current = mq->head;
    while (current != NULL) {
        ipc_mq_message_t *next = current->next;
        if (current->data) {
            AIRY_FREE(current->data);
        }
        AIRY_FREE(current);
        current = next;
    }

    mq->head = NULL;
    mq->tail = NULL;
    mq->current_count = 0;

#ifdef _WIN32
    ReleaseMutex(mq->hMutex);
#else
    airy_mtx_unlock(&mq->mutex);
#endif

    return AIRY_SUCCESS;
}
