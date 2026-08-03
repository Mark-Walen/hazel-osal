#ifndef HAZEL_SYS_INIT_H_
#define HAZEL_SYS_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*initcall_t)(void);

/*
 * Execute a single initcall.
 * Return value is implementation-defined.
 */
extern int do_one_initcall(initcall_t fn);
extern void do_initcalls(void);
extern void do_early_initcalls(void);
extern void do_core_initcalls(void);
extern void do_arch_initcalls(void);
extern void do_bus_initcalls(void);
extern void do_device_initcalls(void);
extern void do_subsys_initcalls(void);
extern void do_app_initcalls(void);
extern void do_late_initcalls(void);

/*
 * Linker-defined boundaries.
 *
 * Defined by linker script:
 *   __initcall_start
 *   __initcall_end
 *
 * And per-level boundaries:
 *   __initcall0_start ... __initcall7_start
 */
extern initcall_t __initcall_start[];
extern initcall_t __initcall0_start[];
extern initcall_t __initcall1_start[];
extern initcall_t __initcall2_start[];
extern initcall_t __initcall3_start[];
extern initcall_t __initcall4_start[];
extern initcall_t __initcall5_start[];
extern initcall_t __initcall6_start[];
extern initcall_t __initcall7_start[];
extern initcall_t __initcall_end[];

/*
 * Init level definition
 *
 * 0 early
 *      Earliest possible init.
 *      No dependency allowed.
 *
 * 1 core
 *      Core runtime:
 *      heap, irq, scheduler primitive, clock.
 *
 * 2 arch
 *      SOC / board-specific:
 *      pinmux, cache, MPU/MMU.
 *
 * 3 bus
 *      Hardware buses/controllers:
 *      UART/SPI/I2C/GPIO/DMA/TIMER.
 *
 * 4 device
 *      Device drivers:
 *      sensors, codec, display, flash.
 *
 * 5 subsys
 *      Middleware / subsystem:
 *      BLE, shell, AT, network.
 *
 * 6 app
 *      Application services.
 *
 * 7 late
 *      Final init.
 */

/*
 * Priority range:
 *
 * 0000 ~ 9999
 *
 * Smaller number runs earlier.
 *
 * Example:
 *
 * bus_initcall(uart0_init, 100);
 * bus_initcall(spi0_init, 200);
 *
 * subsys_initcall(shell_init, 8000);
 */

#define __INITCALL_PRIO_DEFAULT 5000

#define __INITCALL_SECTION(level, prio) \
    ".initcall" #level "." #prio ".init"

/*
 * Internal helper
 *
 * We use:
 *
 * .initcall3.0100.init
 *
 * linker lexicographic sorting ensures order.
 */
#define __define_initcall(fn, level, prio)                       \
    static const initcall_t __initcall_##fn##level##prio        \
    __attribute__((used))                                        \
    __attribute__((section(                                      \
        ".initcall" #level "." #prio ".init"))) = fn

/*
 * Init level APIs
 */

#define early_initcall(fn, prio) \
    __define_initcall(fn, 0, prio)

#define core_initcall(fn, prio) \
    __define_initcall(fn, 1, prio)

#define arch_initcall(fn, prio) \
    __define_initcall(fn, 2, prio)

#define bus_initcall(fn, prio) \
    __define_initcall(fn, 3, prio)

#define device_initcall(fn, prio) \
    __define_initcall(fn, 4, prio)

#define subsys_initcall(fn, prio) \
    __define_initcall(fn, 5, prio)

#define app_initcall(fn, prio) \
    __define_initcall(fn, 6, prio)

#define late_initcall(fn, prio) \
    __define_initcall(fn, 7, prio)

/*
 * Convenience APIs
 */

#define early_init(fn) \
    early_initcall(fn, __INITCALL_PRIO_DEFAULT)

#define core_init(fn) \
    core_initcall(fn, __INITCALL_PRIO_DEFAULT)

#define arch_init(fn) \
    arch_initcall(fn, __INITCALL_PRIO_DEFAULT)

#define bus_init(fn) \
    bus_initcall(fn, __INITCALL_PRIO_DEFAULT)

#define device_init(fn) \
    device_initcall(fn, __INITCALL_PRIO_DEFAULT)

#define subsys_init(fn) \
    subsys_initcall(fn, __INITCALL_PRIO_DEFAULT)

#define app_init(fn) \
    app_initcall(fn, __INITCALL_PRIO_DEFAULT)

#define late_init(fn) \
    late_initcall(fn, __INITCALL_PRIO_DEFAULT)

/*
 * Compatibility aliases
 */

#define platform_initcall(fn, prio) \
    arch_initcall(fn, prio)

#define module_initcall(fn, prio) \
    subsys_initcall(fn, prio)

/*
 * Legacy compatibility
 *
 * Default maps to device level.
 */
#define __initcall(fn) \
    device_init(fn)

#ifdef __cplusplus
}
#endif

#endif
