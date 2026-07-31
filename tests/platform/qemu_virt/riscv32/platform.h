#ifndef OSAL_TEST_QEMU_RISCV32_PLATFORM_H
#define OSAL_TEST_QEMU_RISCV32_PLATFORM_H

#define QEMU_UART0_BASE  0x10000000
#define QEMU_TEST_BASE   0x00100000

#define CLINT_BASE       0x02000000
#define CLINT_MTIMECMP   0x00004000
#define CLINT_MTIME      0x0000bff8

#ifdef __ASSEMBLER__
#define REG_LOAD lw
#define REG_STORE sw
#define REG_SIZE 4
#endif

#ifndef __ASSEMBLER__
void platform_puts(const char *text);
void platform_exit(int success) __attribute__((noreturn));
#endif

#endif
