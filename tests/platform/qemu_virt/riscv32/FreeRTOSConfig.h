#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define configMTIME_BASE_ADDRESS                 0x0200bff8UL
#define configMTIMECMP_BASE_ADDRESS              0x02004000UL
#define configISR_STACK_SIZE_WORDS               256

#define configUSE_PREEMPTION                     1
#define configCPU_CLOCK_HZ                       10000000UL
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     8
#define configMINIMAL_STACK_SIZE                 128
#define configTOTAL_HEAP_SIZE                    (48U * 1024U)
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_MUTEXES                        0
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_QUEUE_SETS                     0
#define configUSE_TIMERS                         0
#define configUSE_TASK_NOTIFICATIONS             1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS  1
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configCHECK_FOR_STACK_OVERFLOW            0
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_TRACE_FACILITY                 0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1

#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_xTaskGetSchedulerState            1

void osal_test_assert(const char *file, uint32_t line);
#define configASSERT(condition) \
    do { if (!(condition)) { osal_test_assert(__FILE__, __LINE__); } } while (0)

#endif
