/**
 * @file signal.c
 * @brief Single-pending integer-result signal implementation.
 */
#include <hazel_wireless/kernel.h>

#include "internal.h"

int os_signal_init(struct os_signal *signal)
{
    if (!signal) {
        return -EINVAL;
    }
    os_common_zero(signal, sizeof(*signal));
    if (os_sem_init(&signal->notification, 0, 1) != 0) {
        return -ENOMEM;
    }
    signal->initialized = true;
    return 0;
}

int os_signal_deinit(struct os_signal *signal)
{
    int result;

    if (!signal || !signal->initialized) {
        return -EINVAL;
    }
    result = os_sem_deinit(&signal->notification);
    if (result == 0) {
        os_common_zero(signal, sizeof(*signal));
    }
    return result;
}

int os_signal_raise(struct os_signal *signal, int result)
{
    os_critical_key_t key;

    if (!signal || !signal->initialized) {
        return -EINVAL;
    }
    key = os_enter_critical();
    if (signal->raised) {
        os_exit_critical(key);
        return -EBUSY;
    }
    signal->result = result;
    signal->raised = true;
    os_sem_give(&signal->notification);
    os_exit_critical(key);
    return 0;
}

int os_signal_wait(struct os_signal *signal, uint32_t timeout, int *result)
{
    os_critical_key_t key;
    int status;

    if (!signal || !signal->initialized || !result) {
        return -EINVAL;
    }
    status = os_sem_take(&signal->notification, timeout);
    if (status != 0) {
        return status;
    }
    key = os_enter_critical();
    *result = signal->result;
    signal->raised = false;
    os_exit_critical(key);
    return 0;
}

int os_signal_reset(struct os_signal *signal)
{
    os_critical_key_t key;

    if (!signal || !signal->initialized) {
        return -EINVAL;
    }
    key = os_enter_critical();
    while (os_sem_trytake(&signal->notification) == 0) {
    }
    signal->raised = false;
    signal->result = 0;
    os_exit_critical(key);
    return 0;
}
