/**
 * @file osal_zephyr.h
 * @brief Map portable OSAL storage types to Zephyr kernel objects.
 */
#ifndef LYNX_SUBSYS_OS_OSAL_ZEPHYR_H_
#define LYNX_SUBSYS_OS_OSAL_ZEPHYR_H_

#include <zephyr/kernel.h>

/** @brief Native Zephyr semaphore storage. */
#undef os_sem_storage_t
#define os_sem_storage_t struct k_sem

/** @brief Native Zephyr thread completion semaphore storage. */
#undef os_thread_completion_storage_t
#define os_thread_completion_storage_t struct k_sem

/** @brief Native Zephyr event storage. */
#undef os_event_storage_t
#define os_event_storage_t struct k_event

#endif
