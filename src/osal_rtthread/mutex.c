#include "internal.h"

void os_mutex_init(struct os_mutex *mutex)
{
    char name[RT_NAME_MAX];

    if (!mutex) {
        return;
    }

    rt_memset(mutex, 0, sizeof(*mutex));
    os_rt_name_generate(name, "mutex", mutex, RT_NULL);
    if (rt_mutex_init(&mutex->storage, name, RT_IPC_FLAG_PRIO) == RT_EOK) {
        mutex->native_handle = &mutex->storage;
        mutex->initialized = true;
    }
}

int os_mutex_deinit(struct os_mutex *mutex)
{
    rt_base_t level;
    bool busy;

    if (!mutex || !mutex->initialized || !mutex->native_handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    level = rt_hw_interrupt_disable();
    busy = (mutex->storage.owner != RT_NULL) ||
           !rt_list_isempty(&mutex->storage.parent.suspend_thread);
    rt_hw_interrupt_enable(level);
    if (busy) {
        return -EBUSY;
    }
    if (rt_mutex_detach(&mutex->storage) != RT_EOK) {
        return -EBUSY;
    }
    rt_memset(mutex, 0, sizeof(*mutex));
    return 0;
}

int os_mutex_lock(struct os_mutex *mutex, uint32_t timeout)
{
    rt_err_t result;

    if (!mutex || !mutex->initialized || !mutex->native_handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    result = rt_mutex_take((rt_mutex_t)mutex->native_handle,
                           os_to_rt_timeout(timeout));
    if (result == RT_EOK) {
        return 0;
    }
    return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
}

int os_mutex_trylock(struct os_mutex *mutex)
{
    if (!mutex || !mutex->initialized || !mutex->native_handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    return (rt_mutex_trytake((rt_mutex_t)mutex->native_handle) == RT_EOK) ?
           0 : -EBUSY;
}

int os_mutex_unlock(struct os_mutex *mutex)
{
    if (!mutex || !mutex->initialized || !mutex->native_handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    return (rt_mutex_release((rt_mutex_t)mutex->native_handle) == RT_EOK) ?
           0 : -EPERM;
}
