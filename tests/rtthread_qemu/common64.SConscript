import os

from building import *

source_root = os.environ['RTTHREAD_SOURCE_ROOT']
common64 = os.path.join(source_root, 'libcpu', 'risc-v', 'common64')
common = os.path.join(source_root, 'libcpu', 'risc-v', 'common')

src = [
    os.path.join(common64, name)
    for name in [
        'context_gcc.S',
        'cpuport.c',
        'cpuport_gcc.S',
        'interrupt_gcc.S',
        'sbi.c',
        'startup_gcc.S',
        'syscall_c.c',
        'tick.c',
        'trap.c',
    ]
]
src += [os.path.join(common, 'atomic_riscv.c')]

group = DefineGroup(
    'CPU',
    src,
    depend=[''],
    CPPPATH=[common64, common],
)

Return('group')
