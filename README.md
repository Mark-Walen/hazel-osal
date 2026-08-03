# Lynx OSAL target tests

This repository builds the OS abstraction layer against real FreeRTOS and
RT-Thread kernels and runs bare-metal RISC-V firmware on QEMU's `virt`
machine. The tests are target-side: task scheduling, ticks, wait queues,
mutexes, static and dynamic task creation, and heap calls execute inside the
selected RTOS rather than in a host mock.

## WSL quick start

Use an Ubuntu/Debian WSL distribution and install the cross compiler and QEMU:

```sh
sudo apt update
sudo apt install cmake ninja-build scons gcc-riscv64-unknown-elf qemu-system-misc
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

Run the same target-side suite on RT-Thread RV64 with:

```sh
bash scripts/wsl-test-rtthread.sh
```

This builds a minimal freestanding configuration around RT-Thread's
`qemu-virt64-riscv` BSP in a temporary directory. The submodule itself is not
modified.

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

- `src/common`: RTOS-independent message queue, FIFO, memory slab, signal,
  and work queue implementations built from core OSAL primitives.
- `src/osal_freertos`: FreeRTOS thread, wait queue, mutex, semaphore, event,
  memory, context, and time adapters.
- `src/osal_rtthread`: RT-Thread adapters for the same OSAL contract.
- `src/zephyr`: Zephyr adapters as they are added.
- `src/third-party/CMakeLists.txt`: RTOS kernel and CPU portable-layer sources.
- `tests/platform/<platform>/<arch>`: startup, memory map, console, simulator
  exit, and `FreeRTOSConfig.h`.
- `cmake/toolchains`: compiler/ABI selection.

An ARM QEMU target therefore adds an ARM toolchain file and an ARM platform
directory, then selects the appropriate FreeRTOS portable layer. A second RTOS
adds its own backend and kernel target without changing target-side OSAL tests.

## Kernel lifecycle

Call `os_kernel_init()` once before using the other OSAL APIs. Repeated calls
are allowed. It initializes backend-owned OSAL state only; board setup, the
native RTOS kernel, and scheduler startup remain platform responsibilities.

Threads have an explicit portable lifetime. `os_thread_join()` waits for the
entry function to return and then reclaims backend completion resources.
`os_thread_delete()` only reclaims a thread that has already returned; it does
not force-kill a running thread, because doing so could abandon an OSAL mutex
or corrupt a priority-inheritance chain.

Mutexes and semaphores have matching `os_mutex_deinit()` and
`os_sem_deinit()` calls. They must not be deinitialized while owned or while
another thread is waiting. The time API also provides monotonic
`os_uptime_get()` in milliseconds plus saturating, explicit-rounding
millisecond/tick conversion helpers.

## RT-Thread integration

RT-Thread is included at `src/third-party/rt-thread`. Its BSP remains
responsible for building the kernel, drivers, and CPU port; this project builds
`lynx_osal` against the selected BSP configuration. For example, the vendored
RISC-V QEMU BSP can be compile-checked with:

```sh
cmake -S . -B build/rtthread-api -G Ninja \
  -DOSAL_RTOS=rtthread \
  -DOSAL_BUILD_TESTS=OFF \
  -DRTTHREAD_CONFIG_DIR="$PWD/src/third-party/rt-thread/bsp/qemu-virt64-riscv" \
  -DRTTHREAD_CPU_INCLUDE_DIRS="$PWD/src/third-party/rt-thread/libcpu/risc-v/common64;$PWD/src/third-party/rt-thread/libcpu/risc-v/virt64"
cmake --build build/rtthread-api
```

For another architecture, point these cache variables at that BSP's
`rtconfig.h` directory and CPU-port include directories. The RT-Thread mutex
adapter uses native `rt_mutex`, including its priority-ordered waiters,
multi-mutex priority recomputation, dynamic waiter reprioritization, and
transitive priority inheritance.

### Semaphore backend adapters

`struct os_sem` has common counting-semaphore semantics while embedding native
static storage selected by the OS adapter:

| Backend | Native storage | Implementation |
| --- | --- | --- |
| FreeRTOS | `StaticSemaphore_t` | `src/osal_freertos/semaphore.c` |
| RT-Thread | `struct rt_semaphore` | `src/osal_rtthread/semaphore.c` |
| Zephyr | `struct k_sem` | `src/zephyr/semaphore.c` |

The common contract includes bounded counts, saturating `give`, non-blocking
`trytake`, tick-based timeouts, and ISR-safe zero-timeout operations. FreeRTOS
and RT-Thread are both exercised by target-side QEMU tests. A Zephyr runtime
target can be added to the build matrix independently.

## IPC and deferred work

The extended IPC API is statically allocated and does not require the heap:

| Primitive | Storage and semantics |
| --- | --- |
| `os_msgq` | Fixed-size messages copied into a caller-provided ring buffer |
| `os_fifo` | Zero-copy FIFO using caller-embedded `os_fifo_node` objects |
| `os_mem_slab` | Equal-size blocks carved from aligned caller memory |
| `os_event` | Native event group with wait-any/wait-all and optional clear |
| `os_signal` | Single pending notification carrying an integer result |
| `os_work_queue` | FIFO-driven OSAL worker thread with ordered execution |

Message queue put/get, FIFO put/get, slab alloc/free, signal raise, and work
submit support interrupt context when they are non-blocking. Event operations
are thread-context-only so FreeRTOS, RT-Thread, and Zephyr share one contract.
Raising an already-pending signal returns `-EBUSY`, preserving its original
result until a waiter consumes or resets it.
The portable event mask is 24 bits with 32-bit FreeRTOS ticks and 8 bits with
16-bit FreeRTOS ticks. FIFO depth is limited to 65535 pending nodes by the
portable semaphore contract.

Deinitialization requires that no thread is waiting. A memory slab additionally
requires every allocated block to have been returned, and a FIFO must be empty.
FreeRTOS RV32 and RT-Thread RV64 execute the same target-side tests for all of
these primitives.
