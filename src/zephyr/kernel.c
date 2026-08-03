#include <hazel_wireless/kernel.h>

#include <zephyr/kernel.h>

int os_kernel_init(void)
{
    return k_is_in_isr() ? -EWOULDBLOCK : 0;
}

int os_kernel_start(void)
{
    return k_is_in_isr() ? -EWOULDBLOCK : -EALREADY;
}

int os_kernel_stop(void)
{
    return k_is_in_isr() ? -EWOULDBLOCK : -ENOTSUP;
}
