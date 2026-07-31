#include "platform.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stddef.h>
#include <stdint.h>

#define UART_THR 0
#define UART_LSR 5
#define UART_LSR_TX_IDLE 0x20

static volatile uint8_t *const uart = (volatile uint8_t *)QEMU_UART0_BASE;

static void platform_putc(char c)
{
    while ((uart[UART_LSR] & UART_LSR_TX_IDLE) == 0U) {
    }
    uart[UART_THR] = (uint8_t)c;
}

void platform_puts(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            platform_putc('\r');
        }
        platform_putc(*text++);
    }
}

void platform_exit(int success)
{
    volatile uint32_t *const finisher = (volatile uint32_t *)QEMU_TEST_BASE;
    *finisher = success ? 0x5555U : 0x3333U;
    for (;;) {
        __asm__ volatile("wfi");
    }
}

/*
 * The FreeRTOS RISC-V port switches to its dedicated ISR stack before calling
 * C code. This makes ISR detection reliable even when task interrupts are
 * temporarily masked by a critical section.
 */
BaseType_t xPortIsInsideInterrupt(void)
{
    extern const StackType_t xISRStackTop;
    uintptr_t sp;
    uintptr_t top = (uintptr_t)xISRStackTop;
    uintptr_t bottom = top - (configISR_STACK_SIZE_WORDS * sizeof(StackType_t));

    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return ((sp >= bottom) && (sp <= top)) ? pdTRUE : pdFALSE;
}

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   StackType_t *stack_depth)
{
    static StaticTask_t idle_tcb;
    static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

    *tcb = &idle_tcb;
    *stack = idle_stack;
    *stack_depth = configMINIMAL_STACK_SIZE;
}

void *memset(void *destination, int value, size_t length)
{
    unsigned char *out = (unsigned char *)destination;
    while (length-- != 0U) {
        *out++ = (unsigned char)value;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    while (length-- != 0U) {
        *out++ = *in++;
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t length)
{
    const unsigned char *lhs = (const unsigned char *)left;
    const unsigned char *rhs = (const unsigned char *)right;
    while (length-- != 0U) {
        if (*lhs != *rhs) {
            return (int)*lhs - (int)*rhs;
        }
        ++lhs;
        ++rhs;
    }
    return 0;
}

size_t strlen(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void abort(void)
{
    platform_puts("FAIL: abort\n");
    platform_exit(0);
}
