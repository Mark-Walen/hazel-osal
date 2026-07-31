# Lynx OSAL target tests

This repository builds the OS abstraction layer against the real FreeRTOS
kernel and runs it as bare-metal RV32 firmware on QEMU's `virt` machine. The
test is target-side: task scheduling, ticks, wait queues, mutexes, static and
dynamic task creation, and heap calls execute inside FreeRTOS rather than in a
host mock.

## WSL quick start

Use an Ubuntu/Debian WSL distribution and install the cross compiler and QEMU:

```sh
sudo apt update
sudo apt install cmake ninja-build gcc-riscv64-unknown-elf qemu-system-misc
```

From the repository (a `/mnt/<drive>/...` path is supported):

```sh
bash scripts/wsl-test.sh
```

Successful output ends with:

```text
OSAL QEMU RV32 / FreeRTOS
PASS: OSAL tests
```

For a manual build or debug session:

```sh
cmake --preset qemu-riscv32-freertos
cmake --build --preset qemu-riscv32-freertos
qemu-system-riscv32 -machine virt -smp 1 -nographic -bios none \
  -kernel build/qemu-riscv32-freertos/tests/osal_qemu_tests.elf
```

Add `-s -S` to QEMU and connect
`riscv64-unknown-elf-gdb` to `localhost:1234`.

## Portability layout

- `src/osal_<rtos>.c`: OSAL backend behavior.
- `src/third-party/CMakeLists.txt`: RTOS kernel and CPU portable-layer sources.
- `tests/platform/<platform>/<arch>`: startup, memory map, console, simulator
  exit, and `FreeRTOSConfig.h`.
- `cmake/toolchains`: compiler/ABI selection.

An ARM QEMU target therefore adds an ARM toolchain file and an ARM platform
directory, then selects the appropriate FreeRTOS portable layer. A second RTOS
adds its own backend and kernel target without changing target-side OSAL tests.

### Semaphore backend adapters

`struct os_sem` has common counting-semaphore semantics while embedding native
static storage selected by the OS adapter:

| Backend | Native storage | Implementation |
| --- | --- | --- |
| FreeRTOS | `StaticSemaphore_t` | `src/osal_sem_freertos.c` |
| RT-Thread | `struct rt_semaphore` | `src/osal_sem_rtthread.c` |
| Zephyr | `struct k_sem` | `src/osal_sem_zephyr.c` |

The common contract includes bounded counts, saturating `give`, non-blocking
`trytake`, tick-based timeouts, and ISR-safe zero-timeout operations. FreeRTOS
is exercised by the current QEMU target; RT-Thread and Zephyr adapters are
compiled when those kernels are added to the build matrix.
