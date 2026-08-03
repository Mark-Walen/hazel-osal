#include "platform.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stddef.h>
#include <stdint.h>

static int semihost_call(int operation, const void *argument)
{
    register int r0 __asm__("r0") = operation;
    register const void *r1 __asm__("r1") = argument;

    __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

void platform_puts(const char *text)
{
    (void)semihost_call(4, text);
}

void platform_exit(int success)
{
    uintptr_t reason = success ? UINT32_C(0x20026) : UINT32_C(0x20023);

    (void)semihost_call(0x18, (const void *)reason);
    for (;;) {
        __asm__ volatile("wfi");
    }
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
