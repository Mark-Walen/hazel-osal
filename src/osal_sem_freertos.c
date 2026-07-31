#include <lynx_wireless/kernel.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>

int os_sem_init(struct os_sem *sem, uint32_t initial_count, uint32_t limit)
{
    SemaphoreHandle_t handle;

    if (!sem || (limit == 0U) || (initial_count > limit)) {
        return -EINVAL;
    }

    memset(sem, 0, sizeof(*sem));
    handle = xSemaphoreCreateCountingStatic((UBaseType_t)limit,
                                            (UBaseType_t)initial_count,
                                            (StaticSemaphore_t *)&sem->storage);
    if (handle == NULL) {
        return -ENOMEM;
    }

    sem->handle = handle;
    sem->limit = limit;
    return 0;
}

int os_sem_take(struct os_sem *sem, uint32_t timeout)
{
    BaseType_t result;

    if (!sem || !sem->handle) {
        return -EINVAL;
    }

    if (os_is_in_isr()) {
        BaseType_t hpw = pdFALSE;

        if (timeout != 0U) {
            return -EWOULDBLOCK;
        }

        result = xSemaphoreTakeFromISR((SemaphoreHandle_t)sem->handle, &hpw);
        if (hpw != pdFALSE) {
            portYIELD_FROM_ISR(hpw);
        }
    } else {
        TickType_t ticks = (timeout == OS_WAIT_FOREVER) ?
                           portMAX_DELAY : (TickType_t)timeout;
        result = xSemaphoreTake((SemaphoreHandle_t)sem->handle, ticks);
    }

    if (result == pdTRUE) {
        return 0;
    }
    return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
}

int os_sem_trytake(struct os_sem *sem)
{
    return os_sem_take(sem, 0U);
}

void os_sem_give(struct os_sem *sem)
{
    if (!sem || !sem->handle) {
        return;
    }

    if (os_is_in_isr()) {
        BaseType_t hpw = pdFALSE;

        (void)xSemaphoreGiveFromISR((SemaphoreHandle_t)sem->handle, &hpw);
        if (hpw != pdFALSE) {
            portYIELD_FROM_ISR(hpw);
        }
    } else {
        /*
         * FreeRTOS reports errQUEUE_FULL at the limit. Ignoring it implements
         * the OSAL's saturating give semantics.
         */
        (void)xSemaphoreGive((SemaphoreHandle_t)sem->handle);
    }
}

uint32_t os_sem_count_get(struct os_sem *sem)
{
    if (!sem || !sem->handle) {
        return 0U;
    }

    if (os_is_in_isr()) {
        return (uint32_t)uxSemaphoreGetCountFromISR(
            (SemaphoreHandle_t)sem->handle);
    }

    return (uint32_t)uxSemaphoreGetCount((SemaphoreHandle_t)sem->handle);
}
