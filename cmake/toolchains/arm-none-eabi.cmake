set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(ARM_GCC arm-none-eabi-gcc REQUIRED)
find_program(ARM_AR arm-none-eabi-ar REQUIRED)
find_program(ARM_RANLIB arm-none-eabi-ranlib REQUIRED)

set(CMAKE_C_COMPILER "${ARM_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_GCC}")
set(CMAKE_AR "${ARM_AR}")
set(CMAKE_RANLIB "${ARM_RANLIB}")

set(ARM_ARCH_FLAGS "-mcpu=cortex-m3 -mthumb"
    CACHE STRING "ARM CPU and instruction-set flags")
set(CMAKE_C_FLAGS_INIT "${ARM_ARCH_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${ARM_ARCH_FLAGS}")
