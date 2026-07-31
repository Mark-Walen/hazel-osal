#ifndef LYNX_OSAL_RTTHREAD_INTERNAL_H
#define LYNX_OSAL_RTTHREAD_INTERNAL_H

#include <lynx_wireless/kernel.h>

#include <rtthread.h>

static inline rt_uint8_t os_to_rt_priority(int priority)
{
    if (priority < 0) {
        priority = 0;
    }
    if (priority >= RT_THREAD_PRIORITY_MAX) {
        priority = RT_THREAD_PRIORITY_MAX - 1;
    }
    return (rt_uint8_t)(RT_THREAD_PRIORITY_MAX - 1 - priority);
}

static inline int rt_to_os_priority(rt_uint8_t priority)
{
    return (int)(RT_THREAD_PRIORITY_MAX - 1U - priority);
}

static inline rt_int32_t os_to_rt_timeout(uint32_t timeout)
{
    return (timeout == OS_WAIT_FOREVER) ?
           RT_WAITING_FOREVER : (rt_int32_t)timeout;
}

void os_rt_waitq_insert_locked(os_wait_q_t *q, struct os_thread *thread);

#endif
