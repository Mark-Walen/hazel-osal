if(OSAL_RTOS STREQUAL "freertos")
    set(OSAL_SOURCES
        osal_freertos/kernel.c
        osal_freertos/context.c
        osal_freertos/thread.c
        osal_freertos/waitq.c
        osal_freertos/mutex.c
        osal_freertos/semaphore.c
        osal_freertos/memory.c
        osal_freertos/time.c
    )
    set(OSAL_CONFIG_DEFINITION CONFIG_FREERTOS_ENABLE=1)
elseif(OSAL_RTOS STREQUAL "rtthread")
    set(OSAL_SOURCES
        osal_rtthread/kernel.c
        osal_rtthread/context.c
        osal_rtthread/name.c
        osal_rtthread/thread.c
        osal_rtthread/waitq.c
        osal_rtthread/mutex.c
        osal_rtthread/semaphore.c
        osal_rtthread/memory.c
        osal_rtthread/time.c
    )
    set(OSAL_CONFIG_DEFINITION CONFIG_RTTHREAD_ENABLE=1)
elseif(OSAL_RTOS STREQUAL "zephyr")
    set(OSAL_SOURCES
        zephyr/kernel.c
        zephyr/semaphore.c
        zephyr/time.c
    )
    set(OSAL_CONFIG_DEFINITION CONFIG_ZEPHYR_ENABLE=1)
else()
    message(FATAL_ERROR "Unsupported OSAL_RTOS '${OSAL_RTOS}'")
endif()

add_library(lynx_osal STATIC ${OSAL_SOURCES})
target_include_directories(lynx_osal PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(lynx_osal PUBLIC ${OSAL_CONFIG_DEFINITION})
target_link_libraries(lynx_osal PUBLIC rtos_core)
target_compile_options(lynx_osal PRIVATE
    -Wall -Wextra -Werror
    -ffunction-sections -fdata-sections
)
