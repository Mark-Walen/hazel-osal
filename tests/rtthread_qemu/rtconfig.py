import os

RTT_ROOT = os.getenv('RTT_ROOT')
ARCH = 'risc-v'
CPU = 'virt64'
CROSS_TOOL = 'gcc'
PLATFORM = 'gcc'
PREFIX = os.getenv('RTT_CC_PREFIX') or 'riscv64-unknown-elf-'
EXEC_PATH = os.getenv('RTT_EXEC_PATH') or '/usr/bin'

CC = PREFIX + 'gcc'
CXX = PREFIX + 'g++'
AS = PREFIX + 'gcc'
AR = PREFIX + 'ar'
LINK = PREFIX + 'gcc'
TARGET_EXT = 'elf'
SIZE = PREFIX + 'size'
OBJDUMP = PREFIX + 'objdump'
OBJCPY = PREFIX + 'objcopy'

DEVICE = ' -mcmodel=medany -march=rv64imac_zicsr -mabi=lp64 '
CFLAGS = (
    DEVICE
    + '-O0 -g -ffreestanding -fno-common '
    + '-ffunction-sections -fdata-sections'
)
AFLAGS = ' -c' + DEVICE + ' -x assembler-with-cpp -D__ASSEMBLY__ -g'
LFLAGS = (
    DEVICE
    + ' -nostdlib -Wl,--gc-sections,-Map=rtthread.map,-cref,-u,_start '
    + '-T link.lds -lgcc'
)
CXXFLAGS = CFLAGS
ARFLAGS = '-rc'
CPATH = ''
LPATH = ''
BUILD = 'debug'

POST_ACTION = (
    OBJCPY + ' -O binary $TARGET rtthread.bin\n'
    + SIZE + ' $TARGET\n'
)
