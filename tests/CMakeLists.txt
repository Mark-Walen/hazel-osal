set(PLATFORM_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/qemu_virt/${OSAL_ARCH}")

add_executable(osal_qemu_tests
    osal_test.c
    "${PLATFORM_DIR}/platform.c"
    "${PLATFORM_DIR}/startup.S"
)
set_target_properties(osal_qemu_tests PROPERTIES SUFFIX ".elf")

target_include_directories(osal_qemu_tests PRIVATE "${PLATFORM_DIR}")
target_link_libraries(osal_qemu_tests PRIVATE hazel_osal rtos_core gcc)
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

if(OSAL_ARCH STREQUAL "riscv32")
    find_program(QEMU_SYSTEM qemu-system-riscv32)
    set(QEMU_ARGS
        -machine virt -smp 1 -nographic -bios none
        -kernel $<TARGET_FILE:osal_qemu_tests>)
elseif(OSAL_ARCH STREQUAL "arm32")
    find_program(QEMU_SYSTEM qemu-system-arm)
    set(QEMU_ARGS
        -machine lm3s6965evb -nographic
        -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:osal_qemu_tests>)
endif()

if(QEMU_SYSTEM)
    add_test(
        NAME osal.qemu.${OSAL_ARCH}.freertos
        COMMAND "${QEMU_SYSTEM}" ${QEMU_ARGS}
    )
    set_tests_properties(osal.qemu.${OSAL_ARCH}.freertos PROPERTIES
        TIMEOUT 15
        PASS_REGULAR_EXPRESSION "PASS: OSAL tests"
        FAIL_REGULAR_EXPRESSION "FAIL:"
    )
else()
    message(WARNING "QEMU for ${OSAL_ARCH} not found; CTest is disabled")
endif()
