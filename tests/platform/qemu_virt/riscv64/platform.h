#ifndef OSAL_TEST_QEMU_RISCV64_PLATFORM_H
#define OSAL_TEST_QEMU_RISCV64_PLATFORM_H

void platform_puts(const char *text);
void platform_exit(int success) __attribute__((noreturn));

#endif
