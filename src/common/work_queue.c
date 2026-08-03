#include <lynx_wireless/kernel.h>

#include "internal.h"

static void os_work_queue_entry(void *p1, void *p2, void *p3)
{
    struct os_work_queue *queue = p1;

    (void)p2;
    (void)p3;
    for (;;) {
        struct os_fifo_node *node;
        struct os_work *work;

        if (os_fifo_get(&queue->fifo, &node, OS_WAIT_FOREVER) != 0) {
            continue;
        }
        if (node == &queue->stop_node) {
            return;
        }
        work = CONTAINER_OF(node, struct os_work, node);
        work->running = true;
        work->handler(work);
        work->running = false;
    }
}

void os_work_init(struct os_work *work, os_work_handler_t handler)
{
    if (work) {
        os_common_zero(work, sizeof(*work));
        os_fifo_node_init(&work->node);
        work->handler = handler;
    }
}

int os_work_queue_start(struct os_work_queue *queue, const char *name,
                        os_thread_stack_t *stack, size_t stack_size,
                        int priority)
{
    int result;

    if (!queue || !name || stack_size == 0U) {
        return -EINVAL;
    }
    os_common_zero(queue, sizeof(*queue));
    result = os_fifo_init(&queue->fifo);
    if (result != 0) {
        return result;
    }
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

int os_work_submit(struct os_work_queue *queue, struct os_work *work)
{
    os_critical_key_t key;

    if (!queue || !queue->started || !work || !work->handler) {
        return -EINVAL;
    }
    key = os_enter_critical();
    if (queue->stopping || work->node.queued) {
        os_exit_critical(key);
        return -EBUSY;
    }
    if (queue->fifo.count >= UINT16_MAX) {
        os_exit_critical(key);
        return -ENOSPC;
    }
    work->node.queued = true;
    sys_dlist_append(&queue->fifo.list, &work->node.node);
    ++queue->fifo.count;
    os_exit_critical(key);
    os_sem_give(&queue->fifo.items);
    return 0;
}

int os_work_queue_stop(struct os_work_queue *queue, uint32_t timeout)
{
    os_critical_key_t key;
    bool first_stop;
    int result;

    if (!queue || !queue->started) {
        return -EINVAL;
    }
    if (os_get_current_thread() == &queue->thread) {
        return -EDEADLK;
    }
    key = os_enter_critical();
    first_stop = !queue->stopping;
    if (first_stop) {
        if (queue->fifo.count >= UINT16_MAX) {
            os_exit_critical(key);
            return -ENOSPC;
        }
        queue->stopping = true;
        queue->stop_node.queued = true;
        sys_dlist_append(&queue->fifo.list, &queue->stop_node.node);
        ++queue->fifo.count;
    }
    os_exit_critical(key);
    if (first_stop) {
        os_sem_give(&queue->fifo.items);
    }
    result = os_thread_join(&queue->thread, timeout);
    if (result != 0) {
        return result;
    }
    result = os_fifo_deinit(&queue->fifo);
    if (result != 0) {
        return result;
    }
    queue->started = false;
    return 0;
}
