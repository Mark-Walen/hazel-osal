/**
 * @file message_queue.c
 * @brief Caller-buffered fixed-size message queue implementation.
 */
#include <lynx_wireless/kernel.h>

#include "internal.h"

int os_msgq_init(struct os_msgq *msgq, void *buffer, size_t message_size,
                 uint32_t capacity)
{
    if (!msgq || !buffer || message_size == 0U || capacity == 0U ||
        message_size > (SIZE_MAX / capacity)) {
        return -EINVAL;
    }
    os_common_zero(msgq, sizeof(*msgq));
    if (os_sem_init(&msgq->items, 0, capacity) != 0) {
        return -ENOMEM;
    }
    if (os_sem_init(&msgq->spaces, capacity, capacity) != 0) {
        (void)os_sem_deinit(&msgq->items);
        return -ENOMEM;
    }
    msgq->buffer = buffer;
    msgq->message_size = message_size;
    msgq->capacity = capacity;
    msgq->initialized = true;
    return 0;
}

int os_msgq_deinit(struct os_msgq *msgq)
{
    int result;

    if (!msgq || !msgq->initialized) {
        return -EINVAL;
    }
    result = os_sem_deinit(&msgq->items);
    if (result != 0) {
        return result;
    }
    result = os_sem_deinit(&msgq->spaces);
    if (result != 0) {
        return result;
    }
    os_common_zero(msgq, sizeof(*msgq));
    return 0;
}

int os_msgq_put(struct os_msgq *msgq, const void *message, uint32_t timeout)
{
    os_critical_key_t key;
    int result;

    if (!msgq || !msgq->initialized || !message) {
        return -EINVAL;
    }
    result = os_sem_take(&msgq->spaces, timeout);
    if (result != 0) {
        return result;
    }
    key = os_enter_critical();
    os_common_copy(msgq->buffer + msgq->write_index * msgq->message_size,
                   message, msgq->message_size);
    msgq->write_index = (msgq->write_index + 1U) % msgq->capacity;
    ++msgq->count;
    os_exit_critical(key);
    os_sem_give(&msgq->items);
    return 0;
}

int os_msgq_get(struct os_msgq *msgq, void *message, uint32_t timeout)
{
    os_critical_key_t key;
    int result;

    if (!msgq || !msgq->initialized || !message) {
        return -EINVAL;
    }
    result = os_sem_take(&msgq->items, timeout);
    if (result != 0) {
        return result;
    }
    key = os_enter_critical();
    os_common_copy(message,
                   msgq->buffer + msgq->read_index * msgq->message_size,
                   msgq->message_size);
    msgq->read_index = (msgq->read_index + 1U) % msgq->capacity;
    --msgq->count;
    os_exit_critical(key);
    os_sem_give(&msgq->spaces);
    return 0;
}

uint32_t os_msgq_count_get(struct os_msgq *msgq)
{
    uint32_t count;
    os_critical_key_t key;

    if (!msgq || !msgq->initialized) {
        return 0U;
    }
    key = os_enter_critical();
    count = msgq->count;
    os_exit_critical(key);
    return count;
}
