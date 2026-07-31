#include <lynx_wireless/kernel.h>

#include <rtthread.h>
#include <string.h>

int os_sem_init(struct os_sem *sem, uint32_t initial_count, uint32_t limit)
{
    rt_err_t result;

    if (!sem || (limit == 0U) || (initial_count > limit) ||
        (limit > RT_SEM_VALUE_MAX)) {
        return -EINVAL;
    }

    memset(sem, 0, sizeof(*sem));
    result = rt_sem_init(&sem->storage, "osal", (rt_uint32_t)initial_count,
                         RT_IPC_FLAG_PRIO);
    if (result != RT_EOK) {
        return -ENOMEM;
    }

    sem->handle = &sem->storage;
    sem->limit = limit;
    return 0;
}

int os_sem_take(struct os_sem *sem, uint32_t timeout)
{
    rt_int32_t ticks;
    rt_err_t result;

    if (!sem || !sem->handle) {
        return -EINVAL;
    }
    if ((rt_interrupt_get_nest() != 0U) && (timeout != 0U)) {
        return -EWOULDBLOCK;
    }

    ticks = (timeout == OS_WAIT_FOREVER) ? RT_WAITING_FOREVER :
            (rt_int32_t)timeout;
    result = (timeout == 0U) ?
             rt_sem_trytake((rt_sem_t)sem->handle) :
             rt_sem_take((rt_sem_t)sem->handle, ticks);
    if (result == RT_EOK) {
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
    rt_base_t level;

    if (!sem || !sem->handle) {
        return;
    }

    level = rt_hw_interrupt_disable();
    if (sem->storage.value < sem->limit) {
        (void)rt_sem_release((rt_sem_t)sem->handle);
    }
    rt_hw_interrupt_enable(level);
}

uint32_t os_sem_count_get(struct os_sem *sem)
{
    rt_base_t level;
    uint32_t count;

    if (!sem || !sem->handle) {
        return 0U;
    }

    level = rt_hw_interrupt_disable();
    count = (uint32_t)sem->storage.value;
    rt_hw_interrupt_enable(level);
    return count;
}
