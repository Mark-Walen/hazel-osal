#include "internal.h"

bool os_is_in_isr(void)
{
    return rt_interrupt_get_nest() != 0U;
}

os_critical_key_t os_enter_critical(void)
{
    os_critical_key_t key;

    key.from_isr = os_is_in_isr();
    key.value = (uintptr_t)rt_hw_interrupt_disable();
    return key;
}

void os_exit_critical(os_critical_key_t key)
{
    rt_hw_interrupt_enable((rt_base_t)key.value);
}
