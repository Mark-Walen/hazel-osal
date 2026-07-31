#include "internal.h"

static uint32_t os_scale_ms_to_ticks_ceil(uint64_t milliseconds,
                                          uint32_t ticks_per_second)
{
    const uint64_t max_ticks = (uint64_t)OS_WAIT_FOREVER - 1U;
    uint64_t whole = milliseconds / 1000U;
    uint64_t remainder = milliseconds % 1000U;
    uint64_t ticks;

    if (whole > (max_ticks / ticks_per_second)) {
        return (uint32_t)max_ticks;
    }
    ticks = whole * ticks_per_second;
    ticks += (remainder * ticks_per_second + 999U) / 1000U;
    return (ticks > max_ticks) ? (uint32_t)max_ticks : (uint32_t)ticks;
}

uint32_t os_tick_get(void)
{
    return (uint32_t)rt_tick_get();
}

uint64_t os_ticks_to_ms(uint64_t ticks)
{
    const uint64_t rate = (uint64_t)RT_TICK_PER_SECOND;
    uint64_t whole = ticks / rate;
    uint64_t remainder = ticks % rate;

    if (whole > (UINT64_MAX / 1000U)) {
        return UINT64_MAX;
    }
    return whole * 1000U + (remainder * 1000U) / rate;
}

uint32_t os_ms_to_ticks_ceil(uint64_t milliseconds)
{
    return os_scale_ms_to_ticks_ceil(milliseconds,
                                     (uint32_t)RT_TICK_PER_SECOND);
}

uint64_t os_uptime_get(void)
{
    static uint64_t epoch;
    static uint32_t previous;
    uint32_t current = os_tick_get();
    uint64_t ticks;
    rt_base_t level = rt_hw_interrupt_disable();

    if (current < previous) {
        epoch += UINT64_C(1) << 32;
    }
    previous = current;
    ticks = epoch + current;
    rt_hw_interrupt_enable(level);
    return os_ticks_to_ms(ticks);
}

void os_delay(uint32_t ticks)
{
    if (!os_is_in_isr()) {
        (void)rt_thread_delay((rt_tick_t)ticks);
    }
}
