#include "internal.h"

#include <string.h>

#define OS_PI_MAX_CHAIN_DEPTH 16U

void os_mutex_init(struct os_mutex *mutex)
{
    if (mutex != NULL) {
        memset(mutex, 0, sizeof(*mutex));
        os_waitq_init(&mutex->wait_q);
        mutex->initialized = true;
    }
}

int os_mutex_deinit(struct os_mutex *mutex)
{
    os_critical_key_t key;

    if (!mutex || !mutex->initialized) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    key = os_enter_critical();
    if ((mutex->owner != NULL) ||
        !sys_dlist_is_empty(&mutex->wait_q.waitq)) {
        os_exit_critical(key);
        return -EBUSY;
    }
    mutex->initialized = false;
    os_exit_critical(key);
    memset(mutex, 0, sizeof(*mutex));
    return 0;
}

static int os_mutex_compute_priority_locked(struct os_thread *thread)
{
    sys_dnode_t *mutex_node;
    int effective_priority;

    if (!thread || !thread->handle) {
        return -EINVAL;
    }

    effective_priority = thread->base_priority;
    SYS_DLIST_FOR_EACH_NODE(&thread->owned_mutexes, mutex_node) {
        struct os_mutex *owned =
            CONTAINER_OF(mutex_node, struct os_mutex, owner_node);
        sys_dnode_t *waiter_node;

        SYS_DLIST_FOR_EACH_NODE(&owned->wait_q.waitq, waiter_node) {
            struct os_thread *waiter =
                CONTAINER_OF(waiter_node, struct os_thread, wait_node);
            int waiter_priority = os_thread_get_priority(waiter);

            if (waiter_priority > effective_priority) {
                effective_priority = waiter_priority;
            }
        }
    }
    return effective_priority;
}

void os_mutex_update_pi_chain_locked(struct os_thread *thread)
{
    struct os_thread *visited[OS_PI_MAX_CHAIN_DEPTH];
    size_t depth = 0;

    while ((thread != NULL) && (depth < OS_PI_MAX_CHAIN_DEPTH)) {
        struct os_mutex *upstream;
        int effective_priority;
        size_t i;

        for (i = 0; i < depth; ++i) {
            if (visited[i] == thread) {
                return;
            }
        }
        visited[depth++] = thread;

        effective_priority = os_mutex_compute_priority_locked(thread);
        if ((effective_priority >= 0) &&
            (os_thread_get_priority(thread) != effective_priority)) {
            vTaskPrioritySet((TaskHandle_t)thread->handle,
                             (UBaseType_t)effective_priority);
        }

        upstream = thread->waiting_mutex;
        if (upstream == NULL) {
            return;
        }

        sys_dlist_remove(&thread->wait_node);
        os_waitq_insert_priority_locked(&upstream->wait_q, thread);
        thread = upstream->owner;
    }
}

static bool os_mutex_would_deadlock_locked(struct os_thread *thread,
                                           struct os_mutex *mutex)
{
    struct os_thread *owner = mutex->owner;
    size_t depth;

    for (depth = 0; owner != NULL && depth < OS_PI_MAX_CHAIN_DEPTH; ++depth) {
        if (owner == thread) {
            return true;
        }
        if (owner->waiting_mutex == NULL) {
            return false;
        }
        owner = owner->waiting_mutex->owner;
    }
    return owner != NULL;
}

int os_mutex_lock(struct os_mutex *mutex, uint32_t timeout)
{
    struct os_thread *curr;
    uint32_t start;

    if (!mutex || !mutex->initialized) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    curr = os_get_current_thread();
    if (!curr) {
        return -EINVAL;
    }
    start = os_tick_get();

    for (;;) {
        uint32_t remaining = OS_WAIT_FOREVER;
        uint32_t notify = 0;
        BaseType_t notified;
        os_critical_key_t key = os_enter_critical();

        if (mutex->owner == NULL) {
            mutex->owner = curr;
            mutex->lock_count = 1;
            sys_dlist_append(&curr->owned_mutexes, &mutex->owner_node);
            os_exit_critical(key);
            return 0;
        }
        if (mutex->owner == curr) {
            ++mutex->lock_count;
            os_exit_critical(key);
            return 0;
        }
        if (timeout == 0U) {
            os_exit_critical(key);
            return -EBUSY;
        }
        if (os_mutex_would_deadlock_locked(curr, mutex)) {
            os_exit_critical(key);
            return -EDEADLK;
        }

        if (timeout != OS_WAIT_FOREVER) {
            uint32_t elapsed = os_tick_get() - start;

            if (elapsed >= timeout) {
                os_exit_critical(key);
                return -ETIMEDOUT;
            }
            remaining = timeout - elapsed;
        }

        curr->wait_q = &mutex->wait_q;
        curr->waiting_mutex = mutex;
        curr->wait_result = 0;
        curr->state = OS_THREAD_PENDING;
        os_waitq_insert_priority_locked(&mutex->wait_q, curr);
        os_mutex_update_pi_chain_locked(mutex->owner);
        os_exit_critical(key);

        notified = xTaskNotifyWait(
            0, UINT32_MAX, &notify,
            (remaining == OS_WAIT_FOREVER) ?
            portMAX_DELAY : (TickType_t)remaining);

        key = os_enter_critical();
        if (curr->wait_q != NULL) {
            struct os_thread *owner = mutex->owner;

            sys_dlist_remove(&curr->wait_node);
            curr->wait_q = NULL;
            curr->waiting_mutex = NULL;
            curr->state = OS_THREAD_READY;
            os_mutex_update_pi_chain_locked(owner);
            os_exit_critical(key);
            return (notified == pdTRUE) ? -EAGAIN : -ETIMEDOUT;
        }

        curr->state = OS_THREAD_READY;
        curr->waiting_mutex = NULL;
        os_exit_critical(key);
    }
}

int os_mutex_trylock(struct os_mutex *mutex)
{
    return os_mutex_lock(mutex, 0U);
}

int os_mutex_unlock(struct os_mutex *mutex)
{
    struct os_thread *curr;
    os_critical_key_t key;

    if (!mutex || !mutex->initialized) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    curr = os_get_current_thread();
    if (!curr) {
        return -EINVAL;
    }

    key = os_enter_critical();
    if (mutex->owner != curr) {
        os_exit_critical(key);
        return -EPERM;
    }
    if (mutex->lock_count == 0U) {
        os_exit_critical(key);
        return -EINVAL;
    }
    if (--mutex->lock_count > 0U) {
        os_exit_critical(key);
        return 0;
    }

    sys_dlist_remove(&mutex->owner_node);
    mutex->owner = NULL;

    if (!sys_dlist_is_empty(&mutex->wait_q.waitq)) {
        struct os_thread *thread =
            CONTAINER_OF(sys_dlist_get(&mutex->wait_q.waitq),
                         struct os_thread, wait_node);

        thread->wait_q = NULL;
        thread->waiting_mutex = NULL;
        thread->wait_result = 0;
        thread->state = OS_THREAD_READY;
        (void)os_waitq_notify_thread_locked(thread, 0);
    }

    os_mutex_update_pi_chain_locked(curr);
    os_exit_critical(key);
    return 0;
}
