#include "platform.h"

.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.global __isr_vector
__isr_vector:
    .word __stack_top
    .word Reset_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word vPortSVCHandler
    .word Default_Handler
    .word 0
    .word xPortPendSVHandler
    .word xPortSysTickHandler

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =__data_load
    ldr r1, =__data_start
    ldr r2, =__data_end
.Lcopy_data:
    cmp r1, r2
    bcs .Lzero_bss
    ldr r3, [r0], #4
    str r3, [r1], #4
    b .Lcopy_data
.Lzero_bss:
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    movs r2, #0
.Lbss_loop:
    cmp r0, r1
    bcs .Lmain
    str r2, [r0], #4
    b .Lbss_loop
.Lmain:
    bl main
.Lhang:
    wfi
    b .Lhang
.size Reset_Handler, . - Reset_Handler

.thumb_func
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    b .
.size Default_Handler, . - Default_Handler
