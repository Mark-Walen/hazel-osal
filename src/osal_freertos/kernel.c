#include "internal.h"

static volatile bool os_scheduler_started_by_osal;
static volatile bool os_scheduler_stop_requested;

static bool os_kernel_scheduler_running(void)
{
#if defined(INCLUDE_xTaskGetSchedulerState) && \
    (INCLUDE_xTaskGetSchedulerState == 1)
    return xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
#else
    return os_scheduler_started_by_osal;
#endif
}

int os_kernel_init(void)
{
    return os_is_in_isr() ? -EWOULDBLOCK : 0;
}

int os_kernel_start(void)
{
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    if (os_kernel_scheduler_running()) {
        return -EALREADY;
    }

    os_scheduler_stop_requested = false;
    os_scheduler_started_by_osal = true;
    vTaskStartScheduler();
    os_scheduler_started_by_osal = false;

    if (os_scheduler_stop_requested) {
        os_scheduler_stop_requested = false;
        return 0;
    }
    return -ENOMEM;
}

int os_kernel_stop(void)
{
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    if (!os_kernel_scheduler_running()) {
        return -EALREADY;
    }

#if defined(CONFIG_OSAL_FREERTOS_SCHEDULER_STOP) && \
    (CONFIG_OSAL_FREERTOS_SCHEDULER_STOP == 1)
    os_scheduler_stop_requested = true;
    vTaskEndScheduler();
    return 0;
#else
    return -ENOTSUP;
#endif
}
