#ifndef OSAL_TEST_QEMU_ARM32_PLATFORM_H
#define OSAL_TEST_QEMU_ARM32_PLATFORM_H

#ifndef __ASSEMBLER__
void platform_puts(const char *text);
void platform_exit(int success) __attribute__((noreturn));
#endif

#endif
