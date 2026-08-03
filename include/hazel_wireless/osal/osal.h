/**
 * @file osal.h
 * @brief Select backend-specific storage mappings for the configured RTOS.
 */
#ifndef HAZEL_INCLUDE_OSAL_H_
#define HAZEL_INCLUDE_OSAL_H_

#if defined(CONFIG_FREERTOS_ENABLE)
#include <hazel_wireless/osal/osal_freertos.h>
#elif defined(CONFIG_RTTHREAD_ENABLE)
#include <hazel_wireless/osal/osal_rtthread.h>
#elif defined(CONFIG_ZEPHYR_ENABLE)
#include <hazel_wireless/osal/osal_zephyr.h>
#elif defined(CONFIG_HAZEL_OSAL_OTHER)
#include <hazel_wireless/osal/osal_other.h>
#else
#error "Unsupported OSAL configuration"
#endif

#endif
