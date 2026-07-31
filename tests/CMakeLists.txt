set(PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/qemu_virt/riscv32")

add_executable(osal_qemu_tests
    osal_test.c
    "${PLATFORM_DIR}/platform.c"
    "${PLATFORM_DIR}/startup.S"
)
set_target_properties(osal_qemu_tests PROPERTIES SUFFIX ".elf")

target_include_directories(osal_qemu_tests PRIVATE "${PLATFORM_DIR}")
target_link_libraries(osal_qemu_tests PRIVATE lynx_osal rtos_core gcc)
target_compile_options(osal_qemu_tests PRIVATE
    -Wall -Wextra -Werror
    -ffreestanding -ffunction-sections -fdata-sections
)
target_link_options(osal_qemu_tests PRIVATE
    -nostdlib
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/osal_qemu_tests.map
    -T${PLATFORM_DIR}/linker.ld
)
set_property(TARGET osal_qemu_tests APPEND PROPERTY
    LINK_DEPENDS "${PLATFORM_DIR}/linker.ld")

find_program(QEMU_RISCV32 qemu-system-riscv32)
if(QEMU_RISCV32)
    add_test(
        NAME osal.qemu.riscv32.freertos
        COMMAND "${QEMU_RISCV32}"
            -machine virt
            -smp 1
            -nographic
            -bios none
            -kernel $<TARGET_FILE:osal_qemu_tests>
    )
    set_tests_properties(osal.qemu.riscv32.freertos PROPERTIES
        TIMEOUT 15
        PASS_REGULAR_EXPRESSION "PASS: OSAL tests"
        FAIL_REGULAR_EXPRESSION "FAIL:"
    )
else()
    message(WARNING "qemu-system-riscv32 not found; firmware will build but CTest is disabled")
endif()
