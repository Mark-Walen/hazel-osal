#include "internal.h"

void os_waitq_init(os_wait_q_t *q)
{
    if (q != NULL) {
        sys_dlist_init(&q->waitq);
    }
}

void os_rt_waitq_insert_locked(os_wait_q_t *q, struct os_thread *thread)
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
    os_critical_key_t key;
    rt_err_t result;

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
    os_rt_waitq_insert_locked(q, thread);
    os_exit_critical(key);

    result = rt_sem_take(&thread->wait_storage, os_to_rt_timeout(timeout));

    key = os_enter_critical();
    if (thread->wait_q != NULL) {
        sys_dlist_remove(&thread->wait_node);
        thread->wait_q = NULL;
        thread->state = OS_THREAD_READY;
        os_exit_critical(key);
        return (result == -RT_ETIMEOUT) ? -ETIMEDOUT : -EAGAIN;
    }

    thread->state = OS_THREAD_READY;
    result = (rt_err_t)thread->wait_result;
    os_exit_critical(key);
    return (int)result;
}

void os_waitq_wake_one(os_wait_q_t *q, uint32_t reason)
{
    os_critical_key_t key;

    if (!q) {
        return;
    }
    key = os_enter_critical();
    if (!sys_dlist_is_empty(&q->waitq)) {
        struct os_thread *thread =
            CONTAINER_OF(sys_dlist_get(&q->waitq),
                         struct os_thread, wait_node);

        thread->wait_q = NULL;
        thread->wait_result = reason;
        thread->state = OS_THREAD_READY;
        (void)rt_sem_release(&thread->wait_storage);
    }
    os_exit_critical(key);
}

void os_waitq_wake_all(os_wait_q_t *q, uint32_t reason)
{
    os_critical_key_t key;

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
        (void)rt_sem_release(&thread->wait_storage);
    }
    os_exit_critical(key);
}
