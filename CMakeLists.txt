cmake_minimum_required(VERSION 3.20)

project(lynx_osal LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(OSAL_RTOS "freertos" CACHE STRING "RTOS backend")
set_property(CACHE OSAL_RTOS PROPERTY STRINGS freertos)
set(OSAL_ARCH "riscv32" CACHE STRING "CPU architecture")
set_property(CACHE OSAL_ARCH PROPERTY STRINGS riscv32 arm)
set(OSAL_PLATFORM "qemu_virt" CACHE STRING "Board/simulator platform")
set_property(CACHE OSAL_PLATFORM PROPERTY STRINGS qemu_virt)
option(OSAL_BUILD_TESTS "Build target-side OSAL tests" ON)

if(NOT OSAL_RTOS STREQUAL "freertos")
    message(FATAL_ERROR "Unsupported OSAL_RTOS '${OSAL_RTOS}'")
endif()
if(NOT OSAL_ARCH STREQUAL "riscv32")
    message(FATAL_ERROR
        "OSAL_ARCH '${OSAL_ARCH}' is not implemented yet. "
        "Add its FreeRTOS portable layer in src/third-party/CMakeLists.txt.")
endif()
if(NOT OSAL_PLATFORM STREQUAL "qemu_virt")
    message(FATAL_ERROR "Unsupported OSAL_PLATFORM '${OSAL_PLATFORM}'")
endif()

add_subdirectory(src/third-party)
add_subdirectory(src)

if(OSAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
