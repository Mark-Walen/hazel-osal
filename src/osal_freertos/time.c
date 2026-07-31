#include "internal.h"

uint32_t os_tick_get(void)
{
    if (os_is_in_isr()) {
        return (uint32_t)xTaskGetTickCountFromISR();
    }
    return (uint32_t)xTaskGetTickCount();
}

void os_delay(uint32_t ticks)
{
    if (!os_is_in_isr()) {
        vTaskDelay((TickType_t)ticks);
    }
}
