#include "internal.h"

int os_event_init(struct os_event *event)
{
    char name[RT_NAME_MAX];

    if (!event) {
        return -EINVAL;
    }
    rt_memset(event, 0, sizeof(*event));
    os_rt_name_generate(name, "event", event, RT_NULL);
    if (rt_event_init(&event->storage, name, RT_IPC_FLAG_PRIO) != RT_EOK) {
        return -ENOMEM;
    }
    event->handle = &event->storage;
    event->initialized = true;
    return 0;
}

int os_event_deinit(struct os_event *event)
{
    rt_base_t level;
    bool busy;

    if (!event || !event->initialized || !event->handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    level = rt_hw_interrupt_disable();
    busy = !rt_list_isempty(&event->storage.parent.suspend_thread);
    rt_hw_interrupt_enable(level);
    if (busy) {
        return -EBUSY;
    }
    if (rt_event_detach(&event->storage) != RT_EOK) {
        return -EBUSY;
    }
    rt_memset(event, 0, sizeof(*event));
    return 0;
}

int os_event_set(struct os_event *event, uint32_t bits)
{
    if (!event || !event->initialized || !event->handle ||
        bits == 0U || (bits & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    return (rt_event_send(&event->storage, (rt_uint32_t)bits) == RT_EOK) ?
           0 : -EINVAL;
}

int os_event_clear(struct os_event *event, uint32_t bits)
{
    rt_base_t level;

    if (!event || !event->initialized || !event->handle ||
        (bits & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    level = rt_hw_interrupt_disable();
    event->storage.set &= ~(rt_uint32_t)bits;
    rt_hw_interrupt_enable(level);
    return 0;
}

int os_event_wait(struct os_event *event, uint32_t requested, bool wait_all,
                  bool clear, uint32_t timeout, uint32_t *received)
{
    rt_uint8_t option;
    rt_uint32_t result = 0;
    rt_err_t status;

    if (!event || !event->initialized || !event->handle || !received ||
        requested == 0U || (requested & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    option = wait_all ? RT_EVENT_FLAG_AND : RT_EVENT_FLAG_OR;
    if (clear) {
        option |= RT_EVENT_FLAG_CLEAR;
    }
    status = rt_event_recv(&event->storage, requested, option,
                           os_to_rt_timeout(timeout), &result);
    *received = result & requested;
    if (status == RT_EOK) {
        return 0;
    }
    return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
}
