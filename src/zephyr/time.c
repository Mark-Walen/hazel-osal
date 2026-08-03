#include <hazel_wireless/kernel.h>

#include <zephyr/kernel.h>

uint32_t os_tick_get(void)
{
    return (uint32_t)k_uptime_ticks();
}

uint64_t os_uptime_get(void)
{
    return (uint64_t)k_uptime_get();
}

uint32_t os_ms_to_ticks_ceil(uint64_t milliseconds)
{
    uint64_t ticks = k_ms_to_ticks_ceil64(milliseconds);

    return (ticks >= OS_WAIT_FOREVER) ?
           (OS_WAIT_FOREVER - 1U) : (uint32_t)ticks;
}

uint64_t os_ticks_to_ms(uint64_t ticks)
{
    return k_ticks_to_ms_floor64(ticks);
}

void os_delay(uint32_t ticks)
{
    if (!k_is_in_isr()) {
        k_sleep(K_TICKS(ticks));
    }
}
