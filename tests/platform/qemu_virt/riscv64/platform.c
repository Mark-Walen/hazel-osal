#include "platform.h"

#include <rtthread.h>
#include <rthw.h>
#include <plic.h>
#include <sbi.h>
#include <tick.h>

extern int entry(void);
extern rt_uint8_t __bss_end;

rt_uint64_t rt_hw_get_clock_timer_freq(void)
{
    return 10000000ULL;
}

void primary_cpu_entry(void)
{
    rt_hw_interrupt_disable();
    (void)entry();
}

void rt_hw_board_init(void)
{
    rt_system_heap_init(&__bss_end, (void *)0x81200000);
    plic_init();
    rt_hw_interrupt_init();
    rt_hw_tick_init();
}

void platform_puts(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            sbi_console_putchar('\r');
        }
        sbi_console_putchar(*text++);
    }
}

void platform_exit(int success)
{
    volatile rt_uint32_t *const finisher =
        (volatile rt_uint32_t *)0x00100000;

    *finisher = success ? 0x5555U : 0x3333U;
    sbi_shutdown();
    for (;;) {
        __asm__ volatile("wfi");
    }
}

void rt_hw_cpu_reset(void)
{
    sbi_shutdown();
    for (;;) {
        __asm__ volatile("wfi");
    }
}
