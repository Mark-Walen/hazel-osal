#include <hazel_wireless/kernel.h>

#include <zephyr/kernel.h>

#include <string.h>

int os_event_init(struct os_event *event)
{
    if (!event) {
        return -EINVAL;
    }
    memset(event, 0, sizeof(*event));
    k_event_init(&event->storage);
    event->handle = &event->storage;
    event->initialized = true;
    return 0;
}

int os_event_deinit(struct os_event *event)
{
    if (!event || !event->initialized || !event->handle) {
        return -EINVAL;
    }
    if (k_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    memset(event, 0, sizeof(*event));
    return 0;
}

int os_event_set(struct os_event *event, uint32_t bits)
{
    if (!event || !event->initialized || !event->handle ||
        bits == 0U || (bits & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (k_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    (void)k_event_post(&event->storage, bits);
    return 0;
}

int os_event_clear(struct os_event *event, uint32_t bits)
{
    if (!event || !event->initialized || !event->handle ||
        (bits & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (k_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    (void)k_event_clear(&event->storage, bits);
    return 0;
}

int os_event_wait(struct os_event *event, uint32_t requested, bool wait_all,
                  bool clear, uint32_t timeout, uint32_t *received)
{
    k_timeout_t wait;
    uint32_t result;

    if (!event || !event->initialized || !event->handle || !received ||
        requested == 0U || (requested & ~OS_EVENT_BITS_MASK) != 0U) {
        return -EINVAL;
    }
    if (k_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    wait = (timeout == OS_WAIT_FOREVER) ? K_FOREVER :
           ((timeout == 0U) ? K_NO_WAIT : K_TICKS(timeout));
    result = wait_all ?
             k_event_wait_all(&event->storage, requested, clear, wait) :
             k_event_wait(&event->storage, requested, clear, wait);
    *received = result & requested;
    if (wait_all ? (*received == requested) : (*received != 0U)) {
        return 0;
    }
    return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
}
