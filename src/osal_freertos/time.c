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
    if (os_is_in_isr()) {
        return (uint32_t)xTaskGetTickCountFromISR();
    }
    return (uint32_t)xTaskGetTickCount();
}

uint64_t os_ticks_to_ms(uint64_t ticks)
{
    const uint64_t rate = (uint64_t)configTICK_RATE_HZ;
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
                                     (uint32_t)configTICK_RATE_HZ);
}

uint64_t os_uptime_get(void)
{
    static uint64_t cached_epoch;
    static uint32_t cached_tick;
    uint64_t ticks;
    uint32_t tick;
    os_critical_key_t key;

    if (!os_is_in_isr()) {
        TimeOut_t state;
        uint64_t tick_range = (uint64_t)portMAX_DELAY + 1U;

        vTaskSetTimeOutState(&state);
        tick = (uint32_t)state.xTimeOnEntering;
        ticks = (uint64_t)(UBaseType_t)state.xOverflowCount * tick_range +
                tick;
        key = os_enter_critical();
        cached_epoch = ticks - tick;
        cached_tick = tick;
        os_exit_critical(key);
        return os_ticks_to_ms(ticks);
    }

    tick = (uint32_t)xTaskGetTickCountFromISR();
    key = os_enter_critical();
    if (tick < cached_tick) {
        cached_epoch += (uint64_t)portMAX_DELAY + 1U;
    }
    cached_tick = tick;
    ticks = cached_epoch + tick;
    os_exit_critical(key);
    return os_ticks_to_ms(ticks);
}

void os_delay(uint32_t ticks)
{
    if (!os_is_in_isr()) {
        vTaskDelay((TickType_t)ticks);
    }
}
