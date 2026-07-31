set(FREERTOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/FreeRTOS")
set(FREERTOS_KERNEL "${FREERTOS_ROOT}/Source")
set(FREERTOS_PORT "${FREERTOS_KERNEL}/portable/GCC/RISC-V")

add_library(freertos_kernel STATIC
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

target_include_directories(freertos_kernel
    PUBLIC
        "${FREERTOS_KERNEL}/include"
        "${FREERTOS_PORT}"
    PRIVATE
        "${FREERTOS_PORT}/chip_specific_extensions/RV32I_CLINT_no_extensions"
)

# FreeRTOSConfig.h is supplied by the selected platform, not by the RTOS.
target_include_directories(freertos_kernel PUBLIC
    "${PROJECT_SOURCE_DIR}/tests/platform/qemu_virt/riscv32"
)

target_compile_options(freertos_kernel PRIVATE
    -ffunction-sections -fdata-sections
)
