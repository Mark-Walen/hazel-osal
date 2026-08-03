/**
 * @file osal_freertos.h
 * @brief FreeRTOS specific definitions for the OSAL layer.
 *
 * Maps generic OSAL types to FreeRTOS specific types.
 */
#ifndef HAZEL_SUBSYS_OS_OSAL_FREERTOS_H_
#define HAZEL_SUBSYS_OS_OSAL_FREERTOS_H_

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

/** @brief Override generic TCB type with FreeRTOS StaticTask_t. */
#undef os_thread_tcb_t
#define os_thread_tcb_t StaticTask_t

/** @brief Override generic stack type with FreeRTOS StackType_t. */
#undef os_thread_stack_t
#define os_thread_stack_t StackType_t

/** @brief Embedded storage used by a statically allocated counting semaphore. */
#undef os_sem_storage_t
#define os_sem_storage_t StaticSemaphore_t

/** @brief Embedded completion event used by os_thread_join(). */
#undef os_thread_completion_storage_t
#define os_thread_completion_storage_t StaticSemaphore_t

#undef os_event_storage_t
/** @brief Embedded storage used by a statically allocated event group. */
#define os_event_storage_t StaticEventGroup_t

/** @brief Portable event mask when FreeRTOS uses 16-bit ticks. */
#if (configUSE_16_BIT_TICKS == 1)
#define OS_EVENT_BITS_MASK UINT32_C(0x000000ff)
#else
#define OS_EVENT_BITS_MASK UINT32_C(0x00ffffff)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* HAZEL_SUBSYS_OS_OSAL_FREERTOS_H_ */
