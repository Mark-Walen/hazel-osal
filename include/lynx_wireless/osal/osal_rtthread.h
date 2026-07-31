#ifndef LYNX_SUBSYS_OS_OSAL_RTTHREAD_H_
#define LYNX_SUBSYS_OS_OSAL_RTTHREAD_H_

#include <rtthread.h>

#undef os_sem_storage_t
#define os_sem_storage_t struct rt_semaphore

#endif
