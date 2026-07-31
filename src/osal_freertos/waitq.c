#include "internal.h"

void os_waitq_init(os_wait_q_t *q)
{
    if (q != NULL) {
        sys_dlist_init(&q->waitq);
    }
}

BaseType_t os_waitq_notify_thread_locked(struct os_thread *thread,
                                         uint32_t reason)
{
    BaseType_t hpw = pdFALSE;

    if (!thread || !thread->handle) {
        return pdFALSE;
    }

    if (os_is_in_isr()) {
        xTaskNotifyFromISR((TaskHandle_t)thread->handle, reason,
                           eSetValueWithOverwrite, &hpw);
    } else {
        (void)xTaskNotify((TaskHandle_t)thread->handle, reason,
                          eSetValueWithOverwrite);
    }
    return hpw;
}

void os_waitq_insert_priority_locked(os_wait_q_t *q,
                                     struct os_thread *thread)
{
    sys_dnode_t *node;
    int priority = os_thread_get_priority(thread);

    SYS_DLIST_FOR_EACH_NODE(&q->waitq, node) {
        struct os_thread *queued =
            CONTAINER_OF(node, struct os_thread, wait_node);

        if (priority > os_thread_get_priority(queued)) {
            break;
        }
    }

    if (node != NULL) {
        sys_dlist_insert(node, &thread->wait_node);
    } else {
        sys_dlist_append(&q->waitq, &thread->wait_node);
    }
}

int os_waitq_block(os_wait_q_t *q, uint32_t timeout)
{
    struct os_thread *thread;
    uint32_t notify = 0;
    BaseType_t result;
    os_critical_key_t key;

    if (!q) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    thread = os_get_current_thread();
    if (!thread) {
        return -EINVAL;
    }

    key = os_enter_critical();
    thread->wait_q = q;
    thread->wait_result = 0;
    thread->state = OS_THREAD_PENDING;
    os_waitq_insert_priority_locked(q, thread);
    os_exit_critical(key);

    result = xTaskNotifyWait(
        0, UINT32_MAX, &notify,
        (timeout == OS_WAIT_FOREVER) ? portMAX_DELAY : (TickType_t)timeout);

    key = os_enter_critical();
    if (thread->wait_q != NULL) {
        sys_dlist_remove(&thread->wait_node);
        thread->wait_q = NULL;
        thread->state = OS_THREAD_READY;
        os_exit_critical(key);
        return -ETIMEDOUT;
    }

    thread->wait_result = notify;
    thread->state = OS_THREAD_READY;
    os_exit_critical(key);
    return (result == pdTRUE) ? (int)notify : -ETIMEDOUT;
}

void os_waitq_wake_one(os_wait_q_t *q, uint32_t reason)
{
    os_critical_key_t key;
    struct os_thread *thread;
    BaseType_t hpw = pdFALSE;

    if (!q) {
        return;
    }

    key = os_enter_critical();
    if (!sys_dlist_is_empty(&q->waitq)) {
        thread = CONTAINER_OF(sys_dlist_get(&q->waitq),
                              struct os_thread, wait_node);
        thread->wait_q = NULL;
        thread->wait_result = reason;
        thread->state = OS_THREAD_READY;
        hpw = os_waitq_notify_thread_locked(thread, reason);
    }
    os_exit_critical(key);

    if (os_is_in_isr() && hpw) {
        portYIELD_FROM_ISR(hpw);
    }
}

void os_waitq_wake_all(os_wait_q_t *q, uint32_t reason)
{
    os_critical_key_t key;
    BaseType_t hpw = pdFALSE;

    if (!q) {
        return;
    }

    key = os_enter_critical();
    while (!sys_dlist_is_empty(&q->waitq)) {
        struct os_thread *thread =
            CONTAINER_OF(sys_dlist_get(&q->waitq),
                         struct os_thread, wait_node);

        thread->wait_q = NULL;
        thread->wait_result = reason;
        thread->state = OS_THREAD_READY;
        hpw |= os_waitq_notify_thread_locked(thread, reason);
    }
    os_exit_critical(key);

    if (os_is_in_isr() && hpw) {
        portYIELD_FROM_ISR(hpw);
    }
}
