/**
 * @file osal_rtthread.h
 * @brief Map portable OSAL storage types to RT-Thread kernel objects.
 */
#ifndef LYNX_SUBSYS_OS_OSAL_RTTHREAD_H_
#define LYNX_SUBSYS_OS_OSAL_RTTHREAD_H_

#include <rtthread.h>

/** @brief Embedded RT-Thread thread control block. */
#undef os_thread_tcb_t
#define os_thread_tcb_t struct rt_thread

/** @brief RT-Thread byte-addressed stack element type. */
#undef os_thread_stack_t
#define os_thread_stack_t rt_uint8_t

/** @brief Per-thread wait notification semaphore storage. */
#undef os_thread_wait_storage_t
#define os_thread_wait_storage_t struct rt_semaphore

/** @brief Thread completion semaphore storage. */
#undef os_thread_completion_storage_t
#define os_thread_completion_storage_t struct rt_semaphore

/** @brief Native RT-Thread priority-inheritance mutex storage. */
#undef os_mutex_storage_t
#define os_mutex_storage_t struct rt_mutex

/** @brief Native RT-Thread counting semaphore storage. */
#undef os_sem_storage_t
#define os_sem_storage_t struct rt_semaphore

/** @brief Native RT-Thread event storage. */
#undef os_event_storage_t
#define os_event_storage_t struct rt_event

#endif
