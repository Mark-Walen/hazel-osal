#include "internal.h"

#if !defined(__ARM_ARCH)
extern BaseType_t xPortIsInsideInterrupt(void);
#endif

bool os_is_in_isr(void)
{
    return xPortIsInsideInterrupt() != pdFALSE;
}

os_critical_key_t os_enter_critical(void)
{
    os_critical_key_t key = {
        .value = 0,
        .from_isr = false,
    };

    if (os_is_in_isr()) {
        key.from_isr = true;
        key.value = (uintptr_t)portSET_INTERRUPT_MASK_FROM_ISR();
    } else {
        taskENTER_CRITICAL();
    }
    return key;
}

void os_exit_critical(os_critical_key_t key)
{
    if (key.from_isr) {
        portCLEAR_INTERRUPT_MASK_FROM_ISR((UBaseType_t)key.value);
    } else {
        taskEXIT_CRITICAL();
    }
}
