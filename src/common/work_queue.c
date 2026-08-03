/**
 * @file work_queue.c
 * @brief RTOS-independent immediate and delayable work queue implementation.
 *
 * One worker thread owns handler execution. Immediate work uses the FIFO;
 * delayable work is held on a protected list until the worker's timed wait
 * reaches the nearest deadline. All public state transitions are serialized
 * by the OSAL critical-section API.
 */
#include <lynx_wireless/kernel.h>

#include "internal.h"

#define OS_WORK_MAGIC UINT32_C(0x6f73776b)

static void os_work_notify(struct os_work *work)
{
    if (work->sync_initialized) {
        os_sem_give(&work->sync);
    }
}

static uint32_t os_work_deadline_remaining(uint32_t deadline, uint32_t now)
{
    /* Signed subtraction preserves ordering across one 32-bit tick wrap. */
    int32_t remaining = (int32_t)(deadline - now);

    return remaining > 0 ? (uint32_t)remaining : 0U;
}

static uint32_t os_work_promote_delayed(struct os_work_queue *queue)
{
    for (;;) {
        struct os_work_delayable *due = NULL;
        struct os_work_delayable *dwork;
        uint32_t timeout = OS_WAIT_FOREVER;
        uint32_t now = os_tick_get();
        bool blocked = false;
        os_critical_key_t key = os_enter_critical();

        SYS_DLIST_FOR_EACH_CONTAINER(&queue->delayed, dwork, delay_node) {
            uint32_t remaining = os_work_deadline_remaining(dwork->deadline,
                                                            now);

            if (remaining == 0U) {
                due = dwork;
                break;
            }
            if (timeout == OS_WAIT_FOREVER || remaining < timeout) {
                timeout = remaining;
            }
        }
        if (due != NULL) {
            sys_dlist_remove(&due->delay_node);
            due->work.flags &= ~OS_WORK_DELAYED;
            blocked = queue->plugged || queue->stopping;
        }
        os_exit_critical(key);

        if (due == NULL) {
            return timeout;
        }
        if (!blocked) {
            (void)os_work_submit_to_queue(queue, &due->work);
        } else {
            os_work_notify(&due->work);
        }
    }
}

static void os_work_queue_entry(void *p1, void *p2, void *p3)
{
    struct os_work_queue *queue = p1;

    (void)p2;
    (void)p3;
    for (;;) {
        struct os_fifo_node *node;
        sys_dnode_t *entry;
        struct os_work *work;
        os_critical_key_t key;
        uint32_t timeout = os_work_promote_delayed(queue);

        if (os_sem_take(&queue->fifo.items, timeout) != 0) {
            continue;
        }
        key = os_enter_critical();
        entry = sys_dlist_get(&queue->fifo.list);
        if (entry == NULL) {
            os_exit_critical(key);
            continue;
        }
        node = CONTAINER_OF(entry, struct os_fifo_node, node);
        node->queued = false;
        --queue->fifo.count;
        if (node == &queue->stop_node) {
            os_exit_critical(key);
            return;
        }
        work = CONTAINER_OF(node, struct os_work, node);
        work->flags &= ~OS_WORK_QUEUED;
        work->flags |= OS_WORK_RUNNING;
        work->running_id = work->submission_id;
        queue->current = work;
        os_exit_critical(key);

        work->handler(work);

        key = os_enter_critical();
        queue->current = NULL;
        work->completion_id = work->running_id;
        work->flags &= ~(OS_WORK_RUNNING | OS_WORK_CANCELING);
        os_exit_critical(key);
        os_work_notify(work);
        if (!queue->no_yield) {
            os_thread_yield();
        }
    }
}

void os_work_init(struct os_work *work, os_work_handler_t handler)
{
    if (!work) {
        return;
    }
    os_common_zero(work, sizeof(*work));
    os_fifo_node_init(&work->node);
    work->handler = handler;
    work->sync_initialized = (os_sem_init(&work->sync, 0, 1) == 0);
    work->magic = OS_WORK_MAGIC;
}

int os_work_queue_start(struct os_work_queue *queue,
                        os_thread_stack_t *stack, size_t stack_size,
                        int priority,
                        const struct os_work_queue_config *config)
{
    const char *name = (config && config->name) ? config->name : "workq";
    int result;

    if (!queue || stack_size == 0U) {
        return -EINVAL;
    }
    if (queue->started) {
        return -EALREADY;
    }
    os_common_zero(queue, sizeof(*queue));
    result = os_fifo_init(&queue->fifo);
    if (result != 0) {
        return result;
    }
    sys_dlist_init(&queue->delayed);
    queue->no_yield = config ? config->no_yield : false;
    result = os_thread_create(&queue->thread, name, stack, stack_size,
                              os_work_queue_entry, queue, NULL, NULL,
                              priority, 0);
    if (result != 0) {
        (void)os_fifo_deinit(&queue->fifo);
        return result;
    }
    queue->started = true;
    return 0;
}

int os_work_submit_to_queue(struct os_work_queue *queue,
                            struct os_work *work)
{
    os_critical_key_t key;
    int result;

    if (!work || work->magic != OS_WORK_MAGIC || !work->handler ||
        !work->sync_initialized) {
        return -EINVAL;
    }
    if (queue == NULL) {
        queue = work->queue;
    }
    if (queue == NULL) {
        return -EINVAL;
    }
    if (!queue->started) {
        return -ENODEV;
    }
    key = os_enter_critical();
    if (queue->stopping ||
        ((queue->draining || queue->plugged) &&
         os_get_current_thread() != &queue->thread) ||
        (work->flags & OS_WORK_CANCELING) != 0U) {
        os_exit_critical(key);
        return -EBUSY;
    }
    if ((work->flags & OS_WORK_QUEUED) != 0U) {
        result = (work->queue == queue) ? 0 : -EBUSY;
        os_exit_critical(key);
        return result;
    }
    if ((work->flags & OS_WORK_RUNNING) != 0U && work->queue != queue) {
        os_exit_critical(key);
        return -EBUSY;
    }
    if (queue->fifo.count >= UINT16_MAX) {
        os_exit_critical(key);
        return -ENOSPC;
    }
    result = ((work->flags & OS_WORK_RUNNING) != 0U) ? 2 : 1;
    ++work->submission_id;
    work->queue = queue;
    work->flags |= OS_WORK_QUEUED;
    work->node.queued = true;
    sys_dlist_append(&queue->fifo.list, &work->node.node);
    ++queue->fifo.count;
    os_exit_critical(key);
    os_sem_give(&queue->fifo.items);
    return result;
}

int os_work_submit(struct os_work_queue *queue, struct os_work *work)
{
    return os_work_submit_to_queue(queue, work);
}

uint32_t os_work_busy_get(const struct os_work *work)
{
    uint32_t flags;
    os_critical_key_t key;

    if (!work || work->magic != OS_WORK_MAGIC) {
        return 0U;
    }
    key = os_enter_critical();
    flags = work->flags;
    os_exit_critical(key);
    return flags;
}

bool os_work_is_pending(const struct os_work *work)
{
    return os_work_busy_get(work) != 0U;
}

int os_work_cancel(struct os_work *work)
{
    struct os_work_queue *queue;
    os_critical_key_t key;
    bool was_queued = false;
    uint32_t flags;

    if (!work || work->magic != OS_WORK_MAGIC ||
        !work->sync_initialized) {
        return -EINVAL;
    }
    key = os_enter_critical();
    queue = work->queue;
    if ((work->flags & OS_WORK_QUEUED) != 0U && queue != NULL) {
        sys_dlist_remove(&work->node.node);
        work->node.queued = false;
        --queue->fifo.count;
        work->flags &= ~OS_WORK_QUEUED;
        was_queued = true;
    }
    if ((work->flags & OS_WORK_RUNNING) != 0U) {
        work->flags |= OS_WORK_CANCELING;
    } else {
        work->completion_id = work->submission_id;
        work->flags &= ~OS_WORK_CANCELING;
    }
    flags = work->flags;
    os_exit_critical(key);
    if (was_queued && queue != NULL) {
        (void)os_sem_trytake(&queue->fifo.items);
    }
    os_work_notify(work);
    return (int)flags;
}

bool os_work_cancel_sync(struct os_work *work, struct os_work_sync *sync)
{
    bool was_pending;
    os_critical_key_t key;

    if (!work || !sync || work->magic != OS_WORK_MAGIC ||
        !work->sync_initialized) {
        return false;
    }
    key = os_enter_critical();
    was_pending = work->flags != 0U;
    os_exit_critical(key);
    (void)os_work_cancel(work);
    while (os_work_busy_get(work) != 0U) {
        (void)os_sem_take(&work->sync, OS_WAIT_FOREVER);
    }
    return was_pending;
}

bool os_work_flush(struct os_work *work, struct os_work_sync *sync)
{
    uint32_t target;
    bool waited;
    os_critical_key_t key;

    if (!work || !sync || work->magic != OS_WORK_MAGIC ||
        !work->sync_initialized) {
        return false;
    }
    key = os_enter_critical();
    target = work->submission_id;
    waited = work->completion_id != target;
    os_exit_critical(key);
    for (;;) {
        bool completed;

        key = os_enter_critical();
        completed = work->completion_id == target;
        os_exit_critical(key);
        if (completed) {
            break;
        }
        (void)os_sem_take(&work->sync, OS_WAIT_FOREVER);
    }
    return waited;
}

void os_work_init_delayable(struct os_work_delayable *dwork,
                            os_work_handler_t handler)
{
    if (!dwork) {
        return;
    }
    os_common_zero(dwork, sizeof(*dwork));
    os_work_init(&dwork->work, handler);
    sys_dnode_init(&dwork->delay_node);
}

struct os_work_delayable *os_work_delayable_from_work(struct os_work *work)
{
    return work ? CONTAINER_OF(work, struct os_work_delayable, work) : NULL;
}

uint32_t os_work_delayable_busy_get(const struct os_work_delayable *dwork)
{
    return dwork ? os_work_busy_get(&dwork->work) : 0U;
}

bool os_work_delayable_is_pending(const struct os_work_delayable *dwork)
{
    return os_work_delayable_busy_get(dwork) != 0U;
}

uint32_t os_work_delayable_expires_get(const struct os_work_delayable *dwork)
{
    uint32_t expires;
    os_critical_key_t key;

    if (!dwork) {
        return os_tick_get();
    }
    key = os_enter_critical();
    expires = (dwork->work.flags & OS_WORK_DELAYED) != 0U ?
              dwork->deadline : os_tick_get();
    os_exit_critical(key);
    return expires;
}

uint32_t os_work_delayable_remaining_get(
    const struct os_work_delayable *dwork)
{
    uint32_t remaining = 0U;
    os_critical_key_t key;

    if (!dwork) {
        return 0U;
    }
    key = os_enter_critical();
    if ((dwork->work.flags & OS_WORK_DELAYED) != 0U) {
        remaining = os_work_deadline_remaining(dwork->deadline,
                                                os_tick_get());
    }
    os_exit_critical(key);
    return remaining;
}

static int os_work_schedule_common(struct os_work_queue *queue,
                                   struct os_work_delayable *dwork,
                                   uint32_t delay, bool reschedule)
{
    struct os_work_queue *old_queue = NULL;
    os_critical_key_t key;

    if (!queue || !dwork || dwork->work.magic != OS_WORK_MAGIC ||
        delay > (uint32_t)INT32_MAX) {
        return -EINVAL;
    }
    if (!queue->started) {
        return -ENODEV;
    }
    key = os_enter_critical();
    if ((dwork->work.flags & OS_WORK_CANCELING) != 0U ||
        queue->stopping ||
        ((queue->draining || queue->plugged) &&
         os_get_current_thread() != &queue->thread)) {
        os_exit_critical(key);
        return -EBUSY;
    }
    if (!reschedule &&
        (dwork->work.flags & (OS_WORK_DELAYED | OS_WORK_QUEUED)) != 0U) {
        os_exit_critical(key);
        return 0;
    }
    if ((dwork->work.flags & OS_WORK_DELAYED) != 0U) {
        old_queue = dwork->queue;
        sys_dlist_remove(&dwork->delay_node);
        dwork->work.flags &= ~OS_WORK_DELAYED;
    }
    if (delay == 0U) {
        os_exit_critical(key);
        if (old_queue != NULL) {
            os_sem_give(&old_queue->fifo.items);
        }
        return os_work_submit_to_queue(queue, &dwork->work);
    }
    dwork->queue = queue;
    dwork->deadline = os_tick_get() + delay;
    dwork->work.flags |= OS_WORK_DELAYED;
    sys_dlist_append(&queue->delayed, &dwork->delay_node);
    os_exit_critical(key);

    if (old_queue != NULL && old_queue != queue) {
        os_sem_give(&old_queue->fifo.items);
    }
    os_sem_give(&queue->fifo.items);
    return 1;
}

int os_work_schedule_for_queue(struct os_work_queue *queue,
                               struct os_work_delayable *dwork,
                               uint32_t delay)
{
    return os_work_schedule_common(queue, dwork, delay, false);
}

int os_work_reschedule_for_queue(struct os_work_queue *queue,
                                 struct os_work_delayable *dwork,
                                 uint32_t delay)
{
    return os_work_schedule_common(queue, dwork, delay, true);
}

int os_work_schedule(struct os_work_queue *queue,
                     struct os_work_delayable *dwork, uint32_t delay)
{
    return os_work_schedule_for_queue(queue, dwork, delay);
}

int os_work_reschedule(struct os_work_queue *queue,
                       struct os_work_delayable *dwork, uint32_t delay)
{
    return os_work_reschedule_for_queue(queue, dwork, delay);
}

int os_work_cancel_delayable(struct os_work_delayable *dwork)
{
    struct os_work_queue *queue = NULL;
    os_critical_key_t key;

    if (!dwork || dwork->work.magic != OS_WORK_MAGIC) {
        return -EINVAL;
    }
    key = os_enter_critical();
    if ((dwork->work.flags & OS_WORK_DELAYED) != 0U) {
        queue = dwork->queue;
        sys_dlist_remove(&dwork->delay_node);
        dwork->work.flags &= ~OS_WORK_DELAYED;
    }
    os_exit_critical(key);
    if (queue != NULL) {
        os_sem_give(&queue->fifo.items);
    }
    return os_work_cancel(&dwork->work);
}

bool os_work_cancel_delayable_sync(struct os_work_delayable *dwork,
                                   struct os_work_sync *sync)
{
    bool was_pending;

    if (!dwork || !sync) {
        return false;
    }
    was_pending = os_work_delayable_is_pending(dwork);
    (void)os_work_cancel_delayable(dwork);
    while (os_work_delayable_is_pending(dwork)) {
        (void)os_sem_take(&dwork->work.sync, OS_WAIT_FOREVER);
    }
    return was_pending;
}

bool os_work_flush_delayable(struct os_work_delayable *dwork,
                             struct os_work_sync *sync)
{
    struct os_work_queue *queue = NULL;
    bool was_delayed = false;
    os_critical_key_t key;

    if (!dwork || !sync || dwork->work.magic != OS_WORK_MAGIC) {
        return false;
    }
    key = os_enter_critical();
    if ((dwork->work.flags & OS_WORK_DELAYED) != 0U) {
        queue = dwork->queue;
        sys_dlist_remove(&dwork->delay_node);
        dwork->work.flags &= ~OS_WORK_DELAYED;
        was_delayed = true;
    }
    os_exit_critical(key);
    if (was_delayed) {
        os_sem_give(&queue->fifo.items);
        (void)os_work_submit_to_queue(queue, &dwork->work);
    }
    return os_work_flush(&dwork->work, sync) || was_delayed;
}

int os_work_queue_drain(struct os_work_queue *queue, bool plug)
{
    os_critical_key_t key;
    bool waited;

    if (!queue || !queue->started || os_is_in_isr()) {
        return !queue || !queue->started ? -EINVAL : -EWOULDBLOCK;
    }
    key = os_enter_critical();
    queue->draining = true;
    if (plug) {
        queue->plugged = true;
    }
    waited = queue->fifo.count != 0U || queue->current != NULL;
    os_exit_critical(key);
    for (;;) {
        bool drained;

        key = os_enter_critical();
        drained = queue->fifo.count == 0U && queue->current == NULL;
        if (drained) {
            queue->draining = false;
        }
        os_exit_critical(key);
        if (drained) {
            return waited ? 1 : 0;
        }
        os_delay(1);
    }
}

int os_work_queue_unplug(struct os_work_queue *queue)
{
    os_critical_key_t key;

    if (!queue || !queue->started) {
        return -EINVAL;
    }
    key = os_enter_critical();
    if (!queue->plugged) {
        os_exit_critical(key);
        return -EALREADY;
    }
    queue->plugged = false;
    os_exit_critical(key);
    return 0;
}

int os_work_queue_stop(struct os_work_queue *queue, uint32_t timeout)
{
    os_critical_key_t key;
    int result;

    if (!queue || !queue->started) {
        return -EALREADY;
    }
    if (os_get_current_thread() == &queue->thread) {
        return -EDEADLK;
    }
    key = os_enter_critical();
    if (!queue->plugged || queue->fifo.count != 0U ||
        queue->current != NULL) {
        os_exit_critical(key);
        return -EBUSY;
    }
    queue->stopping = true;
    queue->stop_node.queued = true;
    sys_dlist_append(&queue->fifo.list, &queue->stop_node.node);
    ++queue->fifo.count;
    os_exit_critical(key);
    os_sem_give(&queue->fifo.items);

    result = os_thread_join(&queue->thread, timeout);
    if (result != 0) {
        return result;
    }
    for (;;) {
        struct os_work_delayable *dwork;

        key = os_enter_critical();
        dwork = SYS_DLIST_PEEK_HEAD_CONTAINER(&queue->delayed, dwork,
                                              delay_node);
        if (dwork != NULL) {
            sys_dlist_remove(&dwork->delay_node);
            dwork->work.flags &= ~OS_WORK_DELAYED;
        }
        os_exit_critical(key);
        if (dwork == NULL) {
            break;
        }
        os_work_notify(&dwork->work);
    }
    result = os_fifo_deinit(&queue->fifo);
    if (result != 0) {
        return result;
    }
    queue->started = false;
    return 0;
}

struct os_thread *os_work_queue_thread_get(struct os_work_queue *queue)
{
    return queue ? &queue->thread : NULL;
}
