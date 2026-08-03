#include "internal.h"

int os_kernel_init(void)
{
    return os_is_in_isr() ? -EWOULDBLOCK : 0;
}

int os_kernel_start(void)
{
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    if (rt_thread_self() != RT_NULL) {
        return -EALREADY;
    }

    rt_system_scheduler_start();
    return -EIO;
}

int os_kernel_stop(void)
{
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    return rt_thread_self() == RT_NULL ? -EALREADY : -ENOTSUP;
}
