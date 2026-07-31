#ifndef LYNX_SUBSYS_OS_OSAL_ZEPHYR_H_
#define LYNX_SUBSYS_OS_OSAL_ZEPHYR_H_

#include <zephyr/kernel.h>

#undef os_sem_storage_t
#define os_sem_storage_t struct k_sem

#endif
