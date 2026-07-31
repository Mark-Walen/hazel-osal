#ifndef LYNX_SUBSYS_OS_OSAL_RTTHREAD_H_
#define LYNX_SUBSYS_OS_OSAL_RTTHREAD_H_

#include <rtthread.h>

#undef os_thread_tcb_t
#define os_thread_tcb_t struct rt_thread

#undef os_thread_stack_t
#define os_thread_stack_t rt_uint8_t

#undef os_thread_wait_storage_t
#define os_thread_wait_storage_t struct rt_semaphore

#undef os_mutex_storage_t
#define os_mutex_storage_t struct rt_mutex

#undef os_sem_storage_t
#define os_sem_storage_t struct rt_semaphore

#endif
