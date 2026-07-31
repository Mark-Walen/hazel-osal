#include "internal.h"

uint32_t os_tick_get(void)
{
    return (uint32_t)rt_tick_get();
}

void os_delay(uint32_t ticks)
{
    if (!os_is_in_isr()) {
        (void)rt_thread_delay((rt_tick_t)ticks);
    }
}
