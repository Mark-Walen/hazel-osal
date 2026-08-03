/**
 * @file fifo.c
 * @brief Intrusive zero-copy FIFO implementation.
 */
#include <hazel_wireless/kernel.h>

#include "internal.h"

int os_fifo_init(struct os_fifo *fifo)
{
    if (!fifo) {
        return -EINVAL;
    }
    os_common_zero(fifo, sizeof(*fifo));
    sys_dlist_init(&fifo->list);
    if (os_sem_init(&fifo->items, 0, UINT16_MAX) != 0) {
        return -ENOMEM;
    }
    fifo->initialized = true;
    return 0;
}

int os_fifo_deinit(struct os_fifo *fifo)
{
    os_critical_key_t key;
    int result;

    if (!fifo || !fifo->initialized) {
        return -EINVAL;
    }
    key = os_enter_critical();
    if (!sys_dlist_is_empty(&fifo->list)) {
        os_exit_critical(key);
        return -EBUSY;
    }
    os_exit_critical(key);
    result = os_sem_deinit(&fifo->items);
    if (result == 0) {
        os_common_zero(fifo, sizeof(*fifo));
    }
    return result;
}

void os_fifo_node_init(struct os_fifo_node *node)
{
    if (node) {
        os_common_zero(node, sizeof(*node));
    }
}

int os_fifo_put(struct os_fifo *fifo, struct os_fifo_node *node)
{
    os_critical_key_t key;

    if (!fifo || !fifo->initialized || !node) {
        return -EINVAL;
    }
    key = os_enter_critical();
    if (node->queued) {
        os_exit_critical(key);
        return -EBUSY;
    }
    if (fifo->count >= UINT16_MAX) {
        os_exit_critical(key);
        return -ENOSPC;
    }
    node->queued = true;
    sys_dlist_append(&fifo->list, &node->node);
    ++fifo->count;
    os_exit_critical(key);
    os_sem_give(&fifo->items);
    return 0;
}

int os_fifo_get(struct os_fifo *fifo, struct os_fifo_node **node,
                uint32_t timeout)
{
    os_critical_key_t key;
    sys_dnode_t *entry;
    int result;

    if (!fifo || !fifo->initialized || !node) {
        return -EINVAL;
    }
    result = os_sem_take(&fifo->items, timeout);
    if (result != 0) {
        return result;
    }
    key = os_enter_critical();
    entry = sys_dlist_get(&fifo->list);
    if (entry == NULL) {
        os_exit_critical(key);
        return -EAGAIN;
    }
    *node = CONTAINER_OF(entry, struct os_fifo_node, node);
    (*node)->queued = false;
    --fifo->count;
    os_exit_critical(key);
    return 0;
}
