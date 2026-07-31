#include "internal.h"

int os_kernel_init(void)
{
    return os_is_in_isr() ? -EWOULDBLOCK : 0;
}
