#ifndef HAZEL_OSAL_FREERTOS_INTERNAL_H
#define HAZEL_OSAL_FREERTOS_INTERNAL_H

#include <hazel_wireless/kernel.h>

#include "FreeRTOS.h"
#include "task.h"

BaseType_t os_waitq_notify_thread_locked(struct os_thread *thread,
                                         uint32_t reason);
void os_waitq_insert_priority_locked(os_wait_q_t *q,
                                     struct os_thread *thread);
void os_mutex_update_pi_chain_locked(struct os_thread *thread);

#endif
