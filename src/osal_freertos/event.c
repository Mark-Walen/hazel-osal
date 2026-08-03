#include "internal.h"

#include "event_groups.h"

#include <string.h>

int os_event_init(struct os_event *event)
{
    EventGroupHandle_t handle;

    if (!event) {
        return -EINVAL;
    }
    memset(event, 0, sizeof(*event));
    handle = xEventGroupCreateStatic((StaticEventGroup_t *)&event->storage);
    if (!handle) {
        return -ENOMEM;
    }
    event->handle = handle;
    event->initialized = true;
    return 0;
}

int os_event_deinit(struct os_event *event)
{
    if (!event || !event->initialized || !event->handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    vEventGroupDelete((EventGroupHandle_t)event->handle);
    memset(event, 0, sizeof(*event));
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
    (void)xEventGroupSetBits((EventGroupHandle_t)event->handle,
                             (EventBits_t)bits);
    return 0;
}

int os_event_clear(struct os_event *event, uint32_t bits)
{
    if (!event || !event->initialized || !event->handle ||
        (bits & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    (void)xEventGroupClearBits((EventGroupHandle_t)event->handle,
                               (EventBits_t)bits);
    return 0;
}

int os_event_wait(struct os_event *event, uint32_t requested, bool wait_all,
                  bool clear, uint32_t timeout, uint32_t *received)
{
    EventBits_t bits;
    bool matched;

    if (!event || !event->initialized || !event->handle || !received ||
        requested == 0U || (requested & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    bits = xEventGroupWaitBits((EventGroupHandle_t)event->handle,
                               (EventBits_t)requested,
                               clear ? pdTRUE : pdFALSE,
                               wait_all ? pdTRUE : pdFALSE,
                               (timeout == OS_WAIT_FOREVER) ?
                               portMAX_DELAY : (TickType_t)timeout);
    *received = (uint32_t)bits & requested;
    matched = wait_all ? (*received == requested) : (*received != 0U);
    if (matched) {
        return 0;
    }
    return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
}
