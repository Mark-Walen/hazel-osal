#include "platform.h"

.section .init
.global _start
.type _start, @function
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    csrr a0, mhartid
    bnez a0, .Lpark
    la sp, __stack_top

    la a0, __bss_start
    la a1, __bss_end
.Lbss:
    bgeu a0, a1, .Lmain
    sw zero, 0(a0)
    addi a0, a0, 4
    j .Lbss

.Lmain:
    call main
.Lpark:
    wfi
    j .Lpark
