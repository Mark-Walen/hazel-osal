set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(RISCV_GCC riscv64-unknown-elf-gcc REQUIRED)
find_program(RISCV_AR riscv64-unknown-elf-ar REQUIRED)
find_program(RISCV_RANLIB riscv64-unknown-elf-ranlib REQUIRED)

set(CMAKE_C_COMPILER "${RISCV_GCC}")
set(CMAKE_ASM_COMPILER "${RISCV_GCC}")
set(CMAKE_AR "${RISCV_AR}")
set(CMAKE_RANLIB "${RISCV_RANLIB}")

set(RISCV_ARCH_FLAGS
    "-march=rv32imac_zicsr -mabi=ilp32 -mcmodel=medany"
    CACHE STRING "RISC-V ISA, ABI and code model flags")
set(CMAKE_C_FLAGS_INIT "${RISCV_ARCH_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${RISCV_ARCH_FLAGS}")
