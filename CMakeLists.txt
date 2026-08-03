cmake_minimum_required(VERSION 3.20)

project(hazel_osal LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(OSAL_RTOS "freertos" CACHE STRING "RTOS backend")
set_property(CACHE OSAL_RTOS PROPERTY STRINGS freertos rtthread zephyr)
set(OSAL_ARCH "riscv32" CACHE STRING "CPU architecture")
set_property(CACHE OSAL_ARCH PROPERTY STRINGS riscv32 arm32)
set(OSAL_PLATFORM "qemu_virt" CACHE STRING "Board/simulator platform")
set_property(CACHE OSAL_PLATFORM PROPERTY STRINGS qemu_virt)
option(OSAL_BUILD_TESTS "Build target-side OSAL tests" ON)

if(OSAL_RTOS STREQUAL "freertos" AND
   NOT OSAL_ARCH STREQUAL "riscv32" AND
   NOT OSAL_ARCH STREQUAL "arm32")
    message(FATAL_ERROR
        "OSAL_ARCH '${OSAL_ARCH}' is not implemented yet. "
        "Add its FreeRTOS portable layer in src/third-party/CMakeLists.txt.")
endif()
if(OSAL_RTOS STREQUAL "freertos" AND NOT OSAL_PLATFORM STREQUAL "qemu_virt")
    message(FATAL_ERROR "Unsupported OSAL_PLATFORM '${OSAL_PLATFORM}'")
endif()

add_subdirectory(src/third-party)
add_subdirectory(src)

if(OSAL_BUILD_TESTS AND OSAL_RTOS STREQUAL "freertos")
    enable_testing()
    add_subdirectory(tests)
endif()
