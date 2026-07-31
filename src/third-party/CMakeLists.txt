if(OSAL_RTOS STREQUAL "freertos")
    set(FREERTOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/FreeRTOS")
    set(FREERTOS_KERNEL "${FREERTOS_ROOT}/Source")
    set(FREERTOS_PORT "${FREERTOS_KERNEL}/portable/GCC/RISC-V")

    add_library(rtos_core STATIC
        "${FREERTOS_KERNEL}/tasks.c"
        "${FREERTOS_KERNEL}/list.c"
        "${FREERTOS_KERNEL}/queue.c"
        "${FREERTOS_KERNEL}/timers.c"
        "${FREERTOS_KERNEL}/event_groups.c"
        "${FREERTOS_KERNEL}/stream_buffer.c"
        "${FREERTOS_KERNEL}/portable/MemMang/heap_4.c"
        "${FREERTOS_PORT}/port.c"
        "${FREERTOS_PORT}/portASM.S"
    )
    target_include_directories(rtos_core
        PUBLIC
            "${FREERTOS_KERNEL}/include"
            "${FREERTOS_PORT}"
            "${PROJECT_SOURCE_DIR}/tests/platform/qemu_virt/riscv32"
        PRIVATE
            "${FREERTOS_PORT}/chip_specific_extensions/RV32I_CLINT_no_extensions"
    )
    target_compile_options(rtos_core PRIVATE
        -ffunction-sections -fdata-sections)

elseif(OSAL_RTOS STREQUAL "rtthread")
    set(RTTHREAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/rt-thread")
    set(RTTHREAD_CONFIG_DIR "" CACHE PATH
        "Directory containing the target RT-Thread rtconfig.h")
    set(RTTHREAD_CPU_INCLUDE_DIRS "" CACHE STRING
        "Target RT-Thread CPU-port include directories")
    if(NOT EXISTS "${RTTHREAD_ROOT}/include/rtthread.h")
        message(FATAL_ERROR
            "RT-Thread submodule missing; run git submodule update --init")
    endif()
    if(NOT EXISTS "${RTTHREAD_CONFIG_DIR}/rtconfig.h")
        message(FATAL_ERROR
            "Set RTTHREAD_CONFIG_DIR to the target directory containing rtconfig.h")
    endif()
    if(NOT RTTHREAD_CPU_INCLUDE_DIRS)
        message(FATAL_ERROR
            "Set RTTHREAD_CPU_INCLUDE_DIRS to the target CPU-port include directories")
    endif()

    # The BSP owns the RT-Thread kernel/CPU-port build. This interface target
    # supplies headers while lynx_osal compiles as a reusable backend library.
    add_library(rtos_core INTERFACE)
    target_include_directories(rtos_core INTERFACE
        "${RTTHREAD_CONFIG_DIR}"
        ${RTTHREAD_CPU_INCLUDE_DIRS}
    )
    target_include_directories(rtos_core SYSTEM INTERFACE
        "${RTTHREAD_ROOT}/include"
        "${RTTHREAD_ROOT}/components/drivers/include"
        "${RTTHREAD_ROOT}/components/finsh"
    )

elseif(OSAL_RTOS STREQUAL "zephyr")
    add_library(rtos_core INTERFACE)
else()
    message(FATAL_ERROR "Unsupported OSAL_RTOS '${OSAL_RTOS}'")
endif()
