This directory contains the ChibiOS bindings for the Eclipse ThreadX kernel:
https://github.com/eclipse-threadx/threadx

The bindings run ThreadX as a ChibiOS sandbox guest. The kernel and the
application run unprivileged inside a sandbox started by a ChibiOS SB host,
interrupts are replaced by the sandbox Virtual IRQs (VRQs) and peripherals
are accessed through the sandbox Virtual I/O (VIO).

Layout
------

- `threadx.mk`
  Kernel source list and include paths. The application configuration is
  in `tx_user.h`, it must be provided in the configuration directory.

- `portable/GCC/ARMvx-M-SB/`
  Port for ARMv7-M and ARMv8-M Mainline sandboxes, GCC only:
  - `port.mk`: port files, it also adds the common sandbox VRQ entry code.
  - `tx_port.h`, `tx_port.c`, `tx_port_asm.S`: the port.

Usage
-----

1. Unpack `ext/threadx-6.5.2.202603.7z` under `ext/`, the kernel is placed
   in `ext/threadx`. The archive contains the `common` directory of the
   ThreadX 6.5.2.202603 release and its license.
2. Start from a sandbox guest makefile, for example the one of
   `demos/various/SB-CLIENT-THREADX`, including:
   - `$(CHIBIOS)/os/common/startup/ARMCMx-SB/compilers/GCC/mk/startup.mk`
   - `$(CHIBIOS)/os/various/threadx_bindings/threadx.mk`
   - `$(CHIBIOS)/os/various/threadx_bindings/portable/GCC/ARMvx-M-SB/port.mk`
   - `$(CHIBIOS)/os/sb/user/sbuser.mk`
3. Provide `tx_user.h` in the configuration directory, the demo one is a
   starting point.
4. Call `tx_kernel_enter()` from `main()` and create the application
   objects in `tx_application_define()`.

Configuration
-------------

- `TX_TIMER_TICKS_PER_SECOND` must not exceed the host system time
  frequency returned by `sbGetFrequency()`, the tick period is rounded down
  to the host time resolution.
- `TX_MINIMUM_STACK` defaults to 512 bytes. VRQ handlers run on the stack
  of the interrupted thread, there is no separate interrupt stack. Thread
  stacks must accommodate the host VRQ frame, up to 108 bytes, and the
  deepest VRQ handler.
- Execution profiling and MISRA mode are not supported.

Scheduler and tick
------------------

- The scheduler runs on the stack of `main()`. Threads return to it when
  they suspend or are preempted, it then switches to the next thread to
  execute. While no thread is ready the CPU is released to the host until a
  VRQ is pending.
- Context switches are performed in user mode. A preemption requested by a
  VRQ handler is performed at the end of the handler.
- The tick is a periodic host alarm delivered on `TX_SB_VRQ_TICK` (VRQ 0).
  Ticks are accounted from the host system time, the ticks elapsed while
  the sandbox was not running are recovered on the next tick VRQ.
- The first unused memory passed to `tx_application_define()` is the start
  of the heap region, which is also used by the C library allocator, only
  one of them should use it.

VRQs
----

- `TX_SB_VRQ_HANDLER(n)` defines the handler of VRQ `n`, a decimal literal
  without suffixes or a macro expanding to one. Handlers run with VRQs
  disabled, they can use the ThreadX services allowed from ISRs.
- `tx_sb_vrq_enable()`, `tx_sb_vrq_disable()`: enable or disable a VRQ.
- `tx_sb_vrq_trigger()`: triggers a VRQ on the guest itself.
- `tx_sb_vrq_get_flags()`: returns and clears the flags set by the host
  for a VRQ, for example by VIO drivers.
- `tx_interrupt_control()` enables or disables all the VRQs.
- VRQ 0 is used by the port, the VRQs of VIO units are assigned by the host
  configuration.

FPU
---

- The port supports guests built for the FPU (`USE_FPU=hard`), the host
  must be built with FPU support too, else FPU instructions fault. With the
  multi-target host demos `make USE_FPU=hard` builds the host and the
  sandbox images for the FPU.
- In FPU builds the thread context includes S16-S31 and FPSCR, S0-S15 are
  saved in the VRQ frame by the host. All threads can use the FPU.

Limitations
-----------

- Tickless operation is not supported, the guest receives a VRQ on each
  tick.
- Blocking sandbox services, for example `sbSleepMilliseconds()` or
  blocking file operations, suspend the whole guest, not just the calling
  thread.
