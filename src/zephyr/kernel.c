#include <hazel_wireless/kernel.h>

#include <zephyr/kernel.h>

int os_kernel_init(void)
{
    return k_is_in_isr() ? -EWOULDBLOCK : 0;
}
